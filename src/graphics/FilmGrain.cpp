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

std::atomic<IDirect3DPixelShader9*> g_grainShader{nullptr};
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
        g_grainShader.store(shader);
        log::Info("Grain de pellicule : shader repéré, intensité {} % de celle du jeu", GetConfig().filmGrain);
    }
}

bool OnSetPixelShader(IDirect3DPixelShader9* shader) noexcept {
    const bool bound = shader != nullptr && shader == g_grainShader.load();
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
