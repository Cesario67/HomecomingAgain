#pragma once

#include <cmath>

namespace hca::graphics::clipcopy {

struct Rect {
    long left = 0;
    long top = 0;
    long right = 0;
    long bottom = 0;
};

// Direct3D 9 refuses a StretchRect whose destination rectangle leaves the destination surface. The engine centres
// its 16:9 picture on a back buffer of another ratio and asks for a destination that overflows it (verified
// 27/10/2026: source 1920x1080 to a 1920x810 back buffer with the rectangle (0,-135,1920,945), answer 0x8876086C).
//
// Pure rule: clips `dest` to the surface (0, 0, surfaceWidth, surfaceHeight) and moves the edges of `source` by the
// same proportion, so every pixel keeps the position it would have had. Returns false when nothing is left to copy
// or when no clipping is needed (`source` and `dest` are then left untouched).
inline bool ClipToSurface(Rect& source, Rect& dest, long surfaceWidth, long surfaceHeight) noexcept {
    const long destWidth = dest.right - dest.left;
    const long destHeight = dest.bottom - dest.top;
    if (destWidth <= 0 || destHeight <= 0 || surfaceWidth <= 0 || surfaceHeight <= 0) {
        return false;
    }
    if (dest.left >= 0 && dest.top >= 0 && dest.right <= surfaceWidth && dest.bottom <= surfaceHeight) {
        return false;
    }
    const long left = dest.left < 0 ? 0 : dest.left;
    const long top = dest.top < 0 ? 0 : dest.top;
    const long right = dest.right > surfaceWidth ? surfaceWidth : dest.right;
    const long bottom = dest.bottom > surfaceHeight ? surfaceHeight : dest.bottom;
    if (right <= left || bottom <= top) {
        return false;
    }
    const double scaleX = static_cast<double>(source.right - source.left) / static_cast<double>(destWidth);
    const double scaleY = static_cast<double>(source.bottom - source.top) / static_cast<double>(destHeight);
    const long newLeft = source.left + std::lround(static_cast<double>(left - dest.left) * scaleX);
    const long newTop = source.top + std::lround(static_cast<double>(top - dest.top) * scaleY);
    const long newRight = source.right - std::lround(static_cast<double>(dest.right - right) * scaleX);
    const long newBottom = source.bottom - std::lround(static_cast<double>(dest.bottom - bottom) * scaleY);
    if (newRight <= newLeft || newBottom <= newTop) {
        return false;
    }
    source = {newLeft, newTop, newRight, newBottom};
    dest = {left, top, right, bottom};
    return true;
}

}  // namespace hca::graphics::clipcopy
