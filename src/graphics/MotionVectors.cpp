#include "graphics/MotionVectors.h"

#include <array>
#include <cmath>
#include <cstdint>

#include "core/Log.h"
#include "graphics/CameraTracker.h"
#include "graphics/InternalDraw.h"
#include "graphics/DepthEncoding.h"

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

std::array<double, 4> Transform(const Matrix4& m, const std::array<double, 4>& v) noexcept {
    std::array<double, 4> r{};
    for (int row = 0; row < 4; ++row) {
        r[row] = m[row * 4] * v[0] + m[row * 4 + 1] * v[1] + m[row * 4 + 2] * v[2] + m[row * 4 + 3] * v[3];
    }
    return r;
}

float HalfToFloat(std::uint16_t h) noexcept {
    const std::uint32_t sign = (h >> 15) & 1u;
    const std::uint32_t exponent = (h >> 10) & 0x1Fu;
    const std::uint32_t mantissa = h & 0x3FFu;
    float value = 0.0f;
    if (exponent == 0) {
        value = std::ldexp(static_cast<float>(mantissa), -24);
    } else if (exponent == 31) {
        value = mantissa == 0 ? INFINITY : NAN;
    } else {
        value = std::ldexp(static_cast<float>(mantissa | 0x400u), static_cast<int>(exponent) - 25);
    }
    return sign != 0 ? -value : value;
}

// Copie d'une texture de rendu (niveau 0) en mémoire système ; nullptr en cas d'échec.
IDirect3DSurface9* ReadBack(IDirect3DDevice9* device, IDirect3DTexture9* texture) noexcept {
    IDirect3DSurface9* source = nullptr;
    if (FAILED(texture->GetSurfaceLevel(0, &source))) {
        return nullptr;
    }
    D3DSURFACE_DESC desc{};
    source->GetDesc(&desc);
    IDirect3DSurface9* copy = nullptr;
    if (FAILED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &copy,
                                                   nullptr)) ||
        FAILED(device->GetRenderTargetData(source, copy))) {
        SafeRelease(copy);
    }
    SafeRelease(source);
    return copy;
}

// Diagnostic des vecteurs faux (13/10/2026) : toutes les 300 images, relit la profondeur et les vecteurs produits par le
// shader en cinq points de l'écran et les compare au même calcul fait sur le processeur (en double) avec les mêmes
// matrices et la même profondeur. Lent (copie de la carte vers la mémoire), d'où la fréquence réduite.
void LogSampledMotion(IDirect3DDevice9* device, IDirect3DTexture9* depth, IDirect3DTexture9* motion,
                      const Matrix4& previous, const Matrix4& inverseCurrent, const float* depthDecode, UINT width,
                      UINT height) noexcept {
    static int counter = 0;
    if (++counter % 300 != 0) {
        return;
    }
    IDirect3DSurface9* depthCopy = ReadBack(device, depth);
    IDirect3DSurface9* motionCopy = ReadBack(device, motion);
    D3DLOCKED_RECT depthRect{};
    D3DLOCKED_RECT motionRect{};
    if (depthCopy != nullptr && motionCopy != nullptr &&
        SUCCEEDED(depthCopy->LockRect(&depthRect, nullptr, D3DLOCK_READONLY))) {
        if (SUCCEEDED(motionCopy->LockRect(&motionRect, nullptr, D3DLOCK_READONLY))) {
            // Position de la caméra : le point dont l'image a x = y = w = 0, soit inverse(caméra) x (0, 0, 1, 0) une fois
            // ramené en coordonnées homogènes.
            Matrix4 inversePrevious{};
            if (Inverse(previous, inversePrevious)) {
                std::array<double, 4> now = Transform(inverseCurrent, {0.0, 0.0, 1.0, 0.0});
                std::array<double, 4> before = Transform(inversePrevious, {0.0, 0.0, 1.0, 0.0});
                log::Info("Vecteurs : caméra en ({:.2f} {:.2f} {:.2f}), avant ({:.2f} {:.2f} {:.2f}), déplacement {:.2f}",
                          now[0] / now[3], now[1] / now[3], now[2] / now[3], before[0] / before[3],
                          before[1] / before[3], before[2] / before[3],
                          std::sqrt(std::pow(now[0] / now[3] - before[0] / before[3], 2) +
                                    std::pow(now[1] / now[3] - before[1] / before[3], 2) +
                                    std::pow(now[2] / now[3] - before[2] / before[3], 2)));
            }
            // Échelle de la projection (champ de vision) des deux caméras : longueur des lignes 0 et 1 sur x, y, z.
            const auto rowLength = [](const Matrix4& m, int row) {
                return std::sqrt(m[row * 4] * m[row * 4] + m[row * 4 + 1] * m[row * 4 + 1] +
                                 m[row * 4 + 2] * m[row * 4 + 2]);
            };
            Matrix4 current{};
            if (Inverse(inverseCurrent, current)) {
                log::Info("Vecteurs : échelle de projection {:.4f} x {:.4f}, avant {:.4f} x {:.4f}", rowLength(current, 0),
                          rowLength(current, 1), rowLength(previous, 0), rowLength(previous, 1));
            }
            constexpr double kPoints[5][2] = {{0.5, 0.5}, {0.1, 0.1}, {0.9, 0.1}, {0.1, 0.9}, {0.9, 0.9}};
            for (const auto& point : kPoints) {
                const UINT x = static_cast<UINT>(point[0] * width);
                const UINT y = static_cast<UINT>(point[1] * height);
                const float z = reinterpret_cast<const float*>(static_cast<const std::uint8_t*>(depthRect.pBits) +
                                                               y * depthRect.Pitch)[x];
                const std::uint16_t* m = reinterpret_cast<const std::uint16_t*>(
                                             static_cast<const std::uint8_t*>(motionRect.pBits) + y * motionRect.Pitch) +
                                         x * 2;
                const double u = (x + 0.5) / width;
                const double v = (y + 0.5) / height;
                const double decoded = std::fmin((z - depthDecode[1]) / depthDecode[0], depthDecode[2]);
                std::array<double, 4> world = Transform(inverseCurrent, {u * 2.0 - 1.0, 1.0 - v * 2.0, decoded, 1.0});
                const double w = world[3];
                for (double& c : world) {
                    c /= w;
                }
                const std::array<double, 4> before = Transform(previous, world);
                const double cx = ((before[0] / before[3]) * 0.5 + 0.5 - u) * width;
                const double cy = (0.5 - (before[1] / before[3]) * 0.5 - v) * height;
                log::Info("Vecteurs ({:.1f}, {:.1f}) : profondeur {:.6f}, monde ({:.1f} {:.1f} {:.1f}), shader ({:.2f}, "
                          "{:.2f}) px, processeur ({:.2f}, {:.2f}) px",
                          point[0], point[1], z, world[0], world[1], world[2], HalfToFloat(m[0]), HalfToFloat(m[1]), cx,
                          cy);
            }
            motionCopy->UnlockRect();
        }
        depthCopy->UnlockRect();
    } else {
        log::Warn("Vecteurs : relecture impossible");
    }
    SafeRelease(depthCopy);
    SafeRelease(motionCopy);
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
    // Décodage de la profondeur (graphics/DepthEncoding.h) et z/w à l'infini : le a de la projection, rapport des
    // longueurs des lignes 2 et 3 de la caméra (ligne 2 = a x ligne 3 + (0, 0, 0, b)).
    const depth::Encoding encoding = depth::Current();
    const float row2 = std::sqrt(current[8] * current[8] + current[9] * current[9] + current[10] * current[10]);
    const float row3 = std::sqrt(current[12] * current[12] + current[13] * current[13] + current[14] * current[14]);
    const float depthDecode[4] = {encoding.scale, encoding.bias, row3 > 0.0f ? row2 / row3 : 1.0f, 0.0f};
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
    device->SetPixelShaderConstantF(9, depthDecode, 1);
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
    LogSampledMotion(device, depth, target_, previous, inverseCurrent, depthDecode, desc.Width, desc.Height);
    return target_;
}

MotionVectors& GetMotionVectors() noexcept {
    static MotionVectors instance;
    return instance;
}

}  // namespace hca::graphics
