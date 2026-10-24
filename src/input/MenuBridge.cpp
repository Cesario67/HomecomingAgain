#include "input/MenuBridge.h"

#include <atomic>

#include "core/Log.h"

namespace hca::input {

namespace {

std::atomic<bool> g_connected{false};
std::atomic<std::uint64_t> g_lastSceneMs{0};
std::atomic<bool> g_loggedMenu{false};

}  // namespace

void OnPadConnected() noexcept {
    g_connected.store(true);
}

void OnPadDisconnected() noexcept {
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
        log::Info("Manette : {}", menu ? "menu ou chargement (aucune scène 3D)" : "en jeu (scène 3D)");
    }
}

bool InMenu() noexcept {
    // Only from the 3D scene. The engine's UI state (input/EngineUiState.h) does not fit: it is 2 in the middle of a
    // game as well as in menus (log of 16/10/2026).
    return MenuActive(GetTickCount64(), g_lastSceneMs.load());
}

std::uint64_t SceneAgeMs() noexcept {
    const std::uint64_t last = g_lastSceneMs.load();
    return last == 0 ? 0 : GetTickCount64() - last;
}

}  // namespace hca::input
