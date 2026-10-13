#include "graphics/MotionVectors.h"

#include "core/Log.h"
#include "graphics/CameraTracker.h"
#include "graphics/InternalDraw.h"

// Shaders compilés par fxc pendant la compilation (voir CMakeLists.txt).
#include "motion_shaders/MotionPS.h"
#include "motion_shaders/MotionQuadVS.h"

namespace hca::graphics {

namespace {

template <typename T>
void SafeRelease(T*& object) noexcept {
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

constexpr D3DVERTEXELEMENT9 kVertexElements[] = {
    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    D3DDECL_END()};

// Quadrilatère plein écran décalé d'un demi-pixel (convention Direct3D 9), comme pour le SMAA et le SSAO.
void DrawQuad(IDirect3DDevice9* device, UINT width, UINT height) {
    const float px = 1.0f / static_cast<float>(width);
    const float py = 1.0f / static_cast<float>(height);
    const float quad[4][5] = {{-1.0f - px, 1.0f + py, 0.5f, 0.0f, 0.0f},
                              {1.0f - px, 1.0f + py, 0.5f, 1.0f, 0.0f},
                              {-1.0f - px, -1.0f + py, 0.5f, 0.0f, 1.0f},
                              {1.0f - px, -1.0f + py, 0.5f, 1.0f, 1.0f}};
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
}

}  // namespace

MotionVectors::~MotionVectors() {
    ReleaseAll();
}

void MotionVectors::ReleaseAll() noexcept {
    SafeRelease(targetSurface_);
    SafeRelease(target_);
    SafeRelease(quadVs_);
    SafeRelease(motionPs_);
    SafeRelease(vertexDeclaration_);
    SafeRelease(savedState_);
    device_ = nullptr;
    width_ = 0;
    height_ = 0;
}

void MotionVectors::OnBeforeReset() noexcept {
    ReleaseAll();
    failed_ = false;
}

bool MotionVectors::EnsureResources(IDirect3DDevice9* device, UINT width, UINT height) noexcept {
    if (device != device_) {
        ReleaseAll();
        device_ = device;
        const bool created =
            SUCCEEDED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(g_MotionQuadVS), &quadVs_)) &&
            SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_MotionPS), &motionPs_)) &&
            SUCCEEDED(device->CreateVertexDeclaration(kVertexElements, &vertexDeclaration_)) &&
            SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &savedState_));
        if (!created) {
            log::Error("Vecteurs de mouvement : création des shaders impossible");
            ReleaseAll();
            return false;
        }
    }
    if (width != width_ || height != height_) {
        SafeRelease(targetSurface_);
        SafeRelease(target_);
        if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_G16R16F, D3DPOOL_DEFAULT,
                                         &target_, nullptr)) ||
            FAILED(target_->GetSurfaceLevel(0, &targetSurface_))) {
            log::Error("Vecteurs de mouvement : cible G16R16F {}x{} impossible", width, height);
            SafeRelease(target_);
            return false;
        }
        width_ = width;
        height_ = height;
    }
    return true;
}

IDirect3DTexture9* MotionVectors::Render(IDirect3DDevice9* device, IDirect3DTexture9* depth) noexcept {
    Matrix4 current{};
    Matrix4 previous{};
    Matrix4 inverseCurrent{};
    if (failed_ || device == nullptr || depth == nullptr || !camera::Current(current, previous) ||
        !Inverse(current, inverseCurrent)) {
        return nullptr;
    }
    D3DSURFACE_DESC desc{};
    if (FAILED(depth->GetLevelDesc(0, &desc))) {
        return nullptr;
    }
    InternalDrawScope internal;
    if (!EnsureResources(device, desc.Width, desc.Height)) {
        failed_ = true;
        return nullptr;
    }
    if (!logged_) {
        logged_ = true;
        log::Info("Vecteurs de mouvement de la caméra calculés : {}x{}, G16R16F", desc.Width, desc.Height);
    }

    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    device->GetRenderTarget(0, &savedTarget);
    device->GetDepthStencilSurface(&savedDepth);
    D3DVIEWPORT9 savedViewport{};
    device->GetViewport(&savedViewport);
    savedState_->Capture();

    const float screen[4] = {static_cast<float>(desc.Width), static_cast<float>(desc.Height), 0.0f, 0.0f};
    device->SetDepthStencilSurface(nullptr);
    device->SetRenderTarget(0, targetSurface_);
    const D3DVIEWPORT9 viewport{0, 0, desc.Width, desc.Height, 0.0f, 1.0f};
    device->SetViewport(&viewport);
    device->SetVertexDeclaration(vertexDeclaration_);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);
    device->SetVertexShader(quadVs_);
    device->SetPixelShader(motionPs_);
    device->SetPixelShaderConstantF(0, inverseCurrent.data(), 4);
    device->SetPixelShaderConstantF(4, previous.data(), 4);
    device->SetPixelShaderConstantF(8, screen, 1);
    device->SetTexture(0, depth);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

    const bool ownScene = SUCCEEDED(device->BeginScene());
    DrawQuad(device, desc.Width, desc.Height);
    if (ownScene) {
        device->EndScene();
    }

    savedState_->Apply();
    device->SetRenderTarget(0, savedTarget);
    device->SetDepthStencilSurface(savedDepth);
    device->SetViewport(&savedViewport);
    SafeRelease(savedTarget);
    SafeRelease(savedDepth);
    return target_;
}

MotionVectors& GetMotionVectors() noexcept {
    static MotionVectors instance;
    return instance;
}

}  // namespace hca::graphics
