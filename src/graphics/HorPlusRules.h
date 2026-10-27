#pragma once

#include <cmath>

namespace hca::graphics::horplus {

// Pure rules of the Hor+ prototype (see HorPlus.h). Covered by tests/unit.

constexpr float kSixteenNine = 16.0f / 9.0f;

// Is the back buffer wider than 16:9 (by more than 1 %)? Hor+ does nothing otherwise.
constexpr bool IsWiderThanSixteenNine(long backWidth, long backHeight) noexcept {
    return backHeight > 0 && static_cast<float>(backWidth) > kSixteenNine * static_cast<float>(backHeight) * 1.01f;
}

// The engine sizes its drawing targets as exact 16:9 pictures from the width (verified 27/10/2026: 1920x1080 for a
// 1920x810 back buffer, 3840x2160 for 3840x1080). Small sizes are left alone (shadow maps, UI atlases).
constexpr bool IsSixteenNineTarget(long width, long height) noexcept {
    return width >= 640 && height * 16 == width * 9;
}

// Height of a 16:9 target of `width` once reshaped to the back buffer's ratio.
inline long ReshapedHeight(long width, long backWidth, long backHeight) noexcept {
    const long height = std::lround(static_cast<double>(width) * static_cast<double>(backHeight) /
                                    static_cast<double>(backWidth));
    return height < 1 ? 1 : height;
}

// Factor applied to the x output row of a projection so that a 16:9 picture covers a wider target without stretching:
// (16/9) / (backWidth / backHeight).
inline float ProjectionXFactor(long backWidth, long backHeight) noexcept {
    return kSixteenNine * static_cast<float>(backHeight) / static_cast<float>(backWidth);
}

// `rows` = 16 floats, one output row (x, y, z, w) per register, as in mWorldViewProj sent at c0 x4. Is it a projection
// made for a 16:9 picture? The y scale is 16/9 of the x scale (verified 27/10/2026 on the world camera: 1.6406 and
// 2.9163 at 1920x810 and at 1920x1080). The rows keep the scale whatever the camera orientation (norm of the 3x3 part).
inline bool IsSixteenNineProjection(const float* rows) noexcept {
    const float x = std::sqrt(rows[0] * rows[0] + rows[1] * rows[1] + rows[2] * rows[2]);
    const float y = std::sqrt(rows[4] * rows[4] + rows[5] * rows[5] + rows[6] * rows[6]);
    if (x < 1e-6f || y < 1e-6f) {
        return false;
    }
    const float ratio = y / x;
    return ratio > kSixteenNine * 0.99f && ratio < kSixteenNine * 1.01f;
}

// Multiplies the x output row (first four floats) by `factor`.
inline void ScaleProjectionX(float* rows, float factor) noexcept {
    for (int i = 0; i < 4; ++i) {
        rows[i] *= factor;
    }
}

}  // namespace hca::graphics::horplus
