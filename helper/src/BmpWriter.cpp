#include "BmpWriter.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

namespace hca::dlss {

namespace {

bool WriteRows(const std::wstring& path, std::uint32_t width, std::uint32_t height,
               const std::vector<std::uint8_t>& rgbTopDown) {
    const std::uint32_t rowSize = (width * 3 + 3) & ~3u;
    BITMAPFILEHEADER file{};
    file.bfType = 0x4D42;  // « BM »
    file.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + rowSize * height;
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = static_cast<LONG>(width);
    info.biHeight = static_cast<LONG>(height);  // positif : lignes de bas en haut
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biSizeImage = rowSize * height;
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(&file), sizeof(file));
    out.write(reinterpret_cast<const char*>(&info), sizeof(info));
    std::vector<std::uint8_t> row(rowSize, 0);
    for (std::uint32_t y = height; y-- > 0;) {
        std::copy_n(rgbTopDown.begin() + static_cast<std::ptrdiff_t>(y) * width * 3, width * 3, row.begin());
        out.write(reinterpret_cast<const char*>(row.data()), rowSize);
    }
    return static_cast<bool>(out);
}

}  // namespace

bool WriteColorBmp(const std::wstring& path, const std::uint8_t* bgra, std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i) {
        rgb[i * 3 + 0] = bgra[i * 4 + 0];  // B (BMP : B, G, R)
        rgb[i * 3 + 1] = bgra[i * 4 + 1];
        rgb[i * 3 + 2] = bgra[i * 4 + 2];
    }
    return WriteRows(path, width, height, rgb);
}

bool WriteDepthBmp(const std::wstring& path, const float* depth, std::uint32_t width, std::uint32_t height) {
    const std::size_t count = static_cast<std::size_t>(width) * height;
    float low = 1e30f;
    float high = -1e30f;
    for (std::size_t i = 0; i < count; ++i) {
        if (depth[i] < 0.99999f) {  // 1 = ciel, ignoré pour l'étirement
            low = std::min(low, depth[i]);
            high = std::max(high, depth[i]);
        }
    }
    const float range = high > low ? high - low : 1.0f;
    std::vector<std::uint8_t> rgb(count * 3);
    for (std::size_t i = 0; i < count; ++i) {
        const float t = std::clamp((depth[i] - low) / range, 0.0f, 1.0f);
        const auto v = static_cast<std::uint8_t>(255.0f * (1.0f - t));
        rgb[i * 3 + 0] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = v;
    }
    return WriteRows(path, width, height, rgb);
}

float HalfToFloat(std::uint16_t half) noexcept {
    const std::uint32_t sign = (half >> 15) & 1u;
    const std::uint32_t exponent = (half >> 10) & 0x1Fu;
    const std::uint32_t mantissa = half & 0x3FFu;
    float value = 0.0f;
    if (exponent == 0) {
        value = std::ldexp(static_cast<float>(mantissa), -24);  // dénormalisé
    } else if (exponent == 31) {
        value = mantissa == 0 ? INFINITY : NAN;
    } else {
        value = std::ldexp(static_cast<float>(mantissa | 0x400u), static_cast<int>(exponent) - 25);
    }
    return sign != 0 ? -value : value;
}

bool WriteMotionBmp(const std::wstring& path, const std::uint16_t* halves, std::uint32_t width, std::uint32_t height,
                    float& averageLength, float& maxLength) {
    const std::size_t count = static_cast<std::size_t>(width) * height;
    std::vector<std::uint8_t> rgb(count * 3);
    double sum = 0.0;
    maxLength = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const float x = HalfToFloat(halves[i * 2]);
        const float y = HalfToFloat(halves[i * 2 + 1]);
        const float length = std::sqrt(x * x + y * y);
        if (std::isfinite(length)) {
            sum += length;
            maxLength = std::max(maxLength, length);
        }
        rgb[i * 3 + 0] = 128;  // B
        rgb[i * 3 + 1] = static_cast<std::uint8_t>(std::clamp(128.0f + y * 8.0f, 0.0f, 255.0f));  // G
        rgb[i * 3 + 2] = static_cast<std::uint8_t>(std::clamp(128.0f + x * 8.0f, 0.0f, 255.0f));  // R
    }
    averageLength = count > 0 ? static_cast<float>(sum / static_cast<double>(count)) : 0.0f;
    return WriteRows(path, width, height, rgb);
}

}  // namespace hca::dlss