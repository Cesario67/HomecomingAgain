#pragma once

#include <cmath>
#include <cwchar>

namespace hca::graphics::moviebars {

// Pure rules of the pre-rendered video fix (see MovieBars.h). Covered by tests/unit.

// Horizontal span of the 16:9 picture centred in a render target of targetWidth x targetHeight.
struct Span {
    long x = 0;
    long width = 0;
};

// Widest picture the player allows for the videos (`[Graphics] MovieAspect`): 16:9, 21:9 or 32:9. Anything else gives 16:9.
inline float ParseMovieAspect(const wchar_t* text) noexcept {
    if (text != nullptr) {
        if (std::wcscmp(text, L"21:9") == 0) {
            return 21.0f / 9.0f;
        }
        if (std::wcscmp(text, L"32:9") == 0) {
            return 32.0f / 9.0f;
        }
    }
    return 16.0f / 9.0f;
}

// Is the target wider than `maxAspect` (by more than 1 %)? If so, `span` receives the centred rectangle of that aspect
// (full height); the rest of the target stays black.
inline bool CentredPicture(long targetWidth, long targetHeight, float maxAspect, Span& span) noexcept {
    if (targetHeight <= 0 || targetWidth <= 0) {
        return false;
    }
    const long width = std::lround(static_cast<double>(targetHeight) * static_cast<double>(maxAspect));
    if (static_cast<double>(targetWidth) <= static_cast<double>(width) * 1.01) {
        return false;
    }
    span.width = width;
    span.x = (targetWidth - width) / 2;
    return true;
}

// Bink videos of the game are 1280x720 (checked on the .bik headers, 10/11/2026): the engine converts them from three
// L8 planes, luma 1280x720 then two chroma planes 640x360 (D3DFMT_L8 = 50, dynamic textures), see MovieBars.cpp.
constexpr unsigned kLumaWidth = 1280;
constexpr unsigned kLumaHeight = 720;
constexpr unsigned kChromaWidth = 640;
constexpr unsigned kChromaHeight = 360;

constexpr bool IsLumaPlane(unsigned width, unsigned height, unsigned format) noexcept {
    return width == kLumaWidth && height == kLumaHeight && format == 50;
}

constexpr bool IsChromaPlane(unsigned width, unsigned height, unsigned format) noexcept {
    return width == kChromaWidth && height == kChromaHeight && format == 50;
}

}  // namespace hca::graphics::moviebars
