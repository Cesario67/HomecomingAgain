#pragma once

#include <windows.h>
#include <Xinput.h>

namespace hca::input::qte {

// Buttons that answer a prompt: face buttons and shoulders (hypothesis: the prompt asks for one of these).
constexpr WORD kMashButtons = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y |
                              XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER;

constexpr int kMinRate = 4;
constexpr int kMaxRate = 15;

// Presses per second clamped to what the engine can follow at 30 to 60 frames per second.
int ClampRate(int pressesPerSecond) noexcept;

// While a mash prompt is active, turns buttons that are physically held into repeated presses: the held buttons of
// `kMashButtons` are released for the second half of every period (1000 / rate ms), so the engine sees `rate` presses
// per second. Outside a prompt, or for other buttons, `buttons` is returned unchanged.
WORD ApplyHold(WORD buttons, bool promptActive, DWORD nowMs, int pressesPerSecond) noexcept;

}  // namespace hca::input::qte
