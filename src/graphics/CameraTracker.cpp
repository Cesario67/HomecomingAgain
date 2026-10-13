#include "graphics/CameraTracker.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#include "core/Log.h"
#include "graphics/ShaderConstantTable.h"

namespace hca::graphics::camera {

namespace {

constexpr std::size_t kMaxShaders = 1024;
constexpr std::size_t kMaxTokens = 1u << 16;

// Vertex shaders qui reçoivent mWorldViewProj en c0..c3 et mWorld en c4..c6 (table de constantes lue à la création).
std::atomic<IDirect3DVertexShader9*> g_shaders[kMaxShaders]{};
std::atomic<std::size_t> g_shaderCount{0};

// Le reste n'est touché que par le thread de rendu de la scène.
bool g_currentQualifies = false;
bool g_depthPass = false;
float g_constants[7 * 4]{};  // c0..c6
bool g_haveConstants[7]{};
bool g_capturedThisFrame = false;
bool g_haveCurrent = false;
bool g_havePrevious = false;
Matrix4 g_current{};
Matrix4 g_previous{};
bool g_logged = false;

bool HasCameraConstants(const DWORD* bytecode) {
    bool worldViewProj = false;
    bool world = false;
    for (const ShaderConstant& c : ParseConstantTable(reinterpret_cast<const std::uint32_t*>(bytecode), kMaxTokens)) {
        if (c.registerSet == 2 && c.name == "mWorldViewProj" && c.registerIndex == 0 && c.registerCount >= 4) {
            worldViewProj = true;
        }
        if (c.registerSet == 2 && c.name == "mWorld" && c.registerIndex == 4 && c.registerCount >= 3) {
            world = true;
        }
    }
    return worldViewProj && world;
}

}  // namespace

void OnVertexShaderCreated(const DWORD* bytecode, IDirect3DVertexShader9* shader) noexcept {
    try {
        if (shader == nullptr || bytecode == nullptr || !HasCameraConstants(bytecode)) {
            return;
        }
    } catch (...) {
        return;
    }
    const std::size_t index = g_shaderCount.load();
    if (index < kMaxShaders) {
        g_shaders[index].store(shader);
        g_shaderCount.store(index + 1);
    }
}

void OnSetVertexShader(IDirect3DVertexShader9* shader) noexcept {
    g_currentQualifies = false;
    const std::size_t count = g_shaderCount.load();
    for (std::size_t i = 0; shader != nullptr && i < count; ++i) {
        if (g_shaders[i].load() == shader) {
            g_currentQualifies = true;
            return;
        }
    }
}

void OnVertexShaderConstants(UINT startRegister, const float* values, UINT vectorCount) noexcept {
    if (values == nullptr) {
        return;
    }
    for (UINT i = 0; i < vectorCount; ++i) {
        const UINT reg = startRegister + i;
        if (reg < 7) {
            std::memcpy(&g_constants[reg * 4], values + i * 4, 4 * sizeof(float));
            g_haveConstants[reg] = true;
        }
    }
}

void OnSetRenderTarget(DWORD index, IDirect3DSurface9* surface, IDirect3DSurface9* depthPassSurface) noexcept {
    if (index == 0) {
        g_depthPass = surface != nullptr && surface == depthPassSurface;
    }
}

void OnDraw() noexcept {
    if (g_capturedThisFrame || !g_depthPass || !g_currentQualifies) {
        return;
    }
    for (bool have : g_haveConstants) {
        if (!have) {
            return;
        }
    }
    Matrix4 viewProjection{};
    if (!ViewProjectionFromDraw(&g_constants[0], &g_constants[16], viewProjection)) {
        return;
    }
    g_capturedThisFrame = true;
    g_current = viewProjection;
    g_haveCurrent = true;
    if (!g_logged) {
        g_logged = true;
        log::Info("Caméra : matrice vue x projection retrouvée (ligne 0 : {:.3f} {:.3f} {:.3f} {:.3f})", viewProjection[0],
                  viewProjection[1], viewProjection[2], viewProjection[3]);
    }
}

void OnPresent() noexcept {
    if (g_capturedThisFrame) {
        g_previous = g_current;
        g_havePrevious = true;
    }
    g_capturedThisFrame = false;
    g_depthPass = false;
}

bool Current(Matrix4& current, Matrix4& previous) noexcept {
    if (!g_haveCurrent || !g_havePrevious || !g_capturedThisFrame) {
        return false;
    }
    current = g_current;
    previous = g_previous;
    return true;
}

}  // namespace hca::graphics::camera
