#include "graphics/SceneHooks.h"

#include <algorithm>
#include <cstring>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "dlss/SharedFrames.h"
#include "graphics/CameraTracker.h"
#include "graphics/DepthEncoding.h"
#include "graphics/Jitter.h"
#include "graphics/FilmGrain.h"
#include "graphics/InternalDraw.h"
#include "graphics/MotionVectors.h"
#include "graphics/ShadowResolution.h"
#include "graphics/Ssao.h"

namespace hca::graphics::scene {

namespace {

// Indices dans la vtable de IDirect3DDevice9 (ordre de déclaration de d3d9.h, IUnknown compris).
constexpr size_t kCreateTextureIndex = 23;
constexpr size_t kCreateDepthStencilSurfaceIndex = 29;
constexpr size_t kSetRenderTargetIndex = 37;
constexpr size_t kSetViewportIndex = 47;
constexpr size_t kDrawIndexedPrimitiveIndex = 82;
constexpr size_t kCreateVertexShaderIndex = 91;
constexpr size_t kSetVertexShaderIndex = 92;
constexpr size_t kSetVertexShaderConstantFIndex = 94;
constexpr size_t kCreatePixelShaderIndex = 106;
constexpr size_t kSetPixelShaderIndex = 107;
constexpr size_t kSetPixelShaderConstantFIndex = 109;

// Plus grand envoi de constantes corrigé (au-delà, envoyé tel quel) : 256 registres, la limite de ps_3_0.
constexpr UINT kMaxConstantVectors = 256;

using CreateTextureFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                                    IDirect3DTexture9**, HANDLE*);
using CreateSurfaceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE,
                                                    DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
using SetRenderTargetFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);
using SetViewportFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const D3DVIEWPORT9*);
using CreatePixelShaderFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const DWORD*, IDirect3DPixelShader9**);
using SetPixelShaderFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DPixelShader9*);
using CreateVertexShaderFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const DWORD*, IDirect3DVertexShader9**);
using SetVertexShaderFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DVertexShader9*);
using DrawIndexedPrimitiveFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT,
                                                           UINT);
using SetShaderConstantFFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, const float*, UINT);

CreateTextureFn g_createTexture = nullptr;
CreateSurfaceFn g_createDepthStencilSurface = nullptr;
SetRenderTargetFn g_setRenderTarget = nullptr;
SetViewportFn g_setViewport = nullptr;
SetShaderConstantFFn g_setVertexShaderConstantF = nullptr;
CreateVertexShaderFn g_createVertexShader = nullptr;
SetVertexShaderFn g_setVertexShader = nullptr;
DrawIndexedPrimitiveFn g_drawIndexedPrimitive = nullptr;
SetShaderConstantFFn g_setPixelShaderConstantF = nullptr;
CreatePixelShaderFn g_createPixelShader = nullptr;
SetPixelShaderFn g_setPixelShader = nullptr;

HRESULT STDMETHODCALLTYPE HookCreateTexture(IDirect3DDevice9* device, UINT width, UINT height, UINT levels, DWORD usage,
                                            D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** texture,
                                            HANDLE* sharedHandle) {
    if (t_internalDraw) {
        return g_createTexture(device, width, height, levels, usage, format, pool, texture, sharedHandle);
    }
    const UINT size = shadows::CreationSize(width, height, usage, format, false);
    HRESULT result = g_createTexture(device, size, size == width ? height : size, levels, usage, format, pool, texture,
                                     sharedHandle);
    if (FAILED(result) && size != width) {
        log::Warn("Ombres : atlas {}x{} refusé par le pilote (0x{:08X}), taille d'origine conservée", size, size,
                  static_cast<unsigned long>(result));
        result = g_createTexture(device, width, height, levels, usage, format, pool, texture, sharedHandle);
    }
    if (SUCCEEDED(result) && texture != nullptr && *texture != nullptr) {
        D3DSURFACE_DESC desc{};
        if (SUCCEEDED((*texture)->GetLevelDesc(0, &desc))) {
            shadows::OnTextureCreated(*texture, desc.Width, desc.Height, usage, format);
            GetSsao().OnTextureCreated(*texture, desc.Width, desc.Height, usage, format);
        }
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateDepthStencilSurface(IDirect3DDevice9* device, UINT width, UINT height,
                                                        D3DFORMAT format, D3DMULTISAMPLE_TYPE multisample,
                                                        DWORD quality, BOOL discard, IDirect3DSurface9** surface,
                                                        HANDLE* sharedHandle) {
    if (!t_internalDraw) {
        const UINT size = shadows::CreationSize(width, height, 0, format, true);
        if (size != width) {
            const HRESULT result = g_createDepthStencilSurface(device, size, size, format, multisample, quality,
                                                               discard, surface, sharedHandle);
            if (SUCCEEDED(result)) {
                return result;
            }
            log::Warn("Ombres : profondeur {}x{} refusée par le pilote (0x{:08X})", size, size,
                      static_cast<unsigned long>(result));
        }
    }
    return g_createDepthStencilSurface(device, width, height, format, multisample, quality, discard, surface,
                                       sharedHandle);
}

HRESULT STDMETHODCALLTYPE HookSetRenderTarget(IDirect3DDevice9* device, DWORD index, IDirect3DSurface9* surface) {
    if (!t_internalDraw) {
        // Avant le changement de cible : le SSAO s'applique à la cible encore active (l'image de la scène).
        GetSsao().OnSetRenderTarget(device, index, surface);
        const bool sceneEnded = GetSsao().ConsumeSceneReady();
        if (sceneEnded) {
            jitter::OnSceneEnd();
        }
        if (sceneEnded && GetConfig().dlssShareFrames) {
            // Fin de la scène 3D : vecteurs de mouvement, puis partage des trois images avec le processus DLSS.
            IDirect3DTexture9* motion = GetMotionVectors().Render(device, GetSsao().DepthTexture());
            const bool cut = camera::IsCut();
            const bool shared = dlss::ShareFrame(GetSsao().SceneTexture(), GetSsao().DepthTexture(), motion, cut);
            // Image sautée : la caméra précédente reste celle de la dernière image transmise (vecteurs cohérents pour
            // le DLSS). Sans vecteurs (première image), la caméra est retenue pour amorcer.
            if (shared || motion == nullptr) {
                camera::CommitFrame();
            }
        }
        shadows::OnSetRenderTarget(index, surface);
        camera::OnSetRenderTarget(index, surface, GetSsao().DepthSurface());
        if (GetConfig().dlssShareFrames) {
            // Avec Apply=1, décalage seulement si le DLSS renvoie bien ses images (sinon l'image tremblerait).
            const bool allowed = !GetConfig().dlssApply || dlss::ReturnActive();
            jitter::OnSetRenderTarget(index, surface, GetSsao().DepthSurface(), allowed);
        }
    }
    return g_setRenderTarget(device, index, surface);
}

HRESULT STDMETHODCALLTYPE HookCreateVertexShader(IDirect3DDevice9* device, const DWORD* function,
                                                 IDirect3DVertexShader9** shader) {
    const HRESULT result = g_createVertexShader(device, function, shader);
    if (!t_internalDraw && SUCCEEDED(result) && shader != nullptr) {
        camera::OnVertexShaderCreated(function, *shader);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookSetVertexShader(IDirect3DDevice9* device, IDirect3DVertexShader9* shader) {
    if (!t_internalDraw) {
        camera::OnSetVertexShader(shader);
    }
    return g_setVertexShader(device, shader);
}

HRESULT STDMETHODCALLTYPE HookDrawIndexedPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertex,
                                                   UINT minIndex, UINT vertices, UINT startIndex, UINT count) {
    if (!t_internalDraw) {
        camera::OnDraw();
    }
    return g_drawIndexedPrimitive(device, type, baseVertex, minIndex, vertices, startIndex, count);
}

HRESULT STDMETHODCALLTYPE HookSetViewport(IDirect3DDevice9* device, const D3DVIEWPORT9* viewport) {
    D3DVIEWPORT9 adjusted{};
    if (!t_internalDraw && viewport != nullptr && shadows::AdjustViewport(*viewport, adjusted)) {
        return g_setViewport(device, &adjusted);
    }
    return g_setViewport(device, viewport);
}

HRESULT STDMETHODCALLTYPE HookSetVertexShaderConstantF(IDirect3DDevice9* device, UINT start, const float* data,
                                                       UINT count) {
    if (!t_internalDraw) {
        GetSsao().OnVertexShaderConstants(start, data, count);
        // La caméra est lue avant le décalage (jitter) : les vecteurs de mouvement n'en tiennent pas compte.
        camera::OnVertexShaderConstants(start, data, count);
        float copy[kMaxConstantVectors * 4];
        if (count <= kMaxConstantVectors && jitter::Adjust(start, data, count, copy)) {
            return g_setVertexShaderConstantF(device, start, copy, count);
        }
    }
    return g_setVertexShaderConstantF(device, start, data, count);
}

HRESULT STDMETHODCALLTYPE HookSetPixelShaderConstantF(IDirect3DDevice9* device, UINT start, const float* data,
                                                      UINT count) {
    if (t_internalDraw || data == nullptr || count > kMaxConstantVectors) {
        return g_setPixelShaderConstantF(device, start, data, count);
    }
    depth::OnPixelShaderConstants(start, data, count);
    const bool touchesShadowFilter = start <= 54 && start + count > 45 &&
                                     shadows::ActiveAtlasSize() != shadows::kOriginalAtlasSize;
    const bool touchesGrain = grain::Active() && start <= grain::kIntensityRegister &&
                              start + count > grain::kIntensityRegister;
    // Décalage de pente des ombres (c92), seulement pendant le dessin de l'atlas : ailleurs c92 sert à autre chose.
    const bool touchesCasterBias = start <= 92 && start + count > 92 && shadows::AtlasBound() &&
                                   shadows::ActiveAtlasSize() != shadows::kOriginalAtlasSize;
    if (!touchesShadowFilter && !touchesGrain && !touchesCasterBias) {
        return g_setPixelShaderConstantF(device, start, data, count);
    }
    // Copie corrigée : filtre des ombres (c45..c54) et intensité du grain (c14, seulement sous son shader).
    float copy[kMaxConstantVectors * 4];
    std::memcpy(copy, data, count * 4 * sizeof(float));
    bool changed = false;
    if (touchesCasterBias) {
        changed |= shadows::ScaleCasterBias(start, copy, count, shadows::ActiveAtlasSize());
    }
    if (touchesShadowFilter) {
        changed |= shadows::ScaleFilterConstants(start, copy, count, shadows::ActiveAtlasSize());
    }
    if (touchesGrain) {
        grain::RememberIntensity(start, data, count);
        if (grain::GrainShaderBound()) {
            changed |= grain::ScaleIntensity(start, copy, count, grain::Scale());
        }
    }
    return g_setPixelShaderConstantF(device, start, changed ? copy : data, count);
}

HRESULT STDMETHODCALLTYPE HookCreatePixelShader(IDirect3DDevice9* device, const DWORD* function,
                                                IDirect3DPixelShader9** shader) {
    const HRESULT result = g_createPixelShader(device, function, shader);
    if (!t_internalDraw && SUCCEEDED(result) && shader != nullptr) {
        grain::OnPixelShaderCreated(function, *shader);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookSetPixelShader(IDirect3DDevice9* device, IDirect3DPixelShader9* shader) {
    const HRESULT result = g_setPixelShader(device, shader);
    // Les constantes survivent au changement de shader : c14 est corrigé à l'arrivée du shader du grain et rétabli à
    // son départ, pour les shaders suivants.
    float intensity[4];
    if (!t_internalDraw && grain::Active() && grain::OnSetPixelShader(shader) && grain::LastIntensity(intensity)) {
        if (grain::GrainShaderBound()) {
            grain::ScaleIntensity(grain::kIntensityRegister, intensity, 1, grain::Scale());
        }
        g_setPixelShaderConstantF(device, grain::kIntensityRegister, intensity, 1);
    }
    return result;
}
}  // namespace

void Install(IDirect3DDevice9* device) {
    const Config& config = GetConfig();
    const bool shadowsActive = shadows::ActiveAtlasSize() != shadows::kOriginalAtlasSize;
    if (!shadowsActive && !config.ssao && !grain::Active() && !config.dlssShareFrames) {
        return;
    }
    bool ok = true;
    ok &= hooking::PatchVtable(device, kCreateTextureIndex, &HookCreateTexture, &g_createTexture);
    ok &= hooking::PatchVtable(device, kCreateDepthStencilSurfaceIndex, &HookCreateDepthStencilSurface,
                               &g_createDepthStencilSurface);
    ok &= hooking::PatchVtable(device, kSetRenderTargetIndex, &HookSetRenderTarget, &g_setRenderTarget);
    ok &= hooking::PatchVtable(device, kSetViewportIndex, &HookSetViewport, &g_setViewport);
    ok &= hooking::PatchVtable(device, kSetVertexShaderConstantFIndex, &HookSetVertexShaderConstantF,
                               &g_setVertexShaderConstantF);
    ok &= hooking::PatchVtable(device, kSetPixelShaderConstantFIndex, &HookSetPixelShaderConstantF,
                               &g_setPixelShaderConstantF);
    ok &= hooking::PatchVtable(device, kCreatePixelShaderIndex, &HookCreatePixelShader, &g_createPixelShader);
    ok &= hooking::PatchVtable(device, kSetPixelShaderIndex, &HookSetPixelShader, &g_setPixelShader);
    if (config.dlssShareFrames) {
        ok &= hooking::PatchVtable(device, kCreateVertexShaderIndex, &HookCreateVertexShader, &g_createVertexShader);
        ok &= hooking::PatchVtable(device, kSetVertexShaderIndex, &HookSetVertexShader, &g_setVertexShader);
        ok &= hooking::PatchVtable(device, kDrawIndexedPrimitiveIndex, &HookDrawIndexedPrimitive,
                                   &g_drawIndexedPrimitive);
    }
    if (!ok) {
        log::Error("Ombres et SSAO : interception incomplète du périphérique");
        return;
    }
    log::Info("Ombres : atlas {}x{} ({}), SSAO : {}, grain : {} %", shadows::ActiveAtlasSize(),
              shadows::ActiveAtlasSize(), shadowsActive ? "agrandi" : "jeu d'origine", config.ssao ? "activé" : "désactivé",
              config.filmGrain);
}

}  // namespace hca::graphics::scene
