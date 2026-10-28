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

// A bare projection (no camera rotation), as sent at c222 x4 in the 27/10/2026 captures: [sx 0 0 0] [0 sy 0 0] ..., with
// sy / sx = 16/9 (1.6407 and 2.9167). Verified in the captures only; whoever reads it in the engine is unknown.
inline bool IsBareSixteenNineProjection(const float* rows) noexcept {
    if (rows[1] != 0.0f || rows[2] != 0.0f || rows[3] != 0.0f || rows[4] != 0.0f || rows[6] != 0.0f ||
        rows[7] != 0.0f || rows[0] < 1e-6f || rows[5] < 1e-6f) {
        return false;
    }
    const float ratio = rows[5] / rows[0];
    return ratio > kSixteenNine * 0.99f && ratio < kSixteenNine * 1.01f;
}

// The inverse of those scales, sent as one pixel shader constant [1/sx, 1/sy, 0, 0] (c117 = 0.60951 0.34285 in the
// captures; the shader that reads it is not identified, hypothesis: view-space position rebuilt from the depth for
// the shadows). 1/sx over 1/sy is then 16/9 as well. Screen sizes in pixels also have that ratio (1920, 1080) or
// (1/1920, 1/1080): the range of the first value excludes them.
inline bool IsInverseProjectionScales(const float* v) noexcept {
    if (v[2] != 0.0f || v[3] != 0.0f || v[0] < 0.05f || v[0] > 4.0f || v[1] < 1e-6f) {
        return false;
    }
    const float ratio = v[0] / v[1];
    return ratio > kSixteenNine * 0.99f && ratio < kSixteenNine * 1.01f;
}

// Constants that carry the 16:9 target height, measured 27/10/2026 (1920x1080 values at c116 and c194, also at half
// resolution): the engine keeps sending them while Hor+ makes the targets 810 high.
//  - pixel shader c116 = [2/W, -2/H, -W/2, -H/2] (0.0010417 -0.0018519 -960 -540): window position to screen
//    coordinates. y scale / x scale = -16/9 and -H/2 over -W/2 = 9/16. Hypothesis: used with c117 to rebuild the
//    position from the depth (shadows).
//  - vertex shader c194 = [-1/W, 1/H, 0, 0] (-0.00052083 0.00092593): half-pixel offset of full-screen quads.
inline bool IsPixelToScreenConstant(const float* v) noexcept {
    if (v[0] <= 0.0f || v[1] >= 0.0f || v[2] >= 0.0f || v[3] >= 0.0f) {
        return false;
    }
    const float yOverX = -v[1] / v[0];
    const float heightOverWidth = v[3] / v[2];
    const float unit = v[0] * -v[2];
    return yOverX > kSixteenNine * 0.995f && yOverX < kSixteenNine * 1.005f && heightOverWidth > 0.5625f * 0.995f &&
           heightOverWidth < 0.5625f * 1.005f && unit > 0.99f && unit < 1.01f;
}

inline bool IsHalfPixelOffsetConstant(const float* v) noexcept {
    if (v[0] >= 0.0f || v[1] <= 0.0f || v[2] != 0.0f || v[3] != 0.0f || -v[0] < 1e-5f || -v[0] > 1e-2f) {
        return false;
    }
    const float yOverX = v[1] / -v[0];
    return yOverX > kSixteenNine * 0.995f && yOverX < kSixteenNine * 1.005f;
}

// Screen scale of the volumetric light pass (mScreenScale at c111 in ps_1C62CA10, captured 27/10/2026): [1/W, 1/H, 0, 0]
// (0.0005211 0.00092678 at 1920x1080, also seen at c108 with a denormal in z). The pass multiplies the window position
// by it to read the linear depth texture: with a target 810 high, the lookup must use 1/810 or it only covers the top
// of the depth texture and the rebuilt positions are wrong (black patches in the lit scene, verified by the flags
// test of 27/10/2026: they appear when the full-size targets are reshaped).
inline bool IsScreenScaleConstant(const float* v) noexcept {
    const auto tiny = [](float x) { return std::fabs(x) < 1e-30f; };
    if (v[0] < 1e-5f || v[0] > 1e-2f || v[1] < 1e-5f || v[1] > 1e-2f || !tiny(v[2]) || !tiny(v[3])) {
        return false;
    }
    const float yOverX = v[1] / v[0];
    return yOverX > kSixteenNine * 0.995f && yOverX < kSixteenNine * 1.005f;
}

// Height factor to apply to those constants (1080 -> 810 gives 4/3): the inverse of ProjectionXFactor.
inline float HeightFactor(long backWidth, long backHeight) noexcept {
    return 1.0f / ProjectionXFactor(backWidth, backHeight);
}

// Multiplies the x output row (first four floats) by `factor`.
inline void ScaleProjectionX(float* rows, float factor) noexcept {
    for (int i = 0; i < 4; ++i) {
        rows[i] *= factor;
    }
}

}  // namespace hca::graphics::horplus
