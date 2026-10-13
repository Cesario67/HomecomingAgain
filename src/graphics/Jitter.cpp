#include "graphics/Jitter.h"

#include <cmath>
#include <cstring>

#include "core/Config.h"
#include "core/Log.h"

namespace hca::graphics::jitter {

namespace {

constexpr std::uint32_t kPhases = 16;

// Le reste n'est touché que par le thread de rendu de la scène.
bool g_inScene = false;  // entre le début de la pré-passe de profondeur et la fin de la scène 3D
std::uint64_t g_frame = 0;
Offset g_current;
float g_ndcX = 0.0f;
float g_ndcY = 0.0f;
bool g_logged = false;

}  // namespace

float Halton(std::uint32_t index, std::uint32_t base) noexcept {
    float result = 0.0f;
    float fraction = 1.0f;
    while (index > 0 && base > 1) {
        fraction /= static_cast<float>(base);
        result += fraction * static_cast<float>(index % base);
        index /= base;
    }
    return result;
}

Offset OffsetForFrame(std::uint64_t frame, float amplitude) noexcept {
    // Indice à partir de 1 : Halton(0) vaut 0 dans les deux bases.
    const auto index = static_cast<std::uint32_t>(frame % kPhases) + 1;
    return {(Halton(index, 2) - 0.5f) * amplitude, (Halton(index, 3) - 0.5f) * amplitude};
}

bool ApplyToMatrix(float* registers, float ndcX, float ndcY) noexcept {
    const float* row3 = registers + 12;
    if (std::fabs(row3[0]) + std::fabs(row3[1]) + std::fabs(row3[2]) < 1e-6f) {
        return false;  // projection orthographique (interface, quadrilatère plein écran)
    }
    for (int i = 0; i < 4; ++i) {
        registers[i] += ndcX * row3[i];
        registers[4 + i] += ndcY * row3[i];
    }
    return true;
}

void OnSetRenderTarget(DWORD index, IDirect3DSurface9* surface, IDirect3DSurface9* depthPassSurface,
                       bool allowed) noexcept {
    const float amplitude = GetConfig().dlssJitter;
    if (!allowed) {
        g_current = {};
        return;
    }
    if (index != 0 || surface == nullptr || surface != depthPassSurface || amplitude <= 0.0f || g_inScene) {
        return;
    }
    D3DSURFACE_DESC desc{};
    if (FAILED(surface->GetDesc(&desc)) || desc.Width == 0 || desc.Height == 0) {
        return;
    }
    g_current = OffsetForFrame(g_frame, amplitude);
    // Pixels vers coordonnées normalisées : l'axe y de l'écran descend, celui des coordonnées normalisées monte.
    g_ndcX = 2.0f * g_current.x / static_cast<float>(desc.Width);
    g_ndcY = -2.0f * g_current.y / static_cast<float>(desc.Height);
    g_inScene = true;
    if (!g_logged) {
        g_logged = true;
        log::Info("Décalage de caméra (jitter) actif : {} phases de Halton, amplitude {:.2f} pixel(s)", kPhases,
                  amplitude);
    }
}

void OnSceneEnd() noexcept {
    g_inScene = false;
}

void OnPresent() noexcept {
    g_inScene = false;
    ++g_frame;
}

bool Adjust(UINT startRegister, const float* values, UINT vectorCount, float* copy) noexcept {
    // mWorldViewProj est toujours en c0..c3 (relevé du 10/10/2026) : seulement les envois qui le contiennent en entier.
    if (!g_inScene || values == nullptr || startRegister != 0 || vectorCount < 4) {
        return false;
    }
    std::memcpy(copy, values, vectorCount * 4 * sizeof(float));
    return ApplyToMatrix(copy, g_ndcX, g_ndcY);
}

Offset Current() noexcept {
    return g_current;
}

}  // namespace hca::graphics::jitter
