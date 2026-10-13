#include "input/XInputProbe.h"

#include <Xinput.h>

#include <algorithm>
#include <cmath>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "input/MenuBridge.h"

namespace hca::input {

namespace {

constexpr const char* kXInputDll = "XINPUT1_3.dll";
constexpr WORD kGetStateOrdinal = 2;
constexpr WORD kSetStateOrdinal = 3;
constexpr WORD kGetCapabilitiesOrdinal = 4;
constexpr ULONGLONG kReportIntervalMs = 10000;
constexpr int kMaxButtonLogs = 40;

using GetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
using SetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);
using GetCapabilitiesFn = DWORD(WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*);

GetStateFn g_getState = nullptr;
SetStateFn g_setState = nullptr;
GetCapabilitiesFn g_getCapabilities = nullptr;

// Statistiques par manette (index 0 à 3), touchées par les threads du moteur qui lisent les manettes.
struct PadStats {
    bool known = false;      // déjà interrogée
    bool connected = false;  // dernier résultat
    WORD lastButtons = 0;
    DWORD calls = 0;
    float leftMax = 0.0f;
    float rightMax = 0.0f;
    BYTE triggerMax = 0;
    DWORD vibrations = 0;
    WORD lastLeftMotor = 0;
    WORD lastRightMotor = 0;
};

PadStats g_pads[XUSER_MAX_COUNT];
int g_buttonLogsLeft = kMaxButtonLogs;
ULONGLONG g_nextReport = 0;
CRITICAL_SECTION g_lock;

const char* TypeName(BYTE subType) {
    switch (subType) {
        case XINPUT_DEVSUBTYPE_GAMEPAD:
            return "manette";
        default:
            return "autre";
    }
}

void LogConnection(DWORD user, bool connected) {
    if (!connected) {
        log::Info("Manette {} : déconnectée (ou absente)", user);
        return;
    }
    XINPUT_CAPABILITIES caps{};
    const DWORD result = g_getCapabilities != nullptr ? g_getCapabilities(user, XINPUT_FLAG_GAMEPAD, &caps)
                                                      : ERROR_DEVICE_NOT_CONNECTED;
    if (result == ERROR_SUCCESS) {
        log::Info("Manette {} : connectée, type {} (sous-type {}), options 0x{:X}, vibration {}", user,
                  TypeName(caps.SubType), caps.SubType, caps.Flags,
                  caps.Vibration.wLeftMotorSpeed != 0 || caps.Vibration.wRightMotorSpeed != 0 ? "oui" : "non");
    } else {
        log::Info("Manette {} : connectée (capacités illisibles, erreur {})", user, result);
    }
}

void Report() {
    const ULONGLONG now = GetTickCount64();
    if (g_nextReport == 0) {
        g_nextReport = now + kReportIntervalMs;
        return;
    }
    if (now < g_nextReport) {
        return;
    }
    g_nextReport = now + kReportIntervalMs;
    for (DWORD user = 0; user < XUSER_MAX_COUNT; ++user) {
        PadStats& pad = g_pads[user];
        if (!pad.known) {
            continue;
        }
        log::Info("Manette {} : {} lectures en 10 s, {}, sticks jusqu'à {:.0f} % (gauche) et {:.0f} % (droit), "
                  "gâchettes jusqu'à {}, {} demande(s) de vibration (dernière {} / {})",
                  user, pad.calls, pad.connected ? "connectée" : "absente", pad.leftMax * 100.0f,
                  pad.rightMax * 100.0f, pad.triggerMax, pad.vibrations, pad.lastLeftMotor, pad.lastRightMotor);
        pad.calls = 0;
        pad.leftMax = 0.0f;
        pad.rightMax = 0.0f;
        pad.triggerMax = 0;
        pad.vibrations = 0;
    }
}

DWORD WINAPI HookGetState(DWORD user, XINPUT_STATE* state) {
    const DWORD result = g_getState(user, state);
    if (user >= XUSER_MAX_COUNT) {
        return result;
    }
    // Première manette : son état sert à la traduction en touches dans les menus (input/MenuBridge.h).
    if (user == 0) {
        if (result == ERROR_SUCCESS && state != nullptr) {
            OnPadState(state->Gamepad.wButtons, state->Gamepad.sThumbLX, state->Gamepad.sThumbLY);
        } else {
            OnPadDisconnected();
        }
    }
    if (!GetConfig().xinputLog) {
        return result;
    }
    EnterCriticalSection(&g_lock);
    PadStats& pad = g_pads[user];
    const bool connected = result == ERROR_SUCCESS && state != nullptr;
    if (!pad.known || pad.connected != connected) {
        if (!pad.known) {
            log::Info("Manette {} : première lecture par le moteur", user);
        }
        pad.known = true;
        pad.connected = connected;
        LogConnection(user, connected);
    }
    ++pad.calls;
    if (connected) {
        const XINPUT_GAMEPAD& g = state->Gamepad;
        pad.leftMax = std::max(pad.leftMax, StickAmount(g.sThumbLX, g.sThumbLY));
        pad.rightMax = std::max(pad.rightMax, StickAmount(g.sThumbRX, g.sThumbRY));
        pad.triggerMax = std::max({pad.triggerMax, g.bLeftTrigger, g.bRightTrigger});
        if (g.wButtons != pad.lastButtons && g_buttonLogsLeft > 0) {
            --g_buttonLogsLeft;
            log::Info("Manette {} : boutons {}", user, DescribeButtons(g.wButtons));
        }
        pad.lastButtons = g.wButtons;
    }
    Report();
    LeaveCriticalSection(&g_lock);
    return result;
}

DWORD WINAPI HookSetState(DWORD user, XINPUT_VIBRATION* vibration) {
    if (user < XUSER_MAX_COUNT && vibration != nullptr) {
        EnterCriticalSection(&g_lock);
        ++g_pads[user].vibrations;
        g_pads[user].lastLeftMotor = vibration->wLeftMotorSpeed;
        g_pads[user].lastRightMotor = vibration->wRightMotorSpeed;
        LeaveCriticalSection(&g_lock);
    }
    return g_setState(user, vibration);
}

DWORD WINAPI HookGetCapabilities(DWORD user, DWORD flags, XINPUT_CAPABILITIES* capabilities) {
    const DWORD result = g_getCapabilities(user, flags, capabilities);
    static int logsLeft = 8;
    if (logsLeft > 0) {
        --logsLeft;
        log::Info("Manette {} : capacités demandées par le moteur (options 0x{:X}) -> {}", user, flags,
                  result == ERROR_SUCCESS ? "ok" : std::to_string(result));
    }
    return result;
}

}  // namespace

std::string DescribeButtons(WORD buttons) {
    static constexpr struct {
        WORD mask;
        const char* name;
    } kNames[] = {
        {XINPUT_GAMEPAD_A, "A"},
        {XINPUT_GAMEPAD_B, "B"},
        {XINPUT_GAMEPAD_X, "X"},
        {XINPUT_GAMEPAD_Y, "Y"},
        {XINPUT_GAMEPAD_LEFT_SHOULDER, "LB"},
        {XINPUT_GAMEPAD_RIGHT_SHOULDER, "RB"},
        {XINPUT_GAMEPAD_BACK, "Back"},
        {XINPUT_GAMEPAD_START, "Start"},
        {XINPUT_GAMEPAD_LEFT_THUMB, "L3"},
        {XINPUT_GAMEPAD_RIGHT_THUMB, "R3"},
        {XINPUT_GAMEPAD_DPAD_UP, "Haut"},
        {XINPUT_GAMEPAD_DPAD_DOWN, "Bas"},
        {XINPUT_GAMEPAD_DPAD_LEFT, "Gauche"},
        {XINPUT_GAMEPAD_DPAD_RIGHT, "Droite"},
    };
    std::string text;
    for (const auto& entry : kNames) {
        if ((buttons & entry.mask) != 0) {
            text += text.empty() ? "" : " ";
            text += entry.name;
        }
    }
    return text.empty() ? "aucun" : text;
}

float StickAmount(SHORT x, SHORT y) noexcept {
    const float fx = static_cast<float>(x) / 32767.0f;
    const float fy = static_cast<float>(y) / 32767.0f;
    return std::min(1.0f, std::sqrt(fx * fx + fy * fy));
}

void InstallXInputProbe(HMODULE engine) {
    if ((!GetConfig().xinputLog && !GetConfig().menuPad && GetConfig().buttonIcons != 0) || engine == nullptr) {
        return;
    }
    InitializeCriticalSection(&g_lock);
    const bool get = hooking::PatchImportOrdinal(engine, kXInputDll, kGetStateOrdinal, &HookGetState, &g_getState);
    const bool set = hooking::PatchImportOrdinal(engine, kXInputDll, kSetStateOrdinal, &HookSetState, &g_setState);
    const bool caps = hooking::PatchImportOrdinal(engine, kXInputDll, kGetCapabilitiesOrdinal, &HookGetCapabilities,
                                                  &g_getCapabilities);
    log::Info("Manettes : interception XInput du moteur (lecture {}, vibration {}, capacités {})", get ? "oui" : "non",
              set ? "oui" : "non", caps ? "oui" : "non");
}

}  // namespace hca::input
