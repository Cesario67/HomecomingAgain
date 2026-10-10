#include "graphics/FilmGrain.h"

#include <atomic>
#include <cstring>

#include "core/Config.h"
#include "core/Log.h"
#include "graphics/ShaderConstantTable.h"

namespace hca::graphics::grain {

namespace {

// Borne de lecture du bytecode : la table de constantes est au début, la lecture s'arrête à la première instruction.
constexpr std::size_t kMaxTokens = 1u << 16;

// Le moteur crée plusieurs variantes du shader du grain (8 constatées le 10/10/2026) : on les garde toutes.
constexpr std::size_t kMaxGrainShaders = 32;
std::atomic<IDirect3DPixelShader9*> g_grainShaders[kMaxGrainShaders]{};
std::atomic<std::size_t> g_grainShaderCount{0};

bool IsKnownGrainShader(IDirect3DPixelShader9* shader) noexcept {
    const std::size_t count = g_grainShaderCount.load();
    for (std::size_t i = 0; i < count && i < kMaxGrainShaders; ++i) {
        if (g_grainShaders[i].load() == shader) {
            return true;
        }
    }
    return false;
}
// Seul le thread de rendu change de shader et de constantes à ce moment de l'image : pas de verrou.
bool g_bound = false;
bool g_haveIntensity = false;
float g_intensity[4]{};

}  // namespace

bool IsGrainShader(const DWORD* bytecode) noexcept {
    try {
        for (const ShaderConstant& constant :
             ParseConstantTable(reinterpret_cast<const std::uint32_t*>(bytecode), kMaxTokens)) {
            if (constant.name == "grainTexture") {
                return true;
            }
        }
    } catch (...) {
    }
    return false;
}

bool ScaleIntensity(UINT startRegister, float* values, UINT vectorCount, float scale) noexcept {
    if (values == nullptr || startRegister > kIntensityRegister || startRegister + vectorCount <= kIntensityRegister) {
        return false;
    }
    values[(kIntensityRegister - startRegister) * 4 + 2] *= scale;
    return true;
}

float Scale() noexcept {
    return static_cast<float>(GetConfig().filmGrain) / 100.0f;
}

bool Active() noexcept {
    return GetConfig().filmGrain != 100;
}

void OnPixelShaderCreated(const DWORD* bytecode, IDirect3DPixelShader9* shader) noexcept {
    if (shader != nullptr && bytecode != nullptr && IsGrainShader(bytecode)) {
        const std::size_t index = g_grainShaderCount.load();
        if (index >= kMaxGrainShaders) {
            log::Warn("Grain de pellicule : plus de {} variantes du shader, les suivantes gardent leur grain", kMaxGrainShaders);
            return;
        }
        g_grainShaders[index].store(shader);
        g_grainShaderCount.store(index + 1);
        log::Info("Grain de pellicule : variante {} du shader repérée, intensité {} % de celle du jeu", index + 1,
                  GetConfig().filmGrain);
    }
}

bool OnSetPixelShader(IDirect3DPixelShader9* shader) noexcept {
    const bool bound = shader != nullptr && IsKnownGrainShader(shader);
    const bool changed = bound != g_bound;
    g_bound = bound;
    return changed;
}

bool GrainShaderBound() noexcept {
    return g_bound;
}

void RememberIntensity(UINT startRegister, const float* values, UINT vectorCount) noexcept {
    if (values == nullptr || startRegister > kIntensityRegister || startRegister + vectorCount <= kIntensityRegister) {
        return;
    }
    std::memcpy(g_intensity, values + (kIntensityRegister - startRegister) * 4, sizeof(g_intensity));
    g_haveIntensity = true;
}

bool LastIntensity(float out[4]) noexcept {
    if (!g_haveIntensity) {
        return false;
    }
    std::memcpy(out, g_intensity, sizeof(g_intensity));
    return true;
}

}  // namespace hca::graphics::grain
