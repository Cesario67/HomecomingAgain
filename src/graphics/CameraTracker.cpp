#include "graphics/CameraTracker.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "core/Log.h"
#include "graphics/ShaderConstantTable.h"

namespace hca::graphics::camera {

namespace {

constexpr std::size_t kMaxShaders = 1024;
constexpr std::size_t kMaxTokens = 1u << 16;
constexpr std::size_t kMaxCandidates = 16;

// Vertex shaders qui reçoivent mWorldViewProj en c0..c3 et mWorld en c4..c6 (table de constantes lue à la création).
std::atomic<IDirect3DVertexShader9*> g_shaders[kMaxShaders]{};
std::atomic<std::size_t> g_shaderCount{0};

struct Candidate {
    Matrix4 matrix{};
    int votes = 0;
};

// Le reste n'est touché que par le thread de rendu de la scène.
bool g_currentQualifies = false;
bool g_afterDepthPass = false;  // la pré-passe de profondeur a commencé dans cette image
float g_constants[7 * 4]{};      // c0..c6
bool g_haveConstants[7]{};
Candidate g_candidates[kMaxCandidates];
std::size_t g_candidateCount = 0;
int g_drawsSeen = 0;
bool g_decided = false;  // caméra choisie pour cette image
bool g_haveCurrent = false;
bool g_havePrevious = false;
Matrix4 g_current{};
Matrix4 g_previous{};
// Statistiques du journal (toutes les kReportFrames images) : caméra qui alterne entre plusieurs candidates ou qui change
// d'une image du jeu à la suivante (diagnostic des vecteurs de mouvement faux, 13/10/2026).
constexpr int kReportFrames = 600;
Matrix4 g_lastFrame{};
bool g_haveLastFrame = false;
int g_framesSeen = 0;
int g_framesWithCamera = 0;
int g_framesSeveralCandidates = 0;
std::size_t g_mostCandidates = 0;
int g_framesChanged = 0;
int g_candidateLogsLeft = 5;

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

bool LooksLikePerspectiveCamera(const Matrix4& m) noexcept {
    // Projection perspective (z_écran = z_vue en w) : la dernière ligne est la direction du regard, de longueur 1.
    const float length = std::sqrt(m[12] * m[12] + m[13] * m[13] + m[14] * m[14]);
    return std::fabs(length - 1.0f) < 0.01f;
}

bool SameCamera(const Matrix4& a, const Matrix4& b) noexcept {
    for (int i = 0; i < 16; ++i) {
        const float scale = std::fmax(1.0f, std::fabs(a[i]));
        if (std::fabs(a[i] - b[i]) > 1e-3f * scale) {
            return false;
        }
    }
    return true;
}

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
    // La pré-passe de profondeur n'utilise que mWorldViewProj (relevé du 10/10/2026, shader vs_0EA4BA20) : la caméra est
    // prise dans les dessins suivants qui ont aussi mWorld (scène principale). Les cartes d'ombres, dessinées avant la
    // pré-passe, sont ainsi exclues.
    if (index == 0 && surface != nullptr && surface == depthPassSurface) {
        g_afterDepthPass = true;
    }
}

void OnDraw() noexcept {
    if (g_decided || !g_afterDepthPass || !g_currentQualifies) {
        return;
    }
    for (bool have : g_haveConstants) {
        if (!have) {
            return;
        }
    }
    Matrix4 viewProjection{};
    if (!ViewProjectionFromDraw(&g_constants[0], &g_constants[16], viewProjection) ||
        !LooksLikePerspectiveCamera(viewProjection)) {
        return;  // interface 2D, effet plein écran ou mWorld non inversible
    }
    ++g_drawsSeen;
    // Vote : chaque dessin propose une caméra ; les objets ordinaires donnent tous la même.
    for (std::size_t i = 0; i < g_candidateCount; ++i) {
        if (SameCamera(g_candidates[i].matrix, viewProjection)) {
            ++g_candidates[i].votes;
            return;
        }
    }
    if (g_candidateCount < kMaxCandidates) {
        g_candidates[g_candidateCount++] = {viewProjection, 1};
    }
}

void OnSceneEnd() noexcept {
    if (g_decided) {
        return;
    }
    g_decided = true;
    const Candidate* best = nullptr;
    for (std::size_t i = 0; i < g_candidateCount; ++i) {
        if (best == nullptr || g_candidates[i].votes > best->votes) {
            best = &g_candidates[i];
        }
    }
    if (best == nullptr) {
        return;
    }
    g_current = best->matrix;
    g_haveCurrent = true;
    ++g_framesWithCamera;
    if (g_haveLastFrame && !SameCamera(g_lastFrame, g_current)) {
        ++g_framesChanged;
    }
    g_lastFrame = g_current;
    g_haveLastFrame = true;
    if (g_candidateCount > 1) {
        ++g_framesSeveralCandidates;
        g_mostCandidates = std::max(g_mostCandidates, g_candidateCount);
        if (g_candidateLogsLeft > 0) {
            --g_candidateLogsLeft;
            for (std::size_t i = 0; i < g_candidateCount; ++i) {
                const Matrix4& m = g_candidates[i].matrix;
                log::Info("Caméra : candidate {} sur {}, {} dessins, regard ({:.3f} {:.3f} {:.3f}), w = {:.2f}", i + 1,
                          g_candidateCount, g_candidates[i].votes, m[12], m[13], m[14], m[15]);
            }
        }
    }
}

void OnPresent() noexcept {
    if (++g_framesSeen >= kReportFrames) {
        log::Info("Caméra : {} images, dont {} avec caméra, {} avec plusieurs candidates (au plus {}), {} où la caméra a "
                  "changé depuis l'image précédente",
                  g_framesSeen, g_framesWithCamera, g_framesSeveralCandidates, g_mostCandidates, g_framesChanged);
        g_framesSeen = 0;
        g_framesWithCamera = 0;
        g_framesSeveralCandidates = 0;
        g_mostCandidates = 0;
        g_framesChanged = 0;
    }
    g_decided = false;
    g_afterDepthPass = false;
    g_candidateCount = 0;
    g_drawsSeen = 0;
}

void CommitFrame() noexcept {
    OnSceneEnd();
    if (g_haveCurrent && g_candidateCount > 0) {
        g_previous = g_current;
        g_havePrevious = true;
    }
}

bool Current(Matrix4& current, Matrix4& previous) noexcept {
    OnSceneEnd();
    if (!g_haveCurrent || !g_havePrevious || g_candidateCount == 0) {
        return false;
    }
    current = g_current;
    previous = g_previous;
    return true;
}

}  // namespace hca::graphics::camera
