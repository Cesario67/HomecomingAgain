#include "graphics/UiClip.h"

#include <atomic>
#include <cstring>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"

namespace hca::graphics::uiclip {

namespace {

constexpr size_t kSetViewportIndex = 47;
constexpr size_t kSetScissorRectIndex = 75;
constexpr size_t kSetVertexShaderConstantFIndex = 94;
constexpr long kLegacyWidth = 1280;
constexpr long kLegacyHeight = 720;

using SetViewportFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const D3DVIEWPORT9*);
using SetScissorRectFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*);
using SetVertexShaderConstantFFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, const float*, UINT);

SetViewportFn g_setViewport = nullptr;
SetScissorRectFn g_setScissorRect = nullptr;
SetVertexShaderConstantFFn g_setVertexShaderConstantF = nullptr;

// Zone 1280x720 étendue en vigueur : rapports 1280 / largeur et 720 / hauteur de la cible (thread de rendu seul).
bool g_active = false;
float g_scaleX = 1.0f;
float g_scaleY = 1.0f;
std::atomic<bool> g_logged{false};
std::atomic<bool> g_loggedPartial{false};

// Taille de la cible de dessin 0 ; faux si elle est illisible.
bool TargetSize(IDirect3DDevice9* device, long& width, long& height) noexcept {
    IDirect3DSurface9* target = nullptr;
    if (FAILED(device->GetRenderTarget(0, &target)) || target == nullptr) {
        return false;
    }
    D3DSURFACE_DESC desc{};
    const bool ok = SUCCEEDED(target->GetDesc(&desc));
    target->Release();
    width = static_cast<long>(desc.Width);
    height = static_cast<long>(desc.Height);
    return ok;
}

// La zone demandée est-elle la zone 1280x720 du jeu sur une cible plus grande ? Si oui, taille de la cible.
bool LegacyTarget(IDirect3DDevice9* device, long x, long y, long width, long height, long& targetWidth,
                  long& targetHeight) noexcept {
    if (x != 0 || y != 0 || width != kLegacyWidth || height != kLegacyHeight ||
        !TargetSize(device, targetWidth, targetHeight)) {
        return false;
    }
    long expandedWidth = width;
    long expandedHeight = height;
    return ExpandLegacyRect(x, y, expandedWidth, expandedHeight, targetWidth, targetHeight);
}

HRESULT STDMETHODCALLTYPE HookSetViewport(IDirect3DDevice9* device, const D3DVIEWPORT9* viewport) {
    long targetWidth = 0;
    long targetHeight = 0;
    if (viewport != nullptr &&
        LegacyTarget(device, static_cast<long>(viewport->X), static_cast<long>(viewport->Y),
                     static_cast<long>(viewport->Width), static_cast<long>(viewport->Height), targetWidth,
                     targetHeight)) {
        g_active = true;
        g_scaleX = static_cast<float>(kLegacyWidth) / static_cast<float>(targetWidth);
        g_scaleY = static_cast<float>(kLegacyHeight) / static_cast<float>(targetHeight);
        D3DVIEWPORT9 expanded = *viewport;
        expanded.Width = static_cast<DWORD>(targetWidth);
        expanded.Height = static_cast<DWORD>(targetHeight);
        if (!g_logged.exchange(true)) {
            log::Info("Interface : zone 1280x720 du jeu étendue à {}x{}, projection compensée (carte)", targetWidth,
                      targetHeight);
        }
        return g_setViewport(device, &expanded);
    }
    g_active = false;
    return g_setViewport(device, viewport);
}

HRESULT STDMETHODCALLTYPE HookSetScissorRect(IDirect3DDevice9* device, const RECT* rect) {
    long targetWidth = 0;
    long targetHeight = 0;
    if (rect != nullptr &&
        LegacyTarget(device, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, targetWidth,
                     targetHeight)) {
        const RECT expanded{0, 0, targetWidth, targetHeight};
        return g_setScissorRect(device, &expanded);
    }
    return g_setScissorRect(device, rect);
}

HRESULT STDMETHODCALLTYPE HookSetVertexShaderConstantF(IDirect3DDevice9* device, UINT start, const float* data,
                                                       UINT count) {
    // mWorldViewProj en c0..c3 (shaders de la carte, vérifié le 16/10/2026) : envoyé en un seul appel c0 x4.
    if (g_active && data != nullptr && start == 0 && count >= 4) {
        float rows[16];
        std::memcpy(rows, data, sizeof(rows));
        CompensateProjection(rows, g_scaleX, g_scaleY);
        if (count == 4) {
            return g_setVertexShaderConstantF(device, start, rows, count);
        }
        const HRESULT first = g_setVertexShaderConstantF(device, 0, rows, 4);
        return FAILED(first) ? first : g_setVertexShaderConstantF(device, 4, data + 16, count - 4);
    }
    if (g_active && data != nullptr && start < 4 && !g_loggedPartial.exchange(true)) {
        log::Warn("Interface : projection envoyée en partie (c{} x{}), non compensée", start, count);
    }
    return g_setVertexShaderConstantF(device, start, data, count);
}

}  // namespace

bool ExpandLegacyRect(long x, long y, long& width, long& height, long targetWidth, long targetHeight) noexcept {
    if (x != 0 || y != 0 || width != kLegacyWidth || height != kLegacyHeight) {
        return false;
    }
    if (targetWidth < kLegacyWidth || targetHeight < kLegacyHeight ||
        (targetWidth == kLegacyWidth && targetHeight == kLegacyHeight)) {
        return false;
    }
    width = targetWidth;
    height = targetHeight;
    return true;
}

void CompensateProjection(float* rows, float scaleX, float scaleY) noexcept {
    // Pixel x = (x/w + 1) / 2 * L : pour retrouver le pixel d'une zone de largeur L * s, x' = s * x + (s - 1) * w.
    // Pixel y = (1 - y/w) / 2 * H : de même, y' = t * y + (1 - t) * w.
    for (int i = 0; i < 4; ++i) {
        const float w = rows[12 + i];
        rows[i] = scaleX * rows[i] + (scaleX - 1.0f) * w;
        rows[4 + i] = scaleY * rows[4 + i] + (1.0f - scaleY) * w;
    }
}

void Install(IDirect3DDevice9* device) {
    if (!GetConfig().fullScreenUiClip || device == nullptr) {
        return;
    }
    // La zone d'affichage en dernier : sans la compensation de projection, l'étendre agrandirait la carte.
    const bool ok = hooking::PatchVtable(device, kSetVertexShaderConstantFIndex, &HookSetVertexShaderConstantF,
                                         &g_setVertexShaderConstantF) &&
                    hooking::PatchVtable(device, kSetScissorRectIndex, &HookSetScissorRect, &g_setScissorRect) &&
                    hooking::PatchVtable(device, kSetViewportIndex, &HookSetViewport, &g_setViewport);
    log::Info("Interface : zone 1280x720 fixe du jeu {}", ok ? "étendue à toute l'image" : "non interceptée");
}

}  // namespace hca::graphics::uiclip
