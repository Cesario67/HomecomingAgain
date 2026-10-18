#include "input/SonyPad.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

#include "core/Log.h"

namespace hca::input {

namespace {

constexpr DWORD kAxisMax = 65535;

// Boutons Sony (base 0) placés sur chaque bouton XInput.
struct ButtonMap {
    int sony;
    WORD xinput;
};
constexpr std::array<ButtonMap, 10> kButtons = {{
    {1, XINPUT_GAMEPAD_A},              // Croix
    {2, XINPUT_GAMEPAD_B},              // Rond
    {0, XINPUT_GAMEPAD_X},              // Carré
    {3, XINPUT_GAMEPAD_Y},              // Triangle
    {4, XINPUT_GAMEPAD_LEFT_SHOULDER},  // L1
    {5, XINPUT_GAMEPAD_RIGHT_SHOULDER}, // R1
    {8, XINPUT_GAMEPAD_BACK},           // Create (Share sur DualShock 4)
    {9, XINPUT_GAMEPAD_START},          // Options
    {10, XINPUT_GAMEPAD_LEFT_THUMB},    // L3
    {11, XINPUT_GAMEPAD_RIGHT_THUMB},   // R3
}};

// Lecture en arrière-plan : joyGetPosEx peut bloquer ~64 ms (réénumération par WinMM, mesuré dans DPStabilityFix).
constexpr DWORD kPollIntervalMs = 4;
constexpr DWORD kScanIntervalMs = 3000;

using JoyGetPosExFn = MMRESULT(WINAPI*)(UINT, LPJOYINFOEX);
using JoyGetDevCapsWFn = MMRESULT(WINAPI*)(UINT_PTR, LPJOYCAPSW, UINT);
using JoyGetNumDevsFn = UINT(WINAPI*)();

SRWLOCK g_lock = SRWLOCK_INIT;
XINPUT_GAMEPAD g_latest{};
bool g_present = false;
std::atomic<bool> g_started{false};
std::atomic<bool> g_inUse{false};

// Valeur de [minimum, maximum] ramenée sur [0, 65535].
DWORD Normalize(DWORD value, DWORD minimum, DWORD maximum) noexcept {
    if (maximum <= minimum) {
        return std::min(value, kAxisMax);
    }
    const DWORD clamped = std::clamp(value, minimum, maximum);
    return static_cast<DWORD>((static_cast<std::uint64_t>(clamped - minimum) * kAxisMax) / (maximum - minimum));
}

// 0 à 65535 vers -32768 à 32767 ; `invert` pour les axes verticaux (WinMM : bas = grand, XInput : haut = positif).
SHORT Thumb(DWORD normalized, bool invert) noexcept {
    const long value = invert ? 32767L - static_cast<long>(normalized) : static_cast<long>(normalized) - 32768L;
    return static_cast<SHORT>(std::clamp(value, -32768L, 32767L));
}

BYTE Trigger(DWORD normalized) noexcept {
    return static_cast<BYTE>((normalized * 255u) / kAxisMax);
}

// Croix directionnelle depuis le POV (centièmes de degré, sens horaire depuis le haut ; diagonales comprises).
WORD DpadFromPov(DWORD pov) noexcept {
    if (pov > 35999) {
        return 0;  // JOY_POVCENTERED (0xFFFF) ou valeur invalide
    }
    WORD buttons = 0;
    if (pov >= 31500 || pov <= 4500) {
        buttons |= XINPUT_GAMEPAD_DPAD_UP;
    }
    if (pov >= 4500 && pov <= 13500) {
        buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
    }
    if (pov >= 13500 && pov <= 22500) {
        buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
    }
    if (pov >= 22500 && pov <= 31500) {
        buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
    }
    return buttons;
}

void Publish(bool present, const XINPUT_GAMEPAD& gamepad) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    g_present = present;
    g_latest = present ? gamepad : XINPUT_GAMEPAD{};
    ReleaseSRWLockExclusive(&g_lock);
}

DWORD WINAPI ReaderThread(LPVOID) {
    const HMODULE winmm = LoadLibraryW(L"winmm.dll");
    const auto getPos = winmm ? reinterpret_cast<JoyGetPosExFn>(GetProcAddress(winmm, "joyGetPosEx")) : nullptr;
    const auto getCaps = winmm ? reinterpret_cast<JoyGetDevCapsWFn>(GetProcAddress(winmm, "joyGetDevCapsW")) : nullptr;
    const auto getNum = winmm ? reinterpret_cast<JoyGetNumDevsFn>(GetProcAddress(winmm, "joyGetNumDevs")) : nullptr;
    if (getPos == nullptr || getCaps == nullptr || getNum == nullptr) {
        log::Warn("Manette PlayStation : WinMM indisponible, lecture désactivée");
        return 0;
    }
    UINT device = UINT_MAX;
    AxisRanges ranges{};
    bool announced = false;
    for (;;) {
        if (device == UINT_MAX) {
            const UINT count = std::min(getNum(), 16u);
            for (UINT id = 0; id < count && device == UINT_MAX; ++id) {
                JOYCAPSW caps{};
                if (getCaps(id, &caps, sizeof(caps)) == JOYERR_NOERROR && caps.wMid == kSonyVendorId) {
                    JOYINFOEX probe{sizeof(JOYINFOEX), JOY_RETURNALL};
                    if (getPos(id, &probe) == JOYERR_NOERROR) {
                        device = id;
                        ranges = RangesFromCaps(caps);
                        if (!announced) {
                            log::Info("Manette PlayStation trouvée (WinMM {}, produit 0x{:04X}) : présentée au jeu comme "
                                      "une manette Xbox 360 en l'absence de manette XInput", id, caps.wPid);
                            announced = true;
                        }
                    }
                }
            }
            if (device == UINT_MAX) {
                Sleep(kScanIntervalMs);
                continue;
            }
        }
        JOYINFOEX state{sizeof(JOYINFOEX), JOY_RETURNALL};
        if (getPos(device, &state) == JOYERR_NOERROR) {
            Publish(true, SonyToXInput(state, ranges));
            Sleep(kPollIntervalMs);
        } else {
            Publish(false, {});
            device = UINT_MAX;
            log::Info("Manette PlayStation débranchée");
            announced = false;
            Sleep(kScanIntervalMs);
        }
    }
}

}  // namespace

AxisRanges RangesFromCaps(const JOYCAPSW& caps) noexcept {
    return {caps.wXmin, caps.wXmax, caps.wYmin, caps.wYmax, caps.wZmin, caps.wZmax,
            caps.wRmin, caps.wRmax, caps.wUmin, caps.wUmax, caps.wVmin, caps.wVmax};
}

XINPUT_GAMEPAD SonyToXInput(const JOYINFOEX& raw, const AxisRanges& ranges) noexcept {
    XINPUT_GAMEPAD gamepad{};
    gamepad.sThumbLX = Thumb(Normalize(raw.dwXpos, ranges.xMin, ranges.xMax), false);
    gamepad.sThumbLY = Thumb(Normalize(raw.dwYpos, ranges.yMin, ranges.yMax), true);
    gamepad.sThumbRX = Thumb(Normalize(raw.dwZpos, ranges.zMin, ranges.zMax), false);
    gamepad.sThumbRY = Thumb(Normalize(raw.dwRpos, ranges.rMin, ranges.rMax), true);
    gamepad.bLeftTrigger = Trigger(Normalize(raw.dwVpos, ranges.vMin, ranges.vMax));
    gamepad.bRightTrigger = Trigger(Normalize(raw.dwUpos, ranges.uMin, ranges.uMax));
    WORD buttons = DpadFromPov(raw.dwPOV);
    for (const ButtonMap& button : kButtons) {
        if ((raw.dwButtons & (1u << button.sony)) != 0) {
            buttons |= button.xinput;
        }
    }
    gamepad.wButtons = buttons;
    return gamepad;
}

void StartSonyPadReader() noexcept {
    if (g_started.exchange(true)) {
        return;
    }
    const HANDLE thread = CreateThread(nullptr, 0, &ReaderThread, nullptr, 0, nullptr);
    if (thread == nullptr) {
        log::Warn("Manette PlayStation : thread de lecture non créé");
        return;
    }
    CloseHandle(thread);
}

bool ReadSonyPad(XINPUT_GAMEPAD& gamepad) noexcept {
    AcquireSRWLockShared(&g_lock);
    const bool present = g_present;
    gamepad = g_latest;
    ReleaseSRWLockShared(&g_lock);
    return present;
}

void NoteSonyPadInUse(bool inUse) noexcept {
    g_inUse.store(inUse, std::memory_order_relaxed);
}

bool SonyPadInUse() noexcept {
    return g_inUse.load(std::memory_order_relaxed);
}

}  // namespace hca::input
