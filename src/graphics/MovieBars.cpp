#include "graphics/MovieBars.h"

#include <atomic>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "graphics/MovieBarsRules.h"

namespace hca::graphics::moviebars {

namespace {

constexpr size_t kDrawPrimitiveIndex = 81;

using DrawPrimitiveFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);

DrawPrimitiveFn g_drawPrimitive = nullptr;
std::atomic<bool> g_logged{false};

// Description of the texture bound to a stage; false if there is none or it is not a 2D texture.
bool BoundTextureDesc(IDirect3DDevice9* device, DWORD stage, D3DSURFACE_DESC& desc) noexcept {
    IDirect3DBaseTexture9* bound = nullptr;
    if (FAILED(device->GetTexture(stage, &bound)) || bound == nullptr) {
        return false;
    }
    bool ok = false;
    if (bound->GetType() == D3DRTYPE_TEXTURE) {
        ok = SUCCEEDED(static_cast<IDirect3DTexture9*>(bound)->GetLevelDesc(0, &desc));
    }
    bound->Release();
    return ok;
}

// Is the current draw the YUV conversion of a video (luma 1280x720 in stage 0, chroma 640x360 in stages 1 and 2)?
bool IsVideoDraw(IDirect3DDevice9* device) noexcept {
    D3DSURFACE_DESC desc{};
    if (!BoundTextureDesc(device, 1, desc) || !IsChromaPlane(desc.Width, desc.Height, desc.Format)) {
        return false;
    }
    if (!BoundTextureDesc(device, 2, desc) || !IsChromaPlane(desc.Width, desc.Height, desc.Format)) {
        return false;
    }
    return BoundTextureDesc(device, 0, desc) && IsLumaPlane(desc.Width, desc.Height, desc.Format);
}

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

HRESULT STDMETHODCALLTYPE HookDrawPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, UINT startVertex,
                                            UINT primitiveCount) {
    // The video draw is a two-triangle strip: anything else goes straight through.
    if (type != D3DPT_TRIANGLESTRIP || primitiveCount != 2 || !IsVideoDraw(device)) {
        return g_drawPrimitive(device, type, startVertex, primitiveCount);
    }
    long targetWidth = 0;
    long targetHeight = 0;
    Span span;
    D3DVIEWPORT9 viewport{};
    RECT scissor{};
    if (!TargetSize(device, targetWidth, targetHeight) || !CentredPicture(targetWidth, targetHeight, GetConfig().movieAspect, span) ||
        FAILED(device->GetViewport(&viewport)) || FAILED(device->GetScissorRect(&scissor))) {
        return g_drawPrimitive(device, type, startVertex, primitiveCount);
    }
    D3DVIEWPORT9 centred = viewport;
    centred.X = static_cast<DWORD>(span.x);
    centred.Y = 0;
    centred.Width = static_cast<DWORD>(span.width);
    centred.Height = static_cast<DWORD>(targetHeight);
    const RECT centredScissor{span.x, 0, span.x + span.width, targetHeight};
    device->SetViewport(&centred);
    device->SetScissorRect(&centredScissor);
    if (!g_logged.exchange(true)) {
        log::Info("Vidéo : image centrée (largeur max {:.2f}) dans la cible {}x{} (x {}, largeur {}), barres noires de chaque côté",
                  GetConfig().movieAspect, targetWidth, targetHeight, span.x, span.width);
    }
    const HRESULT result = g_drawPrimitive(device, type, startVertex, primitiveCount);
    device->SetViewport(&viewport);
    device->SetScissorRect(&scissor);
    return result;
}

}  // namespace

void Install(IDirect3DDevice9* device) {
    if (!GetConfig().movieBars || device == nullptr) {
        return;
    }
    if (hooking::PatchVtable(device, kDrawPrimitiveIndex, &HookDrawPrimitive, &g_drawPrimitive)) {
        log::Info("Vidéos : barres noires 16:9 sur un tampon plus large (appliqué)");
    } else {
        log::Warn("Vidéos : interception de DrawPrimitive impossible, barres noires non appliquées");
    }
}

}  // namespace hca::graphics::moviebars
