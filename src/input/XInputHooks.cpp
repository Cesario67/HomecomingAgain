#include "input/XInputHooks.h"

#include <Xinput.h>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "input/MenuBridge.h"

namespace hca::input {

namespace {

constexpr const char* kXInputDll = "XINPUT1_3.dll";
constexpr WORD kGetStateOrdinal = 2;

using GetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

GetStateFn g_getState = nullptr;

DWORD WINAPI HookGetState(DWORD user, XINPUT_STATE* state) {
    const DWORD result = g_getState(user, state);
    // Première manette : son état sert à la traduction en touches dans les menus (input/MenuBridge.h).
    if (user == 0) {
        if (result == ERROR_SUCCESS && state != nullptr) {
            OnPadState(state->Gamepad.wButtons, state->Gamepad.sThumbLX, state->Gamepad.sThumbLY);
        } else {
            OnPadDisconnected();
        }
    }
    return result;
}

}  // namespace

void InstallXInputHooks(HMODULE engine) {
    if ((!GetConfig().menuPad && GetConfig().buttonIcons != 0) || engine == nullptr) {
        return;
    }
    const bool get = hooking::PatchImportOrdinal(engine, kXInputDll, kGetStateOrdinal, &HookGetState, &g_getState);
    log::Info("Manettes : lecture XInput du moteur {}", get ? "interceptée" : "non interceptée");
}

}  // namespace hca::input
