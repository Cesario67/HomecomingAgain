#include "graphics/D3D9Hooks.h"

#include <d3d9.h>

#include <atomic>
#include <cstdint>

#include "compat/WindowThreadLock.h"
#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "core/Strings.h"
#include "core/SystemInfo.h"
#include "crash/CrashHandler.h"
#include "crash/HangWatchdog.h"
#include "dlss/DxvkInterop.h"
#include "dlss/SharedFrames.h"
#include "framepacing/FrameMonitor.h"
#include "graphics/BackBufferDump.h"
#include "graphics/BorderlessWindow.h"
#include "graphics/CameraTracker.h"
#include "graphics/Jitter.h"
#include "input/ButtonIcons.h"
#include "input/InputLog.h"
#include "input/MenuBridge.h"
#include "graphics/D3D9On12.h"
#include "graphics/FrameCapture.h"
#include "graphics/InternalDraw.h"
#include "graphics/MotionVectors.h"
#include "graphics/SceneHooks.h"
#include "graphics/UiClip.h"
#include "graphics/Ssao.h"
#include "graphics/Smaa.h"
#include "graphics/FlashIcons.h"

namespace hca::graphics {

namespace {

using Direct3DCreate9Fn = IDirect3D9*(WINAPI*)(UINT);
using CreateDeviceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*,
                                                   IDirect3DDevice9**);
using ResetFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using SetSamplerStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD);

// Indices dans les vtables COM (ordre de déclaration de d3d9.h, IUnknown compris).
constexpr size_t kCreateDeviceIndex = 16;  // IDirect3D9::CreateDevice
constexpr size_t kResetIndex = 16;         // IDirect3DDevice9::Reset
constexpr size_t kPresentIndex = 17;       // IDirect3DDevice9::Present
constexpr size_t kSetSamplerStateIndex = 69;  // IDirect3DDevice9::SetSamplerState

Direct3DCreate9Fn g_direct3DCreate9 = nullptr;
CreateDeviceFn g_createDevice = nullptr;
ResetFn g_reset = nullptr;
PresentFn g_present = nullptr;
SetSamplerStateFn g_setSamplerState = nullptr;
Smaa g_smaa;
std::atomic<bool> g_lateInitDone{false};
HRESULT g_lastPresentError = S_OK;
std::uint32_t g_resetFailures = 0;  // échecs consécutifs de Reset

void LogPresentParameters(const char* context, const D3DPRESENT_PARAMETERS& params) {
    log::Info("{} : {}x{} format {} x{} tampons, MSAA {}, swap {}, fenêtré {}, {} Hz, intervalle 0x{:X}, options 0x{:X}, fenêtre {}",
              context, params.BackBufferWidth, params.BackBufferHeight, static_cast<int>(params.BackBufferFormat),
              params.BackBufferCount, static_cast<int>(params.MultiSampleType), static_cast<int>(params.SwapEffect),
              params.Windowed ? "oui" : "non", params.FullScreen_RefreshRateInHz, params.PresentationInterval,
              params.Flags, static_cast<const void*>(params.hDeviceWindow));
}

HRESULT STDMETHODCALLTYPE HookPresent(IDirect3DDevice9* device, const RECT* sourceRect, const RECT* destRect,
                                      HWND window, const RGNDATA* dirtyRegion) {
    if (GetConfig().frameCapture) {
        capture::OnPresent(device);
    }
    // Image déjà anticrénelée par le DLSS : pas de SMAA par-dessus (double anticrénelage, flou).
    if (GetConfig().smaa && !dlss::ConsumeFrameReturned()) {
        InternalDrawScope internal;
        g_smaa.Apply(device);
    }
    if (dlss::Available()) {
        dlss::Probe(GetSsao().SceneTexture(), GetSsao().DepthTexture());
    }
    GetSsao().OnPresent();
    camera::OnPresent();
    jitter::OnPresent();
    input::OnPresent();
    input::RefreshButtonIcons();
    input::LogEngineState();
    if (GetConfig().backBufferDump) {
        InternalDrawScope internal;
        dump::OnPresent(device);
    }
    frames::OnBeforePresent();
    const HRESULT result = g_present(device, sourceRect, destRect, window, dirtyRegion);
    if (frames::OnAfterPresent()) {
        crash::EnsureInstalled();
    }
    // Journaliser seulement les changements d'état (un périphérique perdu échoue à chaque image).
    if (result != g_lastPresentError) {
        if (FAILED(result)) {
            log::Warn("Present échoue : 0x{:08X}", static_cast<unsigned long>(result));
        } else if (FAILED(g_lastPresentError)) {
            log::Info("Present fonctionne de nouveau");
        }
        g_lastPresentError = result;
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
    // Un jeu qui retente Reset en boucle tant qu'il échoue : on ne journalise que le premier échec,
    // un échec sur 100, et le retour à la normale.
    if (params != nullptr && g_resetFailures == 0) {
        LogPresentParameters("Reset du périphérique", *params);
    }
    // Nos ressources D3DPOOL_DEFAULT doivent être libérées avant Reset, sinon il échoue.
    g_smaa.OnBeforeReset();
    GetSsao().OnBeforeReset();
    GetMotionVectors().OnBeforeReset();
    const HRESULT result = g_reset(device, params);
    if (SUCCEEDED(result) && params != nullptr && params->Windowed && GetConfig().borderless) {
        // Un Reset (changement de mode) peut rendre son cadre à la fenêtre.
        D3DDEVICE_CREATION_PARAMETERS creation{};
        if (SUCCEEDED(device->GetCreationParameters(&creation))) {
            ApplyBorderless(params->hDeviceWindow != nullptr ? params->hDeviceWindow : creation.hFocusWindow);
        }
    }
    if (SUCCEEDED(result)) {
        if (g_resetFailures > 0) {
            log::Info("Reset réussi après {} échec(s)", g_resetFailures);
        } else {
            log::Info("Reset -> 0x{:08X}", static_cast<unsigned long>(result));
        }
        g_resetFailures = 0;
    } else {
        ++g_resetFailures;
        if (g_resetFailures == 1 || g_resetFailures % 100 == 0) {
            log::Warn("Reset -> 0x{:08X} ({} échec(s) consécutif(s))", static_cast<unsigned long>(result),
                      g_resetFailures);
        }
    }
    frames::OnDeviceReset();
    return result;
}

// Filtrage anisotrope forcé (repris de l'option filteringOverride de DPfix, en plus prudent) : seul le filtrage
// LINEAR devient ANISOTROPIC ; le filtrage POINT (interface, effets plein écran, ombres) est laissé tel quel.
HRESULT STDMETHODCALLTYPE HookSetSamplerState(IDirect3DDevice9* device, DWORD sampler, D3DSAMPLERSTATETYPE type,
                                              DWORD value) {
    if (!t_internalDraw && (type == D3DSAMP_MINFILTER || type == D3DSAMP_MAGFILTER) && value == D3DTEXF_LINEAR) {
        g_setSamplerState(device, sampler, D3DSAMP_MAXANISOTROPY, static_cast<DWORD>(GetConfig().anisotropy));
        return g_setSamplerState(device, sampler, type, D3DTEXF_ANISOTROPIC);
    }
    if (!t_internalDraw && type == D3DSAMP_MAXANISOTROPY) {
        value = static_cast<DWORD>(GetConfig().anisotropy);
    }
    return g_setSamplerState(device, sampler, type, value);
}

void PatchDevice(IDirect3DDevice9* device) {
    // Toutes les instances d'une même classe partagent la vtable : une seule modification suffit.
    // Si un objet d'une autre classe apparaît, on ne l'intercepte pas (l'original mémorisé serait faux).
    if (g_present != nullptr) {
        void** vtable = *reinterpret_cast<void***>(device);
        if (vtable[kPresentIndex] != reinterpret_cast<void*>(&HookPresent)) {
            log::Warn("Nouveau périphérique avec une vtable différente : non intercepté");
        }
        return;
    }
    if (!hooking::PatchVtable(device, kPresentIndex, &HookPresent, &g_present) ||
        !hooking::PatchVtable(device, kResetIndex, &HookReset, &g_reset)) {
        log::Error("Interception de Present/Reset impossible : pas de mesure de cadence");
        return;
    }
    log::Info("Present intercepté (implémentation : {})",
              WideToUtf8(sysinfo::ModulePathOf(reinterpret_cast<const void*>(g_present))));
    if (GetConfig().anisotropy > 1) {
        if (hooking::PatchVtable(device, kSetSamplerStateIndex, &HookSetSamplerState, &g_setSamplerState)) {
            log::Info("Filtrage anisotrope forcé : x{}", GetConfig().anisotropy);
        } else {
            log::Warn("Filtrage anisotrope : interception de SetSamplerState impossible");
        }
    }
    scene::Install(device);
    uiclip::Install(device);
    flashicons::Install(device);
    if (GetConfig().frameCapture) {
        capture::Install(device);
    }
    log::Info("SMAA : {}", GetConfig().smaa ? "activé (appliqué à chaque image avant Present)" : "désactivé");
}

HRESULT STDMETHODCALLTYPE HookCreateDevice(IDirect3D9* direct3d, UINT adapter, D3DDEVTYPE deviceType, HWND window,
                                           DWORD behaviorFlags, D3DPRESENT_PARAMETERS* params,
                                           IDirect3DDevice9** device) {
    if (params != nullptr) {
        LogPresentParameters("CreateDevice demandé", *params);
        // En mode fenêtré, la fenêtre existe déjà : on la rend sans bordure avant la création du périphérique.
        if (params->Windowed && GetConfig().borderless) {
            ApplyBorderless(params->hDeviceWindow != nullptr ? params->hDeviceWindow : window);
        }
    }
    // Le moteur crée le périphérique sans D3DCREATE_MULTITHREADED (comportement 0x40) mais l'utilise depuis deux
    // threads pendant les chargements (écran de chargement + thread principal). Constaté le 10/10/2026 : les deux
    // threads bloqués dans le pilote NVIDIA, chacun dans un appel Direct3D. Le drapeau fait sérialiser les appels
    // par Direct3D.
    if (GetConfig().d3dMultithreaded && (behaviorFlags & D3DCREATE_MULTITHREADED) == 0) {
        behaviorFlags |= D3DCREATE_MULTITHREADED;
        log::Info("D3DCREATE_MULTITHREADED ajouté : appels Direct3D des threads de chargement et principal sérialisés");
    }
    const HRESULT result = g_createDevice(direct3d, adapter, deviceType, window, behaviorFlags, params, device);
    log::Info("CreateDevice (adaptateur {}, type {}, comportement 0x{:08X}) -> 0x{:08X}", adapter,
              static_cast<int>(deviceType), behaviorFlags, static_cast<unsigned long>(result));
    if (SUCCEEDED(result) && device != nullptr && *device != nullptr) {
        if (GetConfig().d3d9On12) {
            on12::LogDevice(*device);
        }
        if (dlss::OnDeviceCreated(*device)) {
            log::Info("Périphérique DXVK détecté");
        }
        try {
            PatchDevice(*device);
        } catch (...) {
        }
    }
    return result;
}

// Premier appel du jeu à Direct3D : le chargement des DLL est terminé, on peut faire ce qui est
// interdit dans DllMain (LoadLibrary...).
void LateInit() {
    if (g_lateInitDone.exchange(true)) {
        return;
    }
    frames::LateInit();
    crash::PreloadDbgHelp();
    // Direct3DCreate9 est appelé par le thread principal, celui qui possède la fenêtre du jeu.
    crash::StartHangWatchdog(GetCurrentThreadId());
    compat::SetWindowThread(GetCurrentThreadId());
    // L'overlay Steam est injecté avant Direct3DCreate9 : utile pour relier un blocage à sa présence.
    log::Info("Overlay Steam : {}", GetModuleHandleW(L"gameoverlayrenderer.dll") != nullptr ? "chargé" : "absent");
    // Pendant le chargement (~20 s), le thread principal charge sans traiter les messages de la fenêtre, et un
    // autre thread affiche l'écran de chargement. En fenêtré, un clic pendant ce temps fait déclarer le jeu « ne
    // répond pas » par Windows au bout de 5 s (fenêtre fantôme), puis fermer (« Application Hang », constaté le
    // 10/10/2026 à 15 s du lancement). Sans effet en plein écran exclusif, où Windows ne crée pas de fantôme.
    DisableProcessWindowsGhosting();
    log::Info("Fenêtre fantôme de Windows désactivée (chargement sans traitement des messages)");

    // L'IAT peut pointer vers un shim de compatibilité de Windows (apphelp.dll) plutôt que vers d3d9.dll.
    const std::wstring caller = sysinfo::ModulePathOf(reinterpret_cast<const void*>(g_direct3DCreate9));
    const HMODULE d3d9 = GetModuleHandleW(L"d3d9.dll");
    const std::wstring provider = d3d9 != nullptr ? sysinfo::ModulePathOf(d3d9) : std::wstring(L"(non chargé)");
    log::Info("d3d9.dll chargé : {}", WideToUtf8(provider));
    if (!EqualsIgnoreCase(caller, provider)) {
        log::Info("Direct3DCreate9 du jeu passe d'abord par : {} (shim de compatibilité Windows ou autre "
                  "intercepteur)",
                  WideToUtf8(caller));
    }
}

IDirect3D9* WINAPI HookDirect3DCreate9(UINT sdkVersion) {
    try {
        LateInit();
    } catch (...) {
    }
    IDirect3D9* direct3d = GetConfig().d3d9On12 ? on12::Create(sdkVersion, g_direct3DCreate9) : nullptr;
    if (direct3d == nullptr) {
        direct3d = g_direct3DCreate9(sdkVersion);
    }
    log::Info("Direct3DCreate9({}) -> {}", sdkVersion, direct3d != nullptr ? "ok" : "échec");
    if (direct3d != nullptr && g_createDevice == nullptr) {
        if (!hooking::PatchVtable(direct3d, kCreateDeviceIndex, &HookCreateDevice, &g_createDevice)) {
            log::Error("Interception de CreateDevice impossible");
        }
    }
    return direct3d;
}

}  // namespace

void Install(HMODULE engineModule) {
    if (!hooking::PatchImport(engineModule, "d3d9.dll", "Direct3DCreate9", &HookDirect3DCreate9, &g_direct3DCreate9)) {
        log::Error("Interception de Direct3DCreate9 impossible : pas de mesure de cadence");
    }
}

}  // namespace hca::graphics
