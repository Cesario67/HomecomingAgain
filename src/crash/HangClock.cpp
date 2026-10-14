#include "crash/HangClock.h"

#include <algorithm>

namespace hca::crash {

std::int64_t HangElapsed(std::int64_t now, std::int64_t lastPresent, std::int64_t lastBackground) noexcept {
    const std::int64_t since = std::max(lastPresent, lastBackground);
    return now > since ? now - since : 0;
}

}  // namespace hca::crash
