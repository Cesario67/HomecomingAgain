#pragma once

#include <windows.h>

#include <cstdint>

namespace hca::input {

// Pad presence and menu detection. The game's menus read the controller by themselves (verified 17/10/2026), so the
// former controller-to-keyboard translation for menus was removed; what remains is the detection of "no 3D scene is
// being drawn" (menus, loading screens), used by the input log (input/InputLog.h), and the connected state of the
// first pad, used by the button icons (input/ButtonIcons.h).

// Shared between the controller read (engine thread) and the other readers.
void OnPadConnected() noexcept;
void OnPadDisconnected() noexcept;
// First connected pad (last engine read).
bool PadConnected() noexcept;
// Menu detection, called by the Direct3D hooks at each end of 3D scene and at each Present. Menu before the first 3D
// scene, or after kMenuDelayMs without a 3D scene: main menu, long loading screen.
constexpr std::uint64_t kMenuDelayMs = 5000;
// Pure function (tested by tests/unit): `lastSceneMs` = 0 if no 3D scene has been drawn yet.
bool MenuActive(std::uint64_t nowMs, std::uint64_t lastSceneMs) noexcept;
void OnSceneDrawn() noexcept;
void OnPresent() noexcept;
bool InMenu() noexcept;
// DIAGNOSTIC TEMPORAIRE: milliseconds since the last 3D scene drawn (0: never).
std::uint64_t SceneAgeMs() noexcept;

}  // namespace hca::input
