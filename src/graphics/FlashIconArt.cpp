#include "graphics/FlashIconArt.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace hca::graphics {

namespace {

constexpr float kPi = 3.14159265f;
constexpr int kSamples = 4;  // sous-échantillons par axe et par pixel

struct Segment {
    float ax, ay, bx, by;
};
using Glyph = std::vector<Segment>;

void Line(Glyph& g, float ax, float ay, float bx, float by) {
    g.push_back({ax, ay, bx, by});
}

// Arc d'ellipse (angle 0 = +x, pi/2 = +y vers le bas) découpé en petits segments.
void Arc(Glyph& g, float cx, float cy, float rx, float ry, float a0, float a1, int steps) {
    for (int i = 0; i < steps; ++i) {
        const float t0 = a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(steps);
        const float t1 = a0 + (a1 - a0) * static_cast<float>(i + 1) / static_cast<float>(steps);
        Line(g, cx + rx * std::cos(t0), cy + ry * std::sin(t0), cx + rx * std::cos(t1), cy + ry * std::sin(t1));
    }
}

Glyph BuildGlyph(int type, int button) {
    Glyph g;
    if (type == 1) {
        switch (button) {
            case 0:  // A
                Line(g, -0.42f, 0.5f, 0.0f, -0.5f);
                Line(g, 0.0f, -0.5f, 0.42f, 0.5f);
                Line(g, -0.22f, 0.14f, 0.22f, 0.14f);
                break;
            case 1:  // B
                Line(g, -0.28f, -0.5f, -0.28f, 0.5f);
                Line(g, -0.28f, -0.5f, -0.02f, -0.5f);
                Arc(g, -0.02f, -0.25f, 0.24f, 0.25f, -kPi / 2, kPi / 2, 8);
                Line(g, -0.02f, 0.0f, -0.28f, 0.0f);
                Line(g, -0.28f, 0.0f, 0.02f, 0.0f);
                Arc(g, 0.02f, 0.25f, 0.27f, 0.25f, -kPi / 2, kPi / 2, 8);
                Line(g, 0.02f, 0.5f, -0.28f, 0.5f);
                break;
            case 2:  // X
                Line(g, -0.4f, -0.46f, 0.4f, 0.46f);
                Line(g, 0.4f, -0.46f, -0.4f, 0.46f);
                break;
            default:  // Y
                Line(g, -0.42f, -0.5f, 0.0f, 0.0f);
                Line(g, 0.42f, -0.5f, 0.0f, 0.0f);
                Line(g, 0.0f, 0.0f, 0.0f, 0.52f);
                break;
        }
        return g;
    }
    switch (button) {
        case 0:  // croix
            Line(g, -0.4f, -0.4f, 0.4f, 0.4f);
            Line(g, 0.4f, -0.4f, -0.4f, 0.4f);
            break;
        case 1:  // rond
            Arc(g, 0.0f, 0.0f, 0.42f, 0.42f, 0.0f, 2 * kPi, 28);
            break;
        case 2:  // carré
            Line(g, -0.36f, -0.36f, 0.36f, -0.36f);
            Line(g, 0.36f, -0.36f, 0.36f, 0.36f);
            Line(g, 0.36f, 0.36f, -0.36f, 0.36f);
            Line(g, -0.36f, 0.36f, -0.36f, -0.36f);
            break;
        default:  // triangle
            Line(g, 0.0f, -0.46f, 0.42f, 0.32f);
            Line(g, 0.42f, 0.32f, -0.42f, 0.32f);
            Line(g, -0.42f, 0.32f, 0.0f, -0.46f);
            break;
    }
    return g;
}

float DistanceToSegment(float px, float py, const Segment& s) {
    const float dx = s.bx - s.ax;
    const float dy = s.by - s.ay;
    const float lengthSquared = dx * dx + dy * dy;
    float t = lengthSquared > 0.0f ? ((px - s.ax) * dx + (py - s.ay) * dy) / lengthSquared : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const float cx = s.ax + t * dx - px;
    const float cy = s.ay + t * dy - py;
    return std::sqrt(cx * cx + cy * cy);
}

struct Color {
    float r, g, b;
};

Color Mix(const Color& a, const Color& b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

Color Scale(const Color& c, float k) {
    return {c.r * k, c.g * k, c.b * k};
}

Color XboxBase(int button) {
    switch (button) {
        case 0: return {96.0f, 176.0f, 40.0f};
        case 1: return {214.0f, 52.0f, 36.0f};
        case 2: return {40.0f, 116.0f, 204.0f};
        default: return {236.0f, 184.0f, 20.0f};
    }
}

Color Ps3Symbol(int button) {
    switch (button) {
        case 0: return {120.0f, 156.0f, 224.0f};
        case 1: return {232.0f, 96.0f, 96.0f};
        case 2: return {222.0f, 140.0f, 190.0f};
        default: return {84.0f, 200.0f, 150.0f};
    }
}

// Couleur et opacité en un point (x, y) du disque, coordonnées locales de -1 à 1.
bool ShadeDisc(int type, int button, const Glyph& glyph, float x, float y, Color& color) {
    const float radius = std::sqrt(x * x + y * y);
    if (radius > 1.0f) {
        return false;
    }
    const float top = 0.5f - 0.5f * y;  // 1 en haut, 0 en bas
    float distance = 10.0f;
    for (const Segment& s : glyph) {
        distance = std::min(distance, DistanceToSegment(x, y, s));
    }
    if (type == 1) {
        if (radius > 0.88f) {
            color = {28.0f, 28.0f, 30.0f};
            return true;
        }
        const Color base = XboxBase(button);
        color = Mix(Scale(base, 0.7f), Mix(base, {255.0f, 255.0f, 255.0f}, 0.25f), top);
        if (distance < 0.085f) {
            color = {250.0f, 250.0f, 250.0f};
        }
        return true;
    }
    if (radius > 0.9f) {
        color = {172.0f, 176.0f, 188.0f};
        return true;
    }
    color = Mix({26.0f, 28.0f, 34.0f}, {62.0f, 66.0f, 76.0f}, top);
    if (distance < 0.075f) {
        color = Ps3Symbol(button);
    }
    return true;
}

}  // namespace

void RenderFlashFrame(int type, int button, int frame, std::uint32_t* argb) noexcept {
    const bool valid = (type == 1 || type == 2) && button >= 0 && button < 4;
    if (!valid) {
        std::fill(argb, argb + kFlashIconSize * kFlashIconSize, 0u);
        return;
    }
    const Glyph glyph = BuildGlyph(type, button);
    const float scale = frame == 0 ? 0.94f : 0.72f;
    const float brightness = frame == 0 ? 1.0f : 0.78f;
    for (int py = 0; py < kFlashIconSize; ++py) {
        for (int px = 0; px < kFlashIconSize; ++px) {
            float sumR = 0.0f;
            float sumG = 0.0f;
            float sumB = 0.0f;
            int covered = 0;
            for (int sy = 0; sy < kSamples; ++sy) {
                for (int sx = 0; sx < kSamples; ++sx) {
                    const float u = (static_cast<float>(px) + (static_cast<float>(sx) + 0.5f) / kSamples) /
                                        kFlashIconSize * 2.0f - 1.0f;
                    const float v = (static_cast<float>(py) + (static_cast<float>(sy) + 0.5f) / kSamples) /
                                        kFlashIconSize * 2.0f - 1.0f;
                    Color color{};
                    if (ShadeDisc(type, button, glyph, u / scale, v / scale, color)) {
                        sumR += color.r;
                        sumG += color.g;
                        sumB += color.b;
                        ++covered;
                    }
                }
            }
            std::uint32_t pixel = 0;
            if (covered > 0) {
                const float n = static_cast<float>(covered);
                const auto channel = [&](float sum) {
                    return static_cast<std::uint32_t>(std::clamp(sum / n * brightness, 0.0f, 255.0f) + 0.5f);
                };
                const std::uint32_t alpha =
                    static_cast<std::uint32_t>(255.0f * n / static_cast<float>(kSamples * kSamples) + 0.5f);
                pixel = (alpha << 24) | (channel(sumR) << 16) | (channel(sumG) << 8) | channel(sumB);
            }
            argb[py * kFlashIconSize + px] = pixel;
        }
    }
}

}  // namespace hca::graphics
