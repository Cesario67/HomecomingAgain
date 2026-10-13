#include "graphics/Ssao.h"

#include <cmath>

#include "core/Config.h"
#include "core/Log.h"
#include "graphics/InternalDraw.h"

// Shaders compilés par fxc pendant la compilation (voir CMakeLists.txt).
#include "ssao_shaders/BlurPS.h"
#include "ssao_shaders/OcclusionPS.h"
#include "ssao_shaders/QuadVS.h"

namespace hca::graphics {

namespace {

constexpr UINT kProjectionRegister = 222;  // mProj (c222..c225), voir Ssao.h

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

void SetPointSampler(IDirect3DDevice9* device, DWORD sampler, IDirect3DBaseTexture9* texture) {
    device->SetTexture(sampler, texture);
    device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, FALSE);
}

// Quadrilatère plein écran décalé d'un demi-pixel (convention Direct3D 9), comme pour le SMAA.
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

Ssao::~Ssao() {
    ReleaseAll();
}

void Ssao::ReleaseAll() noexcept {
    SafeRelease(aoSurface_);
    SafeRelease(aoTex_);
    SafeRelease(quadVs_);
    SafeRelease(occlusionPs_);
    SafeRelease(blurPs_);
    SafeRelease(vertexDeclaration_);
    SafeRelease(savedState_);
    device_ = nullptr;
    width_ = 0;
    height_ = 0;
}

void Ssao::OnBeforeReset() noexcept {
    ReleaseAll();
    // Le moteur recrée sa texture de profondeur après Reset : on l'oublie.
    depthTexture_ = nullptr;
    depthSurface_ = nullptr;
    sceneTexture_ = nullptr;
    depthWritten_ = false;
    failed_ = false;
}

void Ssao::OnTextureCreated(IDirect3DTexture9* texture, UINT width, UINT height, DWORD usage,
                            D3DFORMAT format) noexcept {
    // Profondeur du moteur : cible R32F de la taille de l'écran. L'atlas d'ombres, carré, est aussi en R32F.
    if (texture == nullptr || (usage & D3DUSAGE_RENDERTARGET) == 0 || format != D3DFMT_R32F || width == height) {
        return;
    }
    // Le moteur crée aussi une version en demi-résolution (960x540 en 1080p, constaté le 10/10/2026) : on garde la plus
    // grande, celle de la taille de l'écran.
    if (depthSurface_ != nullptr) {
        D3DSURFACE_DESC current{};
        if (SUCCEEDED(depthSurface_->GetDesc(&current)) && current.Width >= width) {
            return;
        }
    }
    IDirect3DSurface9* surface = nullptr;
    if (SUCCEEDED(texture->GetSurfaceLevel(0, &surface))) {
        depthTexture_ = texture;
        depthSurface_ = surface;
        surface->Release();
        log::Info("SSAO : texture de profondeur du moteur repérée ({}x{})", width, height);
    }
}

void Ssao::OnVertexShaderConstants(UINT startRegister, const float* values, UINT vectorCount) noexcept {
    if (values == nullptr || startRegister > kProjectionRegister ||
        startRegister + vectorCount < kProjectionRegister + 4) {
        return;
    }
    Projection projection;
    if (ParseProjection(values + (kProjectionRegister - startRegister) * 4, projection)) {
        projection_ = projection;
        if (!projectionSeen_) {
            projectionSeen_ = true;
            log::Info("SSAO : projection de la caméra lue (xs {:.4f}, ys {:.4f}, a {:.5f}, b {:.4f})", projection.xScale,
                      projection.yScale, projection.a, projection.b);
        }
    }
}

void Ssao::OnPresent() noexcept {
    depthWritten_ = false;
    applied_ = false;
}

void Ssao::OnSetRenderTarget(IDirect3DDevice9* device, DWORD index, IDirect3DSurface9* surface) noexcept {
    if (index != 0 || surface == nullptr || depthSurface_ == nullptr) {
        return;
    }
    if (surface == depthSurface_) {
        depthWritten_ = true;
        return;
    }
    if (!depthWritten_ || applied_) {
        return;
    }
    D3DSURFACE_DESC desc{};
    if (FAILED(surface->GetDesc(&desc)) || desc.Format != D3DFMT_A16B16G16R16F) {
        return;
    }
    D3DSURFACE_DESC depthDesc{};
    if (FAILED(depthSurface_->GetDesc(&depthDesc)) || desc.Width != depthDesc.Width ||
        desc.Height != depthDesc.Height) {
        return;
    }
    applied_ = true;
    // Fin de la scène 3D, avant les post-traitements et l'interface : point utilisé aussi par le partage des images
    // pour le DLSS (voir SceneReady), que le SSAO soit activé ou non.
    IDirect3DSurface9* current = nullptr;
    if (SUCCEEDED(device->GetRenderTarget(0, &current)) && current != nullptr) {
        IDirect3DTexture9* container = nullptr;
        if (SUCCEEDED(current->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&container))) &&
            container != nullptr) {
            sceneTexture_ = container;
            container->Release();
        }
        current->Release();
    }
    sceneReady_ = true;
    if (GetConfig().ssao && !failed_) {
        Apply(device);
    }
}

bool Ssao::EnsureResources(IDirect3DDevice9* device, UINT width, UINT height) noexcept {
    if (device != device_) {
        ReleaseAll();
        device_ = device;
        const bool created =
            SUCCEEDED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(g_QuadVS), &quadVs_)) &&
            SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_OcclusionPS), &occlusionPs_)) &&
            SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_BlurPS), &blurPs_)) &&
            SUCCEEDED(device->CreateVertexDeclaration(kVertexElements, &vertexDeclaration_)) &&
            SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &savedState_));
        if (!created) {
            log::Error("SSAO : création des shaders impossible, effet désactivé");
            ReleaseAll();
            return false;
        }
    }
    if (width != width_ || height != height_) {
        SafeRelease(aoSurface_);
        SafeRelease(aoTex_);
        if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &aoTex_,
                                         nullptr)) ||
            FAILED(aoTex_->GetSurfaceLevel(0, &aoSurface_))) {
            log::Error("SSAO : création de la cible {}x{} impossible, effet désactivé", width, height);
            SafeRelease(aoTex_);
            return false;
        }
        width_ = width;
        height_ = height;
        log::Info("SSAO actif : {}x{}, rayon {:.2f}, intensité {} %", width, height, GetConfig().ssaoRadius,
                  GetConfig().ssaoStrength);
    }
    return true;
}

bool Ssao::Apply(IDirect3DDevice9* device) noexcept {
    InternalDrawScope internal;
    IDirect3DSurface9* sceneTarget = nullptr;
    if (FAILED(device->GetRenderTarget(0, &sceneTarget))) {
        return false;
    }
    D3DSURFACE_DESC desc{};
    sceneTarget->GetDesc(&desc);
    D3DSURFACE_DESC depthDesc{};
    depthSurface_->GetDesc(&depthDesc);
    // La cible active doit être l'image de la scène : même taille que la profondeur, couleur 8 bits.
    if (desc.Width != depthDesc.Width || desc.Height != depthDesc.Height || desc.Format != D3DFMT_A8R8G8B8) {
        sceneTarget->Release();
        return false;
    }
    if (!EnsureResources(device, desc.Width, desc.Height)) {
        failed_ = true;  // ne pas réessayer à chaque image
        sceneTarget->Release();
        return false;
    }
    IDirect3DTexture9* container = nullptr;
    if (SUCCEEDED(sceneTarget->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&container))) &&
        container != nullptr) {
        sceneTexture_ = container;
        container->Release();
    }
    if (!loggedFirstApply_) {
        loggedFirstApply_ = true;
        log::Info("SSAO : appliqué à l'image de la scène, avant les post-traitements du moteur");
    }

    IDirect3DSurface9* savedDepth = nullptr;
    device->GetDepthStencilSurface(&savedDepth);
    D3DVIEWPORT9 savedViewport{};
    device->GetViewport(&savedViewport);
    savedState_->Capture();

    const Config& config = GetConfig();
    const float projection[4] = {projection_.xScale, projection_.yScale, projection_.a, projection_.b};
    const float screen[4] = {1.0f / static_cast<float>(desc.Width), 1.0f / static_cast<float>(desc.Height),
                             static_cast<float>(desc.Width), static_cast<float>(desc.Height)};
    // Intensité en % : exposant appliqué à l'occlusion (100 % = 1, 200 % = 2...).
    const float params[4] = {config.ssaoRadius, static_cast<float>(config.ssaoStrength) / 100.0f, config.ssaoFadeStart,
                             config.ssaoFadeEnd};

    device->SetDepthStencilSurface(nullptr);
    device->SetVertexDeclaration(vertexDeclaration_);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    device->SetVertexShader(quadVs_);
    device->SetPixelShaderConstantF(0, projection, 1);
    device->SetPixelShaderConstantF(1, screen, 1);
    device->SetPixelShaderConstantF(2, params, 1);
    SetPointSampler(device, 0, depthTexture_);

    // Les dessins doivent être dans une scène ; le moteur est en général déjà dans la sienne (BeginScene échoue alors
    // sans effet, et on ne doit pas la terminer).
    const bool ownScene = SUCCEEDED(device->BeginScene());

    // Passe 1 : occlusion brute dans aoTex_.
    const D3DVIEWPORT9 viewport{0, 0, desc.Width, desc.Height, 0.0f, 1.0f};
    device->SetRenderTarget(0, aoSurface_);
    device->SetViewport(&viewport);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);
    device->SetPixelShader(occlusionPs_);
    DrawQuad(device, desc.Width, desc.Height);

    // Passe 2 : flou, multiplié sur l'image de la scène (ou affiché seul en mode de réglage).
    device->SetRenderTarget(0, sceneTarget);
    device->SetViewport(&viewport);
    SetPointSampler(device, 1, aoTex_);
    if (config.ssaoDebug) {
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    } else {
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
    }
    device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                                                       D3DCOLORWRITEENABLE_BLUE);
    device->SetPixelShader(blurPs_);
    DrawQuad(device, desc.Width, desc.Height);

    if (ownScene) {
        device->EndScene();
    }
    if (diagnosticsLeft_ > 0 && applyCount_++ % 600 == 0) {
        --diagnosticsLeft_;
        LogDiagnostics(device);
    }

    savedState_->Apply();
    device->SetRenderTarget(0, sceneTarget);
    device->SetDepthStencilSurface(savedDepth);
    device->SetViewport(&savedViewport);
    SafeRelease(savedDepth);
    sceneTarget->Release();
    return true;
}

// Relevé ponctuel (premières applications puis toutes les 600) : valeurs de profondeur lues et occlusion obtenue,
// pour vérifier sans capture d'écran que l'effet lit la bonne texture. Copie vers la mémoire système : lent, donc rare.
void Ssao::LogDiagnostics(IDirect3DDevice9* device) noexcept {
    IDirect3DSurface9* depthCopy = nullptr;
    IDirect3DSurface9* aoCopy = nullptr;
    D3DSURFACE_DESC desc{};
    depthSurface_->GetDesc(&desc);
    HRESULT steps[4] = {
        device->CreateOffscreenPlainSurface(desc.Width, desc.Height, D3DFMT_R32F, D3DPOOL_SYSTEMMEM, &depthCopy, nullptr),
        device->CreateOffscreenPlainSurface(desc.Width, desc.Height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &aoCopy, nullptr),
        E_FAIL, E_FAIL};
    if (SUCCEEDED(steps[0]) && SUCCEEDED(steps[1])) {
        steps[2] = device->GetRenderTargetData(depthSurface_, depthCopy);
        steps[3] = device->GetRenderTargetData(aoSurface_, aoCopy);
    }
    if (FAILED(steps[0]) || FAILED(steps[1]) || FAILED(steps[2]) || FAILED(steps[3])) {
        log::Warn("SSAO : relevé de diagnostic impossible (copies 0x{:08X} 0x{:08X}, lectures 0x{:08X} 0x{:08X}, "
                  "profondeur {}x{} format {})",
                  static_cast<unsigned long>(steps[0]), static_cast<unsigned long>(steps[1]),
                  static_cast<unsigned long>(steps[2]), static_cast<unsigned long>(steps[3]), desc.Width, desc.Height,
                  static_cast<int>(desc.Format));
        SafeRelease(depthCopy);
        SafeRelease(aoCopy);
        return;
    }
    D3DLOCKED_RECT depthRect{};
    D3DLOCKED_RECT aoRect{};
    if (SUCCEEDED(depthCopy->LockRect(&depthRect, nullptr, D3DLOCK_READONLY)) &&
        SUCCEEDED(aoCopy->LockRect(&aoRect, nullptr, D3DLOCK_READONLY))) {
        float minDepth = 1e30f;
        float maxDepth = -1e30f;
        int zeros = 0;
        int ones = 0;
        int samples = 0;
        double aoSum = 0.0;
        int aoDark = 0;
        for (UINT y = 0; y < desc.Height; y += 16) {
            const auto* depthRow = reinterpret_cast<const float*>(static_cast<const BYTE*>(depthRect.pBits) +
                                                                  y * depthRect.Pitch);
            // A8R8G8B8 : octets B, G, R, A ; l'occlusion est la même dans les trois couleurs.
            const auto* aoRow = static_cast<const BYTE*>(aoRect.pBits) + y * aoRect.Pitch;
            for (UINT x = 0; x < desc.Width; x += 16) {
                const float d = depthRow[x];
                minDepth = d < minDepth ? d : minDepth;
                maxDepth = d > maxDepth ? d : maxDepth;
                zeros += d <= 0.0001f ? 1 : 0;
                ones += d >= 0.99999f ? 1 : 0;
                aoSum += aoRow[x * 4 + 2];
                aoDark += aoRow[x * 4 + 2] < 64 ? 1 : 0;
                ++samples;
            }
        }
        const float center = reinterpret_cast<const float*>(static_cast<const BYTE*>(depthRect.pBits) +
                                                            (desc.Height / 2) * depthRect.Pitch)[desc.Width / 2];
        log::Info("SSAO diagnostic : profondeur min {:.6f} max {:.6f} centre {:.6f} (zVue {:.1f}), {} % à 0, {} % à 1 ; "
                  "occlusion moyenne {:.2f}, {} % sombre",
                  minDepth, maxDepth, center, projection_.b / (center - projection_.a), zeros * 100 / samples,
                  ones * 100 / samples, aoSum / samples / 255.0, aoDark * 100 / samples);
    }
    depthCopy->UnlockRect();
    aoCopy->UnlockRect();
    SafeRelease(depthCopy);
    SafeRelease(aoCopy);
}

Ssao& GetSsao() noexcept {
    static Ssao instance;
    return instance;
}

}  // namespace hca::graphics
