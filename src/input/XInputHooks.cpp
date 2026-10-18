#include "input/XInputHooks.h"

#include <Xinput.h>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "input/MenuBridge.h"
#include "input/SonyPad.h"
#include "patches/CameraFix.h"

namespace hca::input {

namespace {

constexpr const char* kXInputDll = "XINPUT1_3.dll";
constexpr WORD kGetStateOrdinal = 2;
constexpr WORD kGetCapabilitiesOrdinal = 4;

using GetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
using GetCapabilitiesFn = DWORD(WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*);

GetStateFn g_getState = nullptr;
GetCapabilitiesFn g_getCapabilities = nullptr;
bool g_playStationPad = false;
DWORD g_sonyPacket = 0;
XINPUT_GAMEPAD g_lastSony{};

// Sans manette XInput : la manette Sony (si elle répond) à sa place, pour la première manette seulement.
bool SubstituteSonyPad(XINPUT_STATE* state) noexcept {
    if (!g_playStationPad || state == nullptr) {
        return false;
    }
    StartSonyPadReader();
    XINPUT_GAMEPAD gamepad{};
    if (!ReadSonyPad(gamepad)) {
        return false;
    }
    // XInput change le numéro de paquet quand l'état change.
    if (gamepad.wButtons != g_lastSony.wButtons || gamepad.sThumbLX != g_lastSony.sThumbLX ||
        gamepad.sThumbLY != g_lastSony.sThumbLY || gamepad.sThumbRX != g_lastSony.sThumbRX ||
        gamepad.sThumbRY != g_lastSony.sThumbRY || gamepad.bLeftTrigger != g_lastSony.bLeftTrigger ||
        gamepad.bRightTrigger != g_lastSony.bRightTrigger) {
        ++g_sonyPacket;
        g_lastSony = gamepad;
    }
    state->dwPacketNumber = g_sonyPacket;
    state->Gamepad = gamepad;
    return true;
}

DWORD WINAPI HookGetState(DWORD user, XINPUT_STATE* state) {
    DWORD result = g_getState(user, state);
    // Première manette : son état sert à la traduction en touches dans les menus (input/MenuBridge.h).
    if (user == 0) {
        const bool sony = result != ERROR_SUCCESS && SubstituteSonyPad(state);
        if (sony) {
            result = ERROR_SUCCESS;
        }
        NoteSonyPadInUse(sony);
        if (result == ERROR_SUCCESS && state != nullptr) {
            OnPadState(state->Gamepad.wButtons, state->Gamepad.sThumbLX, state->Gamepad.sThumbLY);
            patches::NoteRightStick(state->Gamepad.sThumbRX, state->Gamepad.sThumbRY);
        } else {
            OnPadDisconnected();
            patches::NoteRightStick(0, 0);
        }
    }
    return result;
}

// Le moteur peut demander les caractéristiques de la manette : celles d'une manette Xbox 360 pour la manette Sony.
DWORD WINAPI HookGetCapabilities(DWORD user, DWORD flags, XINPUT_CAPABILITIES* capabilities) {
    const DWORD result = g_getCapabilities(user, flags, capabilities);
    if (user != 0 || result == ERROR_SUCCESS || capabilities == nullptr || !g_playStationPad) {
        return result;
    }
    StartSonyPadReader();
    XINPUT_GAMEPAD unused{};
    if (!ReadSonyPad(unused)) {
        return result;
    }
    *capabilities = XINPUT_CAPABILITIES{};
    capabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
    capabilities->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
    capabilities->Gamepad.wButtons = 0xF3FF;
    capabilities->Gamepad.bLeftTrigger = 0xFF;
    capabilities->Gamepad.bRightTrigger = 0xFF;
    capabilities->Gamepad.sThumbLX = capabilities->Gamepad.sThumbLY = static_cast<SHORT>(0xFFC0);
    capabilities->Gamepad.sThumbRX = capabilities->Gamepad.sThumbRY = static_cast<SHORT>(0xFFC0);
    return ERROR_SUCCESS;
}

}  // namespace

void InstallXInputHooks(HMODULE engine) {
    const Config& config = GetConfig();
    g_playStationPad = config.playStationPad;
    if ((!config.menuPad && config.buttonIcons != 0 && !config.cameraFix && !config.playStationPad) || engine == nullptr) {
        return;
    }
    const bool get = hooking::PatchImportOrdinal(engine, kXInputDll, kGetStateOrdinal, &HookGetState, &g_getState);
    // Lecture lancée tout de suite : le moteur peut chercher ses manettes dès son démarrage. CreateThread est permis
    // sous le verrou du chargeur ; le thread ne charge winmm.dll qu'une fois DllMain terminé.
    if (config.playStationPad) {
        StartSonyPadReader();
    }
    const bool caps = config.playStationPad &&
                      hooking::PatchImportOrdinal(engine, kXInputDll, kGetCapabilitiesOrdinal, &HookGetCapabilities,
                                                  &g_getCapabilities);
    log::Info("Manettes : lecture XInput du moteur {}, manette PlayStation {}", get ? "interceptée" : "non interceptée",
              !config.playStationPad ? "désactivée"
              : get && caps          ? "prise en charge sans manette XInput"
                                     : "non prise en charge (caractéristiques non interceptées)");
}

}  // namespace hca::input
