#include "graphics/Smaa.h"

#include <cstring>

#include "core/Log.h"

// Shaders compilés par fxc pendant la compilation (voir CMakeLists.txt), tableaux « const BYTE g_<point d'entrée> ».
#include "smaa_shaders/BlendingWeightCalculationPS.h"
#include "smaa_shaders/BlendingWeightCalculationVS.h"
#include "smaa_shaders/EdgeDetectionVS.h"
#include "smaa_shaders/LumaEdgeDetectionPS.h"
#include "smaa_shaders/NeighborhoodBlendingPS.h"
#include "smaa_shaders/NeighborhoodBlendingVS.h"

// Tables précalculées de SMAA (licence BSD, third_party/smaa).
#include "AreaTex.h"
#include "SearchTex.h"

namespace shhf::graphics {

namespace {

template <typename T>
void SafeRelease(T*& object) noexcept {
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

// Registres des samplers, identiques à shaders/smaa_d3d9.hlsl.
constexpr DWORD kColorSampler = 0;
constexpr DWORD kEdgesSampler = 1;
constexpr DWORD kAreaSampler = 2;
constexpr DWORD kSearchSampler = 3;
constexpr DWORD kBlendSampler = 4;

constexpr D3DVERTEXELEMENT9 kVertexElements[] = {
    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    D3DDECL_END()};

bool UploadTable(IDirect3DDevice9* device, UINT width, UINT height, D3DFORMAT format, const unsigned char* bytes,
                 UINT pitch, IDirect3DTexture9** texture) noexcept {
    if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, format, D3DPOOL_DEFAULT, texture, nullptr))) {
        return false;
    }
    D3DLOCKED_RECT rect{};
    if (FAILED((*texture)->LockRect(0, &rect, nullptr, D3DLOCK_DISCARD))) {
        return false;
    }
    for (UINT row = 0; row < height; ++row) {
        std::memcpy(static_cast<unsigned char*>(rect.pBits) + row * rect.Pitch, bytes + row * pitch, pitch);
    }
    (*texture)->UnlockRect(0);
    return true;
}

bool CreateTarget(IDirect3DDevice9* device, UINT width, UINT height, D3DFORMAT format, IDirect3DTexture9** texture,
                  IDirect3DSurface9** surface) noexcept {
    return SUCCEEDED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, texture,
                                           nullptr)) &&
           SUCCEEDED((*texture)->GetSurfaceLevel(0, surface));
}

void SetSampler(IDirect3DDevice9* device, DWORD sampler, IDirect3DBaseTexture9* texture, D3DTEXTUREFILTERTYPE filter) {
    device->SetTexture(sampler, texture);
    device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetSamplerState(sampler, D3DSAMP_MINFILTER, filter);
    device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, filter);
    device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, FALSE);
}

// Quadrilatère plein écran, décalé d'un demi-pixel (convention Direct3D 9), comme dans la démonstration SMAA.
void DrawQuad(IDirect3DDevice9* device, UINT width, UINT height) {
    const float px = 1.0f / static_cast<float>(width);
    const float py = 1.0f / static_cast<float>(height);
    const float quad[4][5] = {{-1.0f - px, 1.0f + py, 0.5f, 0.0f, 0.0f},
                              {1.0f - px, 1.0f + py, 0.5f, 1.0f, 0.0f},
                              {-1.0f - px, -1.0f + py, 0.5f, 0.0f, 1.0f},
                              {1.0f - px, -1.0f + py, 0.5f, 1.0f, 1.0f}};
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
}

void SetPass(IDirect3DDevice9* device, IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, UINT width,
             UINT height) {
    device->SetVertexShader(vs);
    device->SetPixelShader(ps);
    const float pixelSize[4] = {1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height), 0.0f, 0.0f};
    device->SetVertexShaderConstantF(0, pixelSize, 1);
    device->SetPixelShaderConstantF(0, pixelSize, 1);
}

}  // namespace

Smaa::~Smaa() {
    ReleaseAll();
}

void Smaa::ReleaseSizedResources() noexcept {
    SafeRelease(colorSurface_);
    SafeRelease(colorTex_);
    SafeRelease(edgeSurface_);
    SafeRelease(edgeTex_);
    SafeRelease(blendSurface_);
    SafeRelease(blendTex_);
    width_ = 0;
    height_ = 0;
}

void Smaa::ReleaseAll() noexcept {
    ReleaseSizedResources();
    SafeRelease(edgeVs_);
    SafeRelease(weightVs_);
    SafeRelease(blendVs_);
    SafeRelease(edgePs_);
    SafeRelease(weightPs_);
    SafeRelease(blendPs_);
    SafeRelease(vertexDeclaration_);
    SafeRelease(areaTex_);
    SafeRelease(searchTex_);
    SafeRelease(savedState_);
    device_ = nullptr;
}

void Smaa::OnBeforeReset() noexcept {
    // Les tables et les cibles sont en D3DPOOL_DEFAULT, le bloc d'états aussi est lié au périphérique : on libère
    // tout, la recréation est rapide.
    ReleaseAll();
    failed_ = false;
}

bool Smaa::EnsureResources(IDirect3DDevice9* device, UINT width, UINT height, D3DFORMAT format) noexcept {
    if (device != device_) {
        ReleaseAll();
        device_ = device;
        const bool created =
            SUCCEEDED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(g_EdgeDetectionVS), &edgeVs_)) &&
            SUCCEEDED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(g_BlendingWeightCalculationVS),
                                                 &weightVs_)) &&
            SUCCEEDED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(g_NeighborhoodBlendingVS), &blendVs_)) &&
            SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_LumaEdgeDetectionPS), &edgePs_)) &&
            SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_BlendingWeightCalculationPS),
                                                &weightPs_)) &&
            SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_NeighborhoodBlendingPS), &blendPs_)) &&
            SUCCEEDED(device->CreateVertexDeclaration(kVertexElements, &vertexDeclaration_)) &&
            UploadTable(device, AREATEX_WIDTH, AREATEX_HEIGHT, D3DFMT_A8L8, areaTexBytes, AREATEX_PITCH, &areaTex_) &&
            UploadTable(device, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, D3DFMT_L8, searchTexBytes, SEARCHTEX_PITCH,
                        &searchTex_) &&
            SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &savedState_));
        if (!created) {
            log::Error("SMAA : création des shaders ou des tables impossible, anticrénelage désactivé");
            ReleaseAll();
            return false;
        }
    }
    if (width != width_ || height != height_ || format != format_) {
        ReleaseSizedResources();
        if (!CreateTarget(device, width, height, format, &colorTex_, &colorSurface_) ||
            !CreateTarget(device, width, height, D3DFMT_A8R8G8B8, &edgeTex_, &edgeSurface_) ||
            !CreateTarget(device, width, height, D3DFMT_A8R8G8B8, &blendTex_, &blendSurface_)) {
            log::Error("SMAA : création des cibles {}x{} (format {}) impossible, anticrénelage désactivé", width, height,
                       static_cast<int>(format));
            ReleaseSizedResources();
            return false;
        }
        width_ = width;
        height_ = height;
        format_ = format;
        log::Info("SMAA actif : {}x{}, préréglage HIGH, contours par luminance", width, height);
    }
    return true;
}

bool Smaa::Apply(IDirect3DDevice9* device) noexcept {
    if (failed_ || device == nullptr) {
        return false;
    }
    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer))) {
        return false;
    }
    D3DSURFACE_DESC desc{};
    backBuffer->GetDesc(&desc);
    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE || !EnsureResources(device, desc.Width, desc.Height, desc.Format)) {
        failed_ = desc.MultiSampleType == D3DMULTISAMPLE_NONE;  // MSAA : on réessaiera si le jeu change de mode
        backBuffer->Release();
        return false;
    }

    // État du jeu : bloc d'états pour tout ce qui est capturable, cibles et profondeur à part.
    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    device->GetRenderTarget(0, &savedTarget);
    device->GetDepthStencilSurface(&savedDepth);
    D3DVIEWPORT9 savedViewport{};
    device->GetViewport(&savedViewport);
    savedState_->Capture();

    // Copie de l'image : la dernière passe lit cette copie et écrit dans le tampon arrière.
    device->StretchRect(backBuffer, nullptr, colorSurface_, nullptr, D3DTEXF_NONE);

    device->SetDepthStencilSurface(nullptr);
    device->SetVertexDeclaration(vertexDeclaration_);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);
    const D3DVIEWPORT9 viewport{0, 0, desc.Width, desc.Height, 0.0f, 1.0f};

    // Les dessins doivent être encadrés par BeginScene/EndScene ; le jeu a déjà appelé EndScene avant Present.
    device->BeginScene();

    // Passe 1 : contours.
    device->SetRenderTarget(0, edgeSurface_);
    device->SetViewport(&viewport);
    device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
    SetSampler(device, kColorSampler, colorTex_, D3DTEXF_LINEAR);
    SetPass(device, edgeVs_, edgePs_, desc.Width, desc.Height);
    DrawQuad(device, desc.Width, desc.Height);

    // Passe 2 : poids de mélange.
    device->SetRenderTarget(0, blendSurface_);
    device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
    SetSampler(device, kEdgesSampler, edgeTex_, D3DTEXF_LINEAR);
    SetSampler(device, kAreaSampler, areaTex_, D3DTEXF_LINEAR);
    SetSampler(device, kSearchSampler, searchTex_, D3DTEXF_POINT);
    SetPass(device, weightVs_, weightPs_, desc.Width, desc.Height);
    DrawQuad(device, desc.Width, desc.Height);

    // Passe 3 : mélange des voisins, vers le tampon arrière.
    device->SetRenderTarget(0, backBuffer);
    SetSampler(device, kColorSampler, colorTex_, D3DTEXF_LINEAR);
    SetSampler(device, kBlendSampler, blendTex_, D3DTEXF_LINEAR);
    SetPass(device, blendVs_, blendPs_, desc.Width, desc.Height);
    DrawQuad(device, desc.Width, desc.Height);

    device->EndScene();

    // Restauration. SetRenderTarget remet la fenêtre d'affichage à toute la cible : on la rétablit ensuite.
    savedState_->Apply();
    device->SetRenderTarget(0, savedTarget);
    device->SetDepthStencilSurface(savedDepth);
    device->SetViewport(&savedViewport);
    SafeRelease(savedTarget);
    SafeRelease(savedDepth);
    backBuffer->Release();
    return true;
}

}  // namespace shhf::graphics
