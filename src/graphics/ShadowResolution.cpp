#include "graphics/ShadowResolution.h"

#include <atomic>

#include "core/Config.h"
#include "core/Log.h"

namespace hca::graphics::shadows {

namespace {

constexpr UINT kFirstFilterRegister = 45;  // mLightInfo2PowFilterScale : c45 à c54
constexpr UINT kLastFilterRegister = 54;
constexpr float kOriginalTexel = 1.0f / static_cast<float>(kOriginalAtlasSize);  // 0,00048828125, exact

// Surface de niveau 0 de l'atlas agrandi (non possédée : seulement comparée aux cibles du jeu).
std::atomic<IDirect3DSurface9*> g_atlasSurface{nullptr};
std::atomic<bool> g_atlasBound{false};

}  // namespace

UINT AtlasSizeFor(int configuredSize) noexcept {
    switch (configuredSize) {
        case 4096:
        case 8192:
            return static_cast<UINT>(configuredSize);
        default:
            return kOriginalAtlasSize;
    }
}

bool IsAtlasTexture(UINT width, UINT height, DWORD usage, D3DFORMAT format) noexcept {
    return width == kOriginalAtlasSize && height == kOriginalAtlasSize && (usage & D3DUSAGE_RENDERTARGET) != 0 &&
           format == D3DFMT_R32F;
}

bool IsAtlasDepth(UINT width, UINT height, D3DFORMAT format) noexcept {
    return width == kOriginalAtlasSize && height == kOriginalAtlasSize && format == D3DFMT_D24S8;
}

D3DVIEWPORT9 ScaleViewport(const D3DVIEWPORT9& viewport, UINT atlasSize) noexcept {
    D3DVIEWPORT9 result = viewport;
    result.X = viewport.X * atlasSize / kOriginalAtlasSize;
    result.Y = viewport.Y * atlasSize / kOriginalAtlasSize;
    result.Width = viewport.Width * atlasSize / kOriginalAtlasSize;
    result.Height = viewport.Height * atlasSize / kOriginalAtlasSize;
    return result;
}

bool ScaleFilterConstants(UINT startRegister, float* values, UINT vectorCount, UINT atlasSize) noexcept {
    if (atlasSize == kOriginalAtlasSize || values == nullptr) {
        return false;
    }
    const float scale = static_cast<float>(kOriginalAtlasSize) / static_cast<float>(atlasSize);
    bool changed = false;
    for (UINT i = 0; i < vectorCount; ++i) {
        const UINT reg = startRegister + i;
        if (reg < kFirstFilterRegister || reg > kLastFilterRegister) {
            continue;
        }
        float* vector = values + i * 4;
        // Seuls les décalages d'exactement un texel de l'atlas d'origine sont corrigés.
        if (vector[2] == kOriginalTexel && vector[3] == kOriginalTexel) {
            vector[2] *= scale;
            vector[3] *= scale;
            changed = true;
        }
    }
    return changed;
}

bool ScaleCasterBias(UINT startRegister, float* values, UINT vectorCount, UINT atlasSize) noexcept {
    constexpr UINT kBiasRegister = 92;
    if (atlasSize == kOriginalAtlasSize || values == nullptr || startRegister > kBiasRegister ||
        startRegister + vectorCount <= kBiasRegister) {
        return false;
    }
    values[(kBiasRegister - startRegister) * 4] *= static_cast<float>(atlasSize) / static_cast<float>(kOriginalAtlasSize);
    return true;
}

bool AtlasBound() noexcept {
    return g_atlasBound.load();
}

UINT ActiveAtlasSize() noexcept {
    return AtlasSizeFor(GetConfig().shadowResolution);
}

UINT CreationSize(UINT width, UINT height, DWORD usage, D3DFORMAT format, bool depth) noexcept {
    const UINT size = ActiveAtlasSize();
    if (size == kOriginalAtlasSize) {
        return width;
    }
    const bool atlas = depth ? IsAtlasDepth(width, height, format) : IsAtlasTexture(width, height, usage, format);
    return atlas ? size : width;
}

void OnTextureCreated(IDirect3DTexture9* texture, UINT width, UINT height, DWORD usage, D3DFORMAT format) noexcept {
    // `width` et `height` : taille réellement créée.
    const UINT size = ActiveAtlasSize();
    if (texture == nullptr || size == kOriginalAtlasSize || width != size || height != size ||
        (usage & D3DUSAGE_RENDERTARGET) == 0 || format != D3DFMT_R32F) {
        return;
    }
    IDirect3DSurface9* surface = nullptr;
    if (SUCCEEDED(texture->GetSurfaceLevel(0, &surface))) {
        // Direct3D renvoie toujours le même objet surface pour un niveau donné : la comparaison de pointeurs suffit.
        g_atlasSurface.store(surface);
        surface->Release();
        log::Info("Ombres : atlas créé en {}x{} au lieu de 2048x2048", size, size);
    }
}

void OnSetRenderTarget(DWORD index, IDirect3DSurface9* surface) noexcept {
    if (index == 0) {
        g_atlasBound.store(surface != nullptr && surface == g_atlasSurface.load());
    }
}

bool AdjustViewport(const D3DVIEWPORT9& viewport, D3DVIEWPORT9& adjusted) noexcept {
    const UINT size = ActiveAtlasSize();
    if (size == kOriginalAtlasSize || !g_atlasBound.load()) {
        return false;
    }
    // Viewports de l'atlas d'origine seulement (carrés dans 2048x2048).
    if (viewport.X + viewport.Width > kOriginalAtlasSize || viewport.Y + viewport.Height > kOriginalAtlasSize) {
        return false;
    }
    adjusted = ScaleViewport(viewport, size);
    return true;
}

}  // namespace hca::graphics::shadows
