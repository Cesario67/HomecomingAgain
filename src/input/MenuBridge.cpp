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
std::atomic<bool> g_connected{false};
std::atomic<std::uint64_t> g_lastSceneMs{0};
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
    // A et B ne sont plus traduits (Entrée, Échap) : la détection des menus par la scène 3D se trompe pendant les
    // écrans sans décor (constaté le 17/10/2026) et B ouvrait alors la pause. Start reste Entrée.
    keys[kKeyReturn] = (buttons & XINPUT_GAMEPAD_START) != 0;
    // Axe Y de XInput : positif vers le haut.
    StickAxis(stickY, kKeyDown, kKeyUp, stickHeld, keys);
    StickAxis(stickX, kKeyLeft, kKeyRight, stickHeld, keys);
    return keys;
}

void OnPadState(WORD buttons, SHORT stickX, SHORT stickY) noexcept {
    g_connected.store(true);
    g_buttons.store(buttons);
    g_stickX.store(stickX);
    g_stickY.store(stickY);
}

void OnPadDisconnected() noexcept {
    OnPadState(0, 0, 0);
    g_connected.store(false);
}

bool PadConnected() noexcept {
    return g_connected.load();
}

bool MenuActive(std::uint64_t nowMs, std::uint64_t lastSceneMs) noexcept {
    return lastSceneMs == 0 || nowMs < lastSceneMs || nowMs - lastSceneMs > kMenuDelayMs;
}

void OnSceneDrawn() noexcept {
    g_lastSceneMs.store(GetTickCount64());
}

void OnPresent() noexcept {
    const bool menu = InMenu();
    if (menu != g_loggedMenu.exchange(menu)) {
        log::Info("Manette : {}", menu ? "menu ou chargement (croix, stick, A et B traduits en touches du clavier)"
                                       : "en jeu (traduction en touches arrêtée)");
    }
}

bool InMenu() noexcept {
    // Seulement d'après la scène 3D. L'état d'interface du moteur (input/EngineUiState.h) ne convient pas : il vaut 2
    // en pleine partie comme dans les menus (journal du 16/10/2026), et A devenait Entrée, « Start », à chaque appui.
    return MenuActive(GetTickCount64(), g_lastSceneMs.load());
}

std::uint64_t SceneAgeMs() noexcept {
    const std::uint64_t last = g_lastSceneMs.load();
    return last == 0 ? 0 : GetTickCount64() - last;
}

KeySet CurrentKeys() noexcept {
    if (!GetConfig().menuPad || !InMenu()) {
        g_stickHeld = {};
        return {};
    }
    return KeysForPad(g_buttons.load(), g_stickX.load(), g_stickY.load(), g_stickHeld);
}

}  // namespace hca::input
