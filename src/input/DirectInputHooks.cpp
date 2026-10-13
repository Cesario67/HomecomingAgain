#include "input/DirectInputHooks.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <atomic>
#include <cstring>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "input/MenuBridge.h"

namespace hca::input {

namespace {

constexpr size_t kCreateDeviceIndex = 3;     // IDirectInput8::CreateDevice
constexpr size_t kGetDeviceStateIndex = 9;   // IDirectInputDevice8::GetDeviceState
constexpr size_t kGetDeviceDataIndex = 10;   // IDirectInputDevice8::GetDeviceData

// GUID_SysKeyboard (dinput.h), recopié pour ne pas dépendre de dxguid.lib.
constexpr GUID kSysKeyboard = {0x6F1D2B61, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

using DirectInput8CreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
using CreateDeviceFn = HRESULT(STDMETHODCALLTYPE*)(IDirectInput8A*, REFGUID, LPDIRECTINPUTDEVICE8A*, LPUNKNOWN);
using GetDeviceStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8A*, DWORD, LPVOID);
using GetDeviceDataFn = HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8A*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD,
                                                     DWORD);

DirectInput8CreateFn g_directInput8Create = nullptr;
CreateDeviceFn g_createDevice = nullptr;
GetDeviceStateFn g_getDeviceState = nullptr;
GetDeviceDataFn g_getDeviceData = nullptr;
std::atomic<IDirectInputDevice8A*> g_keyboard{nullptr};
// Touches ajoutées lors de la dernière lecture en mémoire tampon, pour en produire les relâchements.
KeySet g_bufferedKeys{};
DWORD g_sequence = 0x40000000;  // numéros de séquence de nos événements, distincts de ceux de DirectInput
std::atomic<bool> g_loggedState{false};
std::atomic<bool> g_loggedData{false};

HRESULT STDMETHODCALLTYPE HookGetDeviceState(IDirectInputDevice8A* device, DWORD size, LPVOID data) {
    const HRESULT result = g_getDeviceState(device, size, data);
    if (device != g_keyboard.load() || data == nullptr || size < 256 || FAILED(result)) {
        return result;
    }
    if (!g_loggedState.exchange(true)) {
        log::Info("Manette : le moteur lit le clavier par GetDeviceState (état des touches)");
    }
    const KeySet keys = CurrentKeys();
    auto* state = static_cast<BYTE*>(data);
    for (size_t key = 0; key < keys.size(); ++key) {
        if (keys[key]) {
            state[key] |= 0x80;
        }
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookGetDeviceData(IDirectInputDevice8A* device, DWORD objectSize,
                                            LPDIDEVICEOBJECTDATA events, LPDWORD count, DWORD flags) {
    const DWORD capacity = count != nullptr ? *count : 0;
    const HRESULT result = g_getDeviceData(device, objectSize, events, count, flags);
    if (device != g_keyboard.load() || events == nullptr || count == nullptr || FAILED(result) ||
        objectSize < sizeof(DIDEVICEOBJECTDATA) || (flags & DIGDD_PEEK) != 0) {
        return result;
    }
    if (!g_loggedData.exchange(true)) {
        log::Info("Manette : le moteur lit le clavier par GetDeviceData (événements)");
    }
    // Appuis et relâchements de nos touches depuis la lecture précédente, ajoutés après ceux du vrai clavier.
    const KeySet keys = CurrentKeys();
    DWORD written = *count;
    for (size_t key = 0; key < keys.size() && written < capacity; ++key) {
        if (keys[key] == g_bufferedKeys[key]) {
            continue;
        }
        auto* event = reinterpret_cast<DIDEVICEOBJECTDATA*>(reinterpret_cast<BYTE*>(events) + written * objectSize);
        std::memset(event, 0, objectSize);
        event->dwOfs = static_cast<DWORD>(key);
        event->dwData = keys[key] ? 0x80 : 0x00;
        event->dwTimeStamp = GetTickCount();
        event->dwSequence = g_sequence++;
        g_bufferedKeys[key] = keys[key];
        ++written;
    }
    *count = written;
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateDevice(IDirectInput8A* input, REFGUID guid, LPDIRECTINPUTDEVICE8A* device,
                                           LPUNKNOWN outer) {
    const HRESULT result = g_createDevice(input, guid, device, outer);
    if (FAILED(result) || device == nullptr || *device == nullptr || !IsEqualGUID(guid, kSysKeyboard)) {
        return result;
    }
    g_keyboard.store(*device);
    // Une seule fois : la vtable est commune à tous les périphériques DirectInput (clavier et souris).
    bool ok = true;
    if (g_getDeviceState == nullptr) {
        ok &= hooking::PatchVtable(*device, kGetDeviceStateIndex, &HookGetDeviceState, &g_getDeviceState);
        ok &= hooking::PatchVtable(*device, kGetDeviceDataIndex, &HookGetDeviceData, &g_getDeviceData);
    }
    log::Info("Manette : clavier DirectInput du moteur intercepté ({})", ok ? "ok" : "échec");
    return result;
}

HRESULT WINAPI HookDirectInput8Create(HINSTANCE instance, DWORD version, REFIID iid, LPVOID* out, LPUNKNOWN outer) {
    const HRESULT result = g_directInput8Create(instance, version, iid, out, outer);
    if (SUCCEEDED(result) && out != nullptr && *out != nullptr && g_createDevice == nullptr) {
        hooking::PatchVtable(*out, kCreateDeviceIndex, &HookCreateDevice, &g_createDevice);
    }
    return result;
}

}  // namespace

void InstallDirectInputHooks(HMODULE engine) {
    if (!GetConfig().menuPad || engine == nullptr) {
        return;
    }
    const bool ok =
        hooking::PatchImport(engine, "DINPUT8.dll", "DirectInput8Create", &HookDirectInput8Create, &g_directInput8Create);
    log::Info("Manette : traduction en touches du clavier dans les menus {}", ok ? "prête" : "impossible (import absent)");
}

}  // namespace hca::input
