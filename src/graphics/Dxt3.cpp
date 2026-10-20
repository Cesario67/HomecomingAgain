#include "graphics/Dxt3.h"

#include <algorithm>
#include <array>

namespace hca::graphics {

namespace {

struct Rgb {
    int r, g, b;
};

std::uint16_t Pack565(const Rgb& c) noexcept {
    return static_cast<std::uint16_t>(((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3));
}

Rgb Unpack565(std::uint16_t v) noexcept {
    const int r = (v >> 11) & 31;
    const int g = (v >> 5) & 63;
    const int b = v & 31;
    return {(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)};
}

int Distance2(const Rgb& a, const Rgb& b) noexcept {
    const int dr = a.r - b.r;
    const int dg = a.g - b.g;
    const int db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

}  // namespace

std::size_t Dxt3Size(int width, int height) noexcept {
    return static_cast<std::size_t>(width / 4) * static_cast<std::size_t>(height / 4) * 16;
}

void EncodeDxt3(const std::uint32_t* argb, int width, int height, std::uint8_t* out) noexcept {
    for (int by = 0; by < height / 4; ++by) {
        for (int bx = 0; bx < width / 4; ++bx) {
            std::array<Rgb, 16> pixels{};
            std::array<int, 16> alpha{};
            for (int i = 0; i < 16; ++i) {
                const std::uint32_t p = argb[(by * 4 + i / 4) * width + bx * 4 + i % 4];
                alpha[i] = static_cast<int>(p >> 24);
                pixels[i] = {static_cast<int>((p >> 16) & 255), static_cast<int>((p >> 8) & 255),
                             static_cast<int>(p & 255)};
            }
            // Points extrêmes : la paire de pixels (non transparents) la plus éloignée.
            int first = -1;
            int second = -1;
            int best = -1;
            for (int i = 0; i < 16; ++i) {
                if (alpha[i] == 0) {
                    continue;
                }
                for (int j = i; j < 16; ++j) {
                    if (alpha[j] == 0) {
                        continue;
                    }
                    const int d = Distance2(pixels[i], pixels[j]);
                    if (d > best) {
                        best = d;
                        first = i;
                        second = j;
                    }
                }
            }
            std::uint16_t c0 = 0;
            std::uint16_t c1 = 0;
            if (first >= 0) {
                c0 = Pack565(pixels[first]);
                c1 = Pack565(pixels[second]);
            }
            const Rgb e0 = Unpack565(c0);
            const Rgb e1 = Unpack565(c1);
            const std::array<Rgb, 4> palette = {
                e0, e1, Rgb{(2 * e0.r + e1.r) / 3, (2 * e0.g + e1.g) / 3, (2 * e0.b + e1.b) / 3},
                Rgb{(e0.r + 2 * e1.r) / 3, (e0.g + 2 * e1.g) / 3, (e0.b + 2 * e1.b) / 3}};
            std::uint32_t indices = 0;
            std::uint64_t alphaBits = 0;
            for (int i = 0; i < 16; ++i) {
                int nearest = 0;
                for (int k = 1; k < 4; ++k) {
                    if (Distance2(pixels[i], palette[k]) < Distance2(pixels[i], palette[nearest])) {
                        nearest = k;
                    }
                }
                indices |= static_cast<std::uint32_t>(nearest) << (2 * i);
                alphaBits |= static_cast<std::uint64_t>((alpha[i] + 8) / 17) << (4 * i);
            }
            std::uint8_t* block = out + (static_cast<std::size_t>(by) * (width / 4) + bx) * 16;
            for (int i = 0; i < 8; ++i) {
                block[i] = static_cast<std::uint8_t>(alphaBits >> (8 * i));
            }
            block[8] = static_cast<std::uint8_t>(c0);
            block[9] = static_cast<std::uint8_t>(c0 >> 8);
            block[10] = static_cast<std::uint8_t>(c1);
            block[11] = static_cast<std::uint8_t>(c1 >> 8);
            for (int i = 0; i < 4; ++i) {
                block[12 + i] = static_cast<std::uint8_t>(indices >> (8 * i));
            }
        }
    }
}

void DecodeDxt3(const std::uint8_t* data, int width, int height, std::uint32_t* argb) noexcept {
    for (int by = 0; by < height / 4; ++by) {
        for (int bx = 0; bx < width / 4; ++bx) {
            const std::uint8_t* block = data + (static_cast<std::size_t>(by) * (width / 4) + bx) * 16;
            std::uint64_t alphaBits = 0;
            for (int i = 0; i < 8; ++i) {
                alphaBits |= static_cast<std::uint64_t>(block[i]) << (8 * i);
            }
            const Rgb e0 = Unpack565(static_cast<std::uint16_t>(block[8] | (block[9] << 8)));
            const Rgb e1 = Unpack565(static_cast<std::uint16_t>(block[10] | (block[11] << 8)));
            const std::array<Rgb, 4> palette = {
                e0, e1, Rgb{(2 * e0.r + e1.r) / 3, (2 * e0.g + e1.g) / 3, (2 * e0.b + e1.b) / 3},
                Rgb{(e0.r + 2 * e1.r) / 3, (e0.g + 2 * e1.g) / 3, (e0.b + 2 * e1.b) / 3}};
            std::uint32_t indices = 0;
            for (int i = 0; i < 4; ++i) {
                indices |= static_cast<std::uint32_t>(block[12 + i]) << (8 * i);
            }
            for (int i = 0; i < 16; ++i) {
                const Rgb c = palette[(indices >> (2 * i)) & 3];
                const std::uint32_t a = static_cast<std::uint32_t>((alphaBits >> (4 * i)) & 15) * 17;
                argb[(by * 4 + i / 4) * width + bx * 4 + i % 4] =
                    (a << 24) | (static_cast<std::uint32_t>(c.r) << 16) | (static_cast<std::uint32_t>(c.g) << 8) |
                    static_cast<std::uint32_t>(c.b);
            }
        }
    }
}

}  // namespace hca::graphics
