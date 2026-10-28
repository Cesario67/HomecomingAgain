#include "graphics/HorPlus.h"

#include <atomic>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "graphics/HorPlusRules.h"

namespace hca::graphics::horplus {

namespace {

constexpr size_t kCreateTextureIndex = 23;
constexpr size_t kCreateRenderTargetIndex = 28;
constexpr size_t kCreateDepthStencilSurfaceIndex = 29;
constexpr size_t kSetViewportIndex = 47;
constexpr size_t kSetScissorRectIndex = 75;
constexpr size_t kSetVertexShaderConstantFIndex = 94;
constexpr size_t kSetPixelShaderConstantFIndex = 109;

using CreateTextureFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                                    IDirect3DTexture9**, HANDLE*);
using CreateSurfaceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE,
                                                    DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
using SetViewportFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const D3DVIEWPORT9*);
using SetScissorRectFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*);
using SetVertexShaderConstantFFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, const float*, UINT);

CreateTextureFn g_createTexture = nullptr;
CreateSurfaceFn g_createRenderTarget = nullptr;
CreateSurfaceFn g_createDepthStencilSurface = nullptr;
SetViewportFn g_setViewport = nullptr;
SetScissorRectFn g_setScissorRect = nullptr;
SetVertexShaderConstantFFn g_setVertexShaderConstantF = nullptr;
SetVertexShaderConstantFFn g_setPixelShaderConstantF = nullptr;  // same signature as the vertex one

std::atomic<long> g_backWidth{0};
std::atomic<long> g_backHeight{0};
std::atomic<int> g_logCount{0};

bool Active() noexcept {
    return IsWiderThanSixteenNine(g_backWidth.load(), g_backHeight.load());
}

// Height to ask for a w x h target: reshaped when it is an exact 16:9 picture, unchanged otherwise.
UINT TargetHeight(UINT width, UINT height) noexcept {
    if (!Active() || !IsSixteenNineTarget(static_cast<long>(width), static_cast<long>(height))) {
        return height;
    }
    return static_cast<UINT>(ReshapedHeight(static_cast<long>(width), g_backWidth.load(), g_backHeight.load()));
}

void LogReshaped(const char* what, UINT width, UINT oldHeight, UINT newHeight) {
    if (newHeight != oldHeight && g_logCount.fetch_add(1) < 24) {
        log::Info("Hor+ : {} {}x{} -> {}x{}", what, width, oldHeight, width, newHeight);
    }
}

HRESULT STDMETHODCALLTYPE HookCreateTexture(IDirect3DDevice9* device, UINT width, UINT height, UINT levels,
                                            DWORD usage, D3DFORMAT format, D3DPOOL pool,
                                            IDirect3DTexture9** texture, HANDLE* sharedHandle) {
    if ((usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) != 0) {
        const UINT newHeight = TargetHeight(width, height);
        LogReshaped("texture cible", width, height, newHeight);
        height = newHeight;
    }
    return g_createTexture(device, width, height, levels, usage, format, pool, texture, sharedHandle);
}

HRESULT STDMETHODCALLTYPE HookCreateRenderTarget(IDirect3DDevice9* device, UINT width, UINT height, D3DFORMAT format,
                                                 D3DMULTISAMPLE_TYPE multiSample, DWORD quality, BOOL lockable,
                                                 IDirect3DSurface9** surface, HANDLE* sharedHandle) {
    const UINT newHeight = TargetHeight(width, height);
    LogReshaped("surface cible", width, height, newHeight);
    return g_createRenderTarget(device, width, newHeight, format, multiSample, quality, lockable, surface,
                                sharedHandle);
}

HRESULT STDMETHODCALLTYPE HookCreateDepthStencilSurface(IDirect3DDevice9* device, UINT width, UINT height,
                                                        D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample,
                                                        DWORD quality, BOOL discard, IDirect3DSurface9** surface,
                                                        HANDLE* sharedHandle) {
    const UINT newHeight = TargetHeight(width, height);
    LogReshaped("surface de profondeur", width, height, newHeight);
    return g_createDepthStencilSurface(device, width, newHeight, format, multiSample, quality, discard, surface,
                                       sharedHandle);
}

HRESULT STDMETHODCALLTYPE HookSetViewport(IDirect3DDevice9* device, const D3DVIEWPORT9* viewport) {
    if (viewport != nullptr && Active() && viewport->X == 0 && viewport->Y == 0) {
        const UINT newHeight = TargetHeight(viewport->Width, viewport->Height);
        if (newHeight != viewport->Height) {
            D3DVIEWPORT9 reshaped = *viewport;
            reshaped.Height = newHeight;
            return g_setViewport(device, &reshaped);
        }
    }
    return g_setViewport(device, viewport);
}

HRESULT STDMETHODCALLTYPE HookSetScissorRect(IDirect3DDevice9* device, const RECT* rect) {
    if (rect != nullptr && Active() && rect->left == 0 && rect->top == 0) {
        const UINT width = static_cast<UINT>(rect->right);
        const UINT height = static_cast<UINT>(rect->bottom);
        const UINT newHeight = TargetHeight(width, height);
        if (newHeight != height) {
            RECT reshaped = *rect;
            reshaped.bottom = static_cast<LONG>(newHeight);
            return g_setScissorRect(device, &reshaped);
        }
    }
    return g_setScissorRect(device, rect);
}

HRESULT STDMETHODCALLTYPE HookSetVertexShaderConstantF(IDirect3DDevice9* device, UINT start, const float* data,
                                                       UINT count) {
    if (count == 1 && data != nullptr && Active() && IsHalfPixelOffsetConstant(data)) {
        const float shifted[4] = {data[0], data[1] * HeightFactor(g_backWidth.load(), g_backHeight.load()), data[2], data[3]};
        return g_setVertexShaderConstantF(device, start, shifted, count);
    }
    if (count < 4 || data == nullptr || !Active() || count > 32) {
        return g_setVertexShaderConstantF(device, start, data, count);
    }
    // c0 x4: mWorldViewProj of the world camera (any rotation). Elsewhere: only a bare 16:9 projection (c222 x4).
    const bool world = start == 0 && IsSixteenNineProjection(data);
    if (!world && !IsBareSixteenNineProjection(data)) {
        return g_setVertexShaderConstantF(device, start, data, count);
    }
    float buffer[4 * 32];
    for (UINT i = 0; i < count * 4; ++i) {
        buffer[i] = data[i];
    }
    ScaleProjectionX(buffer, ProjectionXFactor(g_backWidth.load(), g_backHeight.load()));
    return g_setVertexShaderConstantF(device, start, buffer, count);
}

HRESULT STDMETHODCALLTYPE HookSetPixelShaderConstantF(IDirect3DDevice9* device, UINT start, const float* data,
                                                      UINT count) {
    if (count == 1 && data != nullptr && Active() && IsScreenScaleConstant(data)) {
        const float rescaled[4] = {data[0], data[1] * HeightFactor(g_backWidth.load(), g_backHeight.load()), data[2], data[3]};
        if (g_logCount.fetch_add(1) < 40) {
            log::Info("Hor+ : constante c{} du pixel shader (échelle écran) {:.7f} -> {:.7f}", start, data[1], rescaled[1]);
        }
        return g_setPixelShaderConstantF(device, start, rescaled, count);
    }
    if (count == 1 && data != nullptr && Active() && IsPixelToScreenConstant(data)) {
        const float factor = HeightFactor(g_backWidth.load(), g_backHeight.load());
        const float rescaled[4] = {data[0], data[1] * factor, data[2], data[3] / factor};
        if (g_logCount.fetch_add(1) < 40) {
            log::Info("Hor+ : constante c{} du pixel shader (position pixel vers écran) {:.6f} {:.1f} -> {:.6f} {:.1f}", start,
                      data[1], data[3], rescaled[1], rescaled[3]);
        }
        return g_setPixelShaderConstantF(device, start, rescaled, count);
    }
    if (count == 1 && data != nullptr && Active() && IsInverseProjectionScales(data)) {
        const float factor = ProjectionXFactor(g_backWidth.load(), g_backHeight.load());
        const float scales[4] = {data[0] / factor, data[1], data[2], data[3]};
        if (g_logCount.fetch_add(1) < 40) {
            log::Info("Hor+ : constante c{} du pixel shader (inverse de la projection) {:.5f} -> {:.5f}", start,
                      data[0], scales[0]);
        }
        return g_setPixelShaderConstantF(device, start, scales, count);
    }
    return g_setPixelShaderConstantF(device, start, data, count);
}

}  // namespace

void SetBackBufferSize(unsigned width, unsigned height) noexcept {
    g_backWidth = static_cast<long>(width);
    g_backHeight = static_cast<long>(height);
    if (Active() && GetConfig().horPlus) {
        log::Info("Hor+ : tampon arrière {}x{}, facteur x de la projection {:.4f}", width, height,
                  ProjectionXFactor(static_cast<long>(width), static_cast<long>(height)));
    }
}

void Install(IDirect3DDevice9* device) {
    if (!GetConfig().horPlus) {
        return;
    }
    bool ok = true;
    ok &= hooking::PatchVtable(device, kCreateTextureIndex, &HookCreateTexture, &g_createTexture);
    ok &= hooking::PatchVtable(device, kCreateRenderTargetIndex, &HookCreateRenderTarget, &g_createRenderTarget);
    ok &= hooking::PatchVtable(device, kCreateDepthStencilSurfaceIndex, &HookCreateDepthStencilSurface,
                               &g_createDepthStencilSurface);
    ok &= hooking::PatchVtable(device, kSetViewportIndex, &HookSetViewport, &g_setViewport);
    ok &= hooking::PatchVtable(device, kSetScissorRectIndex, &HookSetScissorRect, &g_setScissorRect);
    ok &= hooking::PatchVtable(device, kSetVertexShaderConstantFIndex, &HookSetVertexShaderConstantF,
                               &g_setVertexShaderConstantF);
    ok &= hooking::PatchVtable(device, kSetPixelShaderConstantFIndex, &HookSetPixelShaderConstantF,
                               &g_setPixelShaderConstantF);
    if (ok) {
        log::Info("Hor+ (prototype) : installé");
    } else {
        log::Warn("Hor+ : interception incomplète");
    }
}

}  // namespace hca::graphics::horplus
