#include "graphics/ClipCopy.h"

#include <atomic>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "graphics/ClipCopyRules.h"

namespace hca::graphics::clipcopy {

namespace {

constexpr size_t kStretchRectIndex = 34;  // IDirect3DDevice9::StretchRect (vtable order of d3d9.h, IUnknown included)

using StretchRectFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*,
                                                  IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE);

StretchRectFn g_stretchRect = nullptr;
std::atomic<bool> g_logged{false};

HRESULT RetryClipped(IDirect3DDevice9* device, IDirect3DSurface9* source, const RECT* sourceRect,
                     IDirect3DSurface9* dest, const RECT* destRect, D3DTEXTUREFILTERTYPE filter,
                     HRESULT original) noexcept {
    if (destRect == nullptr) {
        return original;
    }
    D3DSURFACE_DESC sourceDesc{};
    D3DSURFACE_DESC destDesc{};
    if (FAILED(source->GetDesc(&sourceDesc)) || FAILED(dest->GetDesc(&destDesc))) {
        return original;
    }
    Rect clippedSource = sourceRect != nullptr
                             ? Rect{sourceRect->left, sourceRect->top, sourceRect->right, sourceRect->bottom}
                             : Rect{0, 0, static_cast<long>(sourceDesc.Width), static_cast<long>(sourceDesc.Height)};
    Rect clippedDest{destRect->left, destRect->top, destRect->right, destRect->bottom};
    if (!ClipToSurface(clippedSource, clippedDest, static_cast<long>(destDesc.Width),
                       static_cast<long>(destDesc.Height))) {
        return original;
    }
    const RECT newSource{clippedSource.left, clippedSource.top, clippedSource.right, clippedSource.bottom};
    const RECT newDest{clippedDest.left, clippedDest.top, clippedDest.right, clippedDest.bottom};
    const HRESULT result = g_stretchRect(device, source, &newSource, dest, &newDest, filter);
    if (!g_logged.exchange(true)) {
        log::Info("Copie finale rognée : destination ({},{},{},{}) -> ({},{},{},{}) sur {}x{}, résultat 0x{:08X}",
                  destRect->left, destRect->top, destRect->right, destRect->bottom, newDest.left, newDest.top,
                  newDest.right, newDest.bottom, destDesc.Width, destDesc.Height, static_cast<unsigned long>(result));
    }
    return SUCCEEDED(result) ? result : original;
}

HRESULT STDMETHODCALLTYPE HookStretchRect(IDirect3DDevice9* device, IDirect3DSurface9* source, const RECT* sourceRect,
                                          IDirect3DSurface9* dest, const RECT* destRect,
                                          D3DTEXTUREFILTERTYPE filter) {
    const HRESULT result = g_stretchRect(device, source, sourceRect, dest, destRect, filter);
    if (SUCCEEDED(result) || source == nullptr || dest == nullptr) {
        return result;
    }
    try {
        return RetryClipped(device, source, sourceRect, dest, destRect, filter, result);
    } catch (...) {
        return result;
    }
}

}  // namespace

void Install(IDirect3DDevice9* device) {
    if (!GetConfig().clipFinalCopy) {
        return;
    }
    if (hooking::PatchVtable(device, kStretchRectIndex, &HookStretchRect, &g_stretchRect)) {
        log::Info("Copie finale : rectangle de destination rogné si StretchRect le refuse (hors de la surface)");
    } else {
        log::Warn("Copie finale : interception de StretchRect impossible");
    }
}

}  // namespace hca::graphics::clipcopy
