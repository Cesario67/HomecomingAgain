#include "graphics/UiClip.h"

#include <atomic>
#include <cmath>
#include <cstring>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "graphics/HorPlus.h"

namespace hca::graphics::uiclip {

namespace {

constexpr size_t kSetViewportIndex = 47;
constexpr size_t kSetScissorRectIndex = 75;
constexpr size_t kSetVertexShaderConstantFIndex = 94;
constexpr size_t kSetPixelShaderConstantFIndex = 109;
constexpr long kLegacyWidth = 1280;
constexpr long kLegacyHeight = 720;

using SetViewportFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const D3DVIEWPORT9*);
using SetScissorRectFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*);
using SetVertexShaderConstantFFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, const float*, UINT);
using SetPixelShaderConstantFFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, const float*, UINT);

SetViewportFn g_setViewport = nullptr;
SetScissorRectFn g_setScissorRect = nullptr;
SetVertexShaderConstantFFn g_setVertexShaderConstantF = nullptr;
SetPixelShaderConstantFFn g_setPixelShaderConstantF = nullptr;

// Zone 1280x720 étendue en vigueur : rapports 1280 / largeur et 720 / hauteur de la cible (thread de rendu seul).
bool g_active = false;
float g_scaleX = 1.0f;
float g_scaleY = 1.0f;
std::atomic<bool> g_logged{false};
std::atomic<bool> g_loggedPartial{false};
std::atomic<bool> g_loggedObject{false};
std::atomic<bool> g_loggedLight{false};
long g_targetWidth = 0;
long g_targetHeight = 0;

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
        g_targetWidth = targetWidth;
        g_targetHeight = targetHeight;
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
    // La carte (vue de dessus) garde ses pixels 720p ; l'objet examiné, dessiné dans la même zone, est recentré à
    // l'échelle de toute l'image (voir UiClip.h).
    if (g_active && data != nullptr && start == 0 && count >= 4) {
        float rows[16];
        std::memcpy(rows, data, sizeof(rows));
        // The Hor+ engine redirect also reached these cameras: give them back their original aspect first.
        ScaleXRow(rows, horplus::OriginalAspectRestoreFactor());
        if (LooksStraightDown(data)) {
            CompensateProjection(rows, g_scaleX, g_scaleY);
        } else {
            // Objet examiné : on retire l'échelle h / 720 du modèle (perspective), puis on le replace sur l'axe.
            if (GetConfig().objectViewPerspective && RestoreObjectPerspective(rows, 1.0f / g_scaleY) &&
                !g_loggedObject.exchange(true)) {
                log::Info("Interface : perspective de la vue d'inspection des objets rétablie (échelle {:.3f})",
                          1.0f / g_scaleY);
            }
            RecenterProjection(rows, g_scaleX, g_scaleY);
        }
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

// Position of the first light (mLightPos_DPMin[0], c20) of the documents and photos shader: see
// RestoreInspectionLight. Only inside the extended 1280x720 area.
HRESULT STDMETHODCALLTYPE HookSetPixelShaderConstantF(IDirect3DDevice9* device, UINT start, const float* data,
                                                      UINT count) {
    constexpr UINT kLightPositionRegister = 20;
    if (g_active && data != nullptr && start == kLightPositionRegister && count >= 1 &&
        GetConfig().objectViewPerspective) {
        float rows[64];
        const UINT copied = count < 16 ? count : 16;
        std::memcpy(rows, data, copied * 4 * sizeof(float));
        if (RestoreInspectionLight(rows, g_targetWidth, g_targetHeight)) {
            if (!g_loggedLight.exchange(true)) {
                log::Info("Interface : projecteur des documents et photos recentré (c20 ramenée à x = y = 0)");
            }
            if (count <= 16) {
                return g_setPixelShaderConstantF(device, start, rows, count);
            }
            const HRESULT first = g_setPixelShaderConstantF(device, start, rows, 16);
            return FAILED(first) ? first : g_setPixelShaderConstantF(device, start + 16, data + 64, count - 16);
        }
    }
    return g_setPixelShaderConstantF(device, start, data, count);
}

}  // namespace

bool LegacyAreaActive() noexcept {
    return g_active;
}

void ScaleXRow(float* rows, float factor) noexcept {
    if (factor == 1.0f) {
        return;
    }
    for (int i = 0; i < 4; ++i) {
        rows[i] *= factor;
    }
}

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

bool LooksStraightDown(const float* rows) noexcept {
    // Ligne w = profondeur devant la caméra. Carte : [0 -1 0 d] (relevé du 16/10/2026, caméra verticale). Objet
    // examiné : [0.94 -1.16 0.06 d] (relevé du 18/10/2026).
    const float x = rows[12];
    const float y = rows[13];
    const float z = rows[14];
    return y < 0.0f && std::fabs(x) <= 0.001f * -y && std::fabs(z) <= 0.001f * -y;
}

void RecenterProjection(float* rows, float scaleX, float scaleY) noexcept {
    // Relevés F11 du 18/10/2026 (couteau et trousse, 720p natif puis 1080p) : même projection aux deux résolutions
    // (la matrice 1080p est entière multipliée par 1,5, sans effet), seule la position de l'objet change. En 1080p, il
    // est placé dans la direction x/w = 1/scaleX - 1, y/w = 1 - 1/scaleY (le centre de la vraie image vu dans la zone
    // 1280x720, hypothèse sur la cause), donc vu de biais. On le déplace en 3D sur l'axe de la caméra : seule la
    // colonne de position change (déplacement de l'objet), pas son orientation ni la perspective.
    const float depth = rows[15];
    rows[3] -= (1.0f / scaleX - 1.0f) * depth;
    rows[7] += (1.0f / scaleY - 1.0f) * depth;
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

bool RestoreObjectPerspective(float* rows, float scale) noexcept {
    if (scale <= 1.01f) {
        return false;
    }
    const float norm = std::sqrt(rows[12] * rows[12] + rows[13] * rows[13] + rows[14] * rows[14]);
    // La carte porte la même échelle (1,5 en 1080p, relevé du 16/10/2026) mais à une distance de 50 : elle est jugée
    // correcte par le joueur et reste intacte. Les objets inspectés sont à 10 - 17 (relevés du 18 et 20/10/2026).
    constexpr float kObjectMaxDistance = 30.0f;
    if (std::fabs(norm - scale) > 0.01f * scale || rows[15] <= 0.0f || rows[15] >= kObjectMaxDistance) {
        return false;
    }
    // Quatre lignes : seul le bloc 3x3 est remis à l'échelle, pas la constante de translation (la matrice redevient celle du 720p).
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 3; ++column) {
            rows[row * 4 + column] /= scale;
        }
    }
    return true;
}

bool RestoreInspectionLight(float* position, long targetWidth, long targetHeight) noexcept {
    // 28.8: measured constant (640 / 22.222 and 360 / 12.5), see UiClip.h.
    constexpr float kPixelsPerUnit = 28.8f;
    const float offsetX = -static_cast<float>(targetWidth - kLegacyWidth) / kPixelsPerUnit;
    const float offsetY = static_cast<float>(targetHeight - kLegacyHeight) / kPixelsPerUnit;
    if (offsetX >= -0.01f || offsetY <= 0.01f) {
        return false;
    }
    if (std::fabs(position[0] - offsetX) > 0.01f * std::fabs(offsetX) ||
        std::fabs(position[1] - offsetY) > 0.01f * offsetY) {
        return false;
    }
    position[0] = 0.0f;
    position[1] = 0.0f;
    return true;
}

void Install(IDirect3DDevice9* device) {
    if (!GetConfig().fullScreenUiClip || device == nullptr) {
        return;
    }
    // La zone d'affichage en dernier : sans la compensation de projection, l'étendre agrandirait la carte.
    const bool ok = hooking::PatchVtable(device, kSetVertexShaderConstantFIndex, &HookSetVertexShaderConstantF,
                                         &g_setVertexShaderConstantF) &&
                    hooking::PatchVtable(device, kSetPixelShaderConstantFIndex, &HookSetPixelShaderConstantF,
                                         &g_setPixelShaderConstantF) &&
                    hooking::PatchVtable(device, kSetScissorRectIndex, &HookSetScissorRect, &g_setScissorRect) &&
                    hooking::PatchVtable(device, kSetViewportIndex, &HookSetViewport, &g_setViewport);
    log::Info("Interface : zone 1280x720 fixe du jeu {}", ok ? "étendue à toute l'image" : "non interceptée");
}

}  // namespace hca::graphics::uiclip
