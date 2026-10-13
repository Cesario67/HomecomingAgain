#include "MotionCheck.h"

#include <cmath>
#include <cstdlib>

#include "BmpWriter.h"

namespace hca::dlss {

namespace {

int Luma(const std::uint8_t* bgra) noexcept {
    return (bgra[0] * 29 + bgra[1] * 150 + bgra[2] * 77) >> 8;
}

}  // namespace

float MeanWarpError(const std::uint8_t* current, const std::uint8_t* previous, const std::uint16_t* motion,
                    std::uint32_t width, std::uint32_t height, bool useMotion, float signX, float signY,
                    std::uint32_t step) noexcept {
    if (current == nullptr || previous == nullptr || motion == nullptr || step == 0) {
        return 0.0f;
    }
    double sum = 0.0;
    std::uint64_t count = 0;
    for (std::uint32_t y = 0; y < height; y += step) {
        for (std::uint32_t x = 0; x < width; x += step) {
            const std::size_t i = static_cast<std::size_t>(y) * width + x;
            float dx = 0.0f;
            float dy = 0.0f;
            if (useMotion) {
                dx = signX * HalfToFloat(motion[i * 2]);
                dy = signY * HalfToFloat(motion[i * 2 + 1]);
                if (!std::isfinite(dx) || !std::isfinite(dy)) {
                    continue;
                }
            }
            const long px = std::lround(static_cast<float>(x) + dx);
            const long py = std::lround(static_cast<float>(y) + dy);
            if (px < 0 || py < 0 || px >= static_cast<long>(width) || py >= static_cast<long>(height)) {
                continue;  // sorti de l'écran : pas de comparaison possible
            }
            const std::size_t j = static_cast<std::size_t>(py) * width + static_cast<std::size_t>(px);
            sum += std::abs(Luma(current + i * 4) - Luma(previous + j * 4));
            ++count;
        }
    }
    return count > 0 ? static_cast<float>(sum / static_cast<double>(count)) : 0.0f;
}

}  // namespace hca::dlss
