#include "graphics/D3D9On12.h"

#include <d3d12.h>
#include <d3d9on12.h>
#include <dxgi1_4.h>

#include "core/Log.h"
#include "core/Strings.h"

namespace hca::graphics::on12 {

namespace {

using CreateDxgiFactory1Fn = HRESULT(WINAPI*)(REFIID, void**);

// Périphérique Direct3D 12 transmis à D3D9On12 : gardé pour toute la durée du jeu.
ID3D12Device* g_device12 = nullptr;

// Identifiants (fabricant, modèle) de l'adaptateur 0 de Direct3D 9, celui que le moteur passe à CreateDevice.
bool QueryD3D9Adapter(Direct3DCreate9Fn createOriginal, UINT sdkVersion, D3DADAPTER_IDENTIFIER9& identifier) {
    IDirect3D9* direct3d = createOriginal(sdkVersion);
    if (direct3d == nullptr) {
        return false;
    }
    const bool ok = SUCCEEDED(direct3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &identifier));
    direct3d->Release();
    return ok;
}

// Crée le périphérique Direct3D 12 sur la carte de l'adaptateur Direct3D 9 (même fabricant et modèle). Avec deux
// cartes (RTX + graphique intégré AMD sur la machine de test), D3D9On12 laissé à lui-même pourrait en choisir une
// autre que celle de l'écran : écran noir constaté le 10/10/2026, cause à confirmer.
ID3D12Device* CreateDeviceOn(const D3DADAPTER_IDENTIFIER9& wanted) {
    const HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
    const HMODULE d3d12 = LoadLibraryW(L"d3d12.dll");
    const auto createFactory =
        dxgi != nullptr ? reinterpret_cast<CreateDxgiFactory1Fn>(GetProcAddress(dxgi, "CreateDXGIFactory1")) : nullptr;
    const auto createDevice =
        d3d12 != nullptr ? reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(GetProcAddress(d3d12, "D3D12CreateDevice"))
                         : nullptr;
    if (createFactory == nullptr || createDevice == nullptr) {
        log::Warn("D3D9On12 : dxgi.dll ou d3d12.dll indisponible");
        return nullptr;
    }
    IDXGIFactory1* factory = nullptr;
    if (FAILED(createFactory(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
        return nullptr;
    }
    ID3D12Device* device = nullptr;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT index = 0; factory->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND; ++index) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        const bool match = desc.VendorId == wanted.VendorId && desc.DeviceId == wanted.DeviceId;
        log::Info("D3D9On12 : carte DXGI {} : {} (0x{:04X}:0x{:04X}){}", index, WideToUtf8(desc.Description),
                  desc.VendorId, desc.DeviceId, match ? " <- celle de Direct3D 9" : "");
        if (match && device == nullptr &&
            FAILED(createDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                                reinterpret_cast<void**>(&device)))) {
            log::Warn("D3D9On12 : création du périphérique Direct3D 12 impossible sur cette carte");
            device = nullptr;
        }
        adapter->Release();
    }
    factory->Release();
    return device;
}

}  // namespace

IDirect3D9* Create(UINT sdkVersion, Direct3DCreate9Fn createOriginal) noexcept {
    const HMODULE d3d9 = GetModuleHandleW(L"d3d9.dll");
    const auto create = d3d9 != nullptr ? reinterpret_cast<PFN_Direct3DCreate9On12>(
                                              GetProcAddress(d3d9, "Direct3DCreate9On12"))
                                        : nullptr;
    if (create == nullptr) {
        log::Warn("D3D9On12 : Direct3DCreate9On12 absent de d3d9.dll, Direct3D 9 habituel utilisé");
        return nullptr;
    }
    D3D9ON12_ARGS args{};
    args.Enable9On12 = TRUE;
    D3DADAPTER_IDENTIFIER9 identifier{};
    if (QueryD3D9Adapter(createOriginal, sdkVersion, identifier)) {
        log::Info("D3D9On12 : adaptateur Direct3D 9 : {} (0x{:04X}:0x{:04X})", identifier.Description,
                  identifier.VendorId, identifier.DeviceId);
        g_device12 = CreateDeviceOn(identifier);
    }
    // Sans périphérique fourni, la couche crée le sien (choix de carte laissé à Windows).
    args.pD3D12Device = g_device12;
    IDirect3D9* direct3d = create(sdkVersion, &args, 1);
    if (direct3d == nullptr) {
        log::Warn("D3D9On12 : création impossible, Direct3D 9 habituel utilisé");
        return nullptr;
    }
    log::Info("D3D9On12 actif : Direct3D 9 du jeu exécuté par Direct3D 12 (expérimental), périphérique {}",
              g_device12 != nullptr ? "créé sur la carte de Direct3D 9" : "choisi par Windows");
    return direct3d;
}

void LogDevice(IDirect3DDevice9* device) noexcept {
    if (device == nullptr) {
        return;
    }
    IDirect3DDevice9On12* on12 = nullptr;
    if (SUCCEEDED(device->QueryInterface(__uuidof(IDirect3DDevice9On12), reinterpret_cast<void**>(&on12))) &&
        on12 != nullptr) {
        log::Info("D3D9On12 : périphérique confirmé (IDirect3DDevice9On12 disponible)");
        on12->Release();
    } else {
        log::Warn("D3D9On12 : le périphérique n'expose pas IDirect3DDevice9On12");
    }
}

}  // namespace hca::graphics::on12
