#include "input/MenuBridge.h"

#include <Xinput.h>

#include <atomic>

#include "core/Config.h"
#include "core/Log.h"

namespace hca::input {

namespace {

constexpr SHORT kPressThreshold = 16384;   // moitié de la course du stick
constexpr SHORT kReleaseThreshold = 10923;  // un tiers

std::atomic<WORD> g_buttons{0};
std::atomic<SHORT> g_stickX{0};
std::atomic<SHORT> g_stickY{0};
std::atomic<int> g_framesSinceScene{kMenuFrames + 1};
std::atomic<bool> g_loggedMenu{false};
// Directions du stick déjà enfoncées (lecture du clavier, un seul thread du moteur).
KeySet g_stickHeld{};

void StickAxis(SHORT value, std::uint8_t negativeKey, std::uint8_t positiveKey, KeySet& held, KeySet& keys) {
    const bool negative = held[negativeKey] ? value < -kReleaseThreshold : value < -kPressThreshold;
    const bool positive = held[positiveKey] ? value > kReleaseThreshold : value > kPressThreshold;
    held[negativeKey] = negative;
    held[positiveKey] = positive;
    keys[negativeKey] = keys[negativeKey] || negative;
    keys[positiveKey] = keys[positiveKey] || positive;
}

}  // namespace

KeySet KeysForPad(WORD buttons, SHORT stickX, SHORT stickY, KeySet& stickHeld) noexcept {
    KeySet keys{};
    keys[kKeyUp] = (buttons & XINPUT_GAMEPAD_DPAD_UP) != 0;
    keys[kKeyDown] = (buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    keys[kKeyLeft] = (buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    keys[kKeyRight] = (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
    keys[kKeyReturn] = (buttons & (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_START)) != 0;
    keys[kKeyEscape] = (buttons & XINPUT_GAMEPAD_B) != 0;
    // Axe Y de XInput : positif vers le haut.
    StickAxis(stickY, kKeyDown, kKeyUp, stickHeld, keys);
    StickAxis(stickX, kKeyLeft, kKeyRight, stickHeld, keys);
    return keys;
}

void OnPadState(WORD buttons, SHORT stickX, SHORT stickY) noexcept {
    g_buttons.store(buttons);
    g_stickX.store(stickX);
    g_stickY.store(stickY);
}

void OnPadDisconnected() noexcept {
    OnPadState(0, 0, 0);
}

void OnSceneDrawn() noexcept {
    g_framesSinceScene.store(0);
}

void OnPresent() noexcept {
    const int frames = g_framesSinceScene.load();
    if (frames <= kMenuFrames) {
        g_framesSinceScene.store(frames + 1);
    }
    const bool menu = InMenu();
    if (menu != g_loggedMenu.exchange(menu)) {
        log::Info("Manette : {}", menu ? "menu ou chargement (croix, stick, A et B traduits en touches du clavier)"
                                       : "en jeu (traduction en touches arrêtée)");
    }
}

bool InMenu() noexcept {
    return g_framesSinceScene.load() > kMenuFrames;
}

KeySet CurrentKeys() noexcept {
    if (!GetConfig().menuPad || !InMenu()) {
        g_stickHeld = {};
        return {};
    }
    return KeysForPad(g_buttons.load(), g_stickX.load(), g_stickY.load(), g_stickHeld);
}

}  // namespace hca::input
