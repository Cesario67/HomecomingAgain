#include "input/QteHoldLogic.h"

#include <algorithm>

namespace hca::input::qte {

int ClampRate(int pressesPerSecond) noexcept {
    return std::clamp(pressesPerSecond, kMinRate, kMaxRate);
}

WORD ApplyHold(WORD buttons, bool promptActive, DWORD nowMs, int pressesPerSecond) noexcept {
    if (!promptActive || (buttons & kMashButtons) == 0) {
        return buttons;
    }
    const DWORD periodMs = 1000U / static_cast<DWORD>(ClampRate(pressesPerSecond));
    const bool released = (nowMs % periodMs) >= periodMs / 2;
    return released ? static_cast<WORD>(buttons & ~kMashButtons) : buttons;
}

}  // namespace hca::input::qte
