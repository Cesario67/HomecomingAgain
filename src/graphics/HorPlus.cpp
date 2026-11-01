#include "graphics/HorPlus.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "graphics/HorPlusRules.h"
#include "graphics/UiClip.h"

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
    if (viewport != nullptr && Active() && viewport->X == 0 && viewport->Y == 0 &&
        !(GetConfig().fullScreenUiClip && IsLegacyInterfaceArea(0, 0, viewport->Width, viewport->Height))) {
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
    if (rect != nullptr && Active() && rect->left == 0 && rect->top == 0 &&
        !(GetConfig().fullScreenUiClip && IsLegacyInterfaceArea(0, 0, rect->right, rect->bottom))) {
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

// The pause menu dims the whole picture with a full-screen interface quad textured with a 32x32 image (captured
// 28/10/2026, ortho projection 2/1280 x 2/720). Compressed to the central 16:9 like the rest of the interface, it left
// the sides of an ultrawide picture undimmed. Hypothesis: any 32x32 texture on a full-screen interface quad is such a
// veil; checked on the pause menu only.
bool BoundTextureIsOverlay(IDirect3DDevice9* device) noexcept {
    IDirect3DBaseTexture9* bound = nullptr;
    if (FAILED(device->GetTexture(0, &bound)) || bound == nullptr) {
        return false;
    }
    bool overlay = false;
    if (bound->GetType() == D3DRTYPE_TEXTURE) {
        D3DSURFACE_DESC desc{};
        overlay = SUCCEEDED(static_cast<IDirect3DTexture9*>(bound)->GetLevelDesc(0, &desc)) && desc.Width == 32 &&
                  desc.Height == 32;
    }
    bound->Release();
    return overlay;
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
    // Inside the fixed 1280x720 area (map, examined objects) the layout is the interface fix's business (UiClip).
    if (world && (uiclip::LegacyAreaActive() || BoundTextureIsOverlay(device))) {
        return g_setVertexShaderConstantF(device, start, data, count);
    }
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

// The engine camera (g_SilentHill.sgl) keeps its inverse aspect in [camera+0x94] (0.5625 = 1080 / 1920). Verified on
// 28/10/2026: a hardware write watchpoint on the projection matrix led to 0x10B29100, which builds it (m00 = [camera+0x94]
// / tan, m11 = 1 / tan), and the disassembly shows the frustum builder at 0x10B2B6E0 reading the same field; a setter
// (0x10B2952C) rewrites it as height / width of the view rectangle the engine passes (1920x1080), which is why patching
// the constant 0x1109615C read by the constructor had no effect.
// The three reads (0x10B29209 and 0x10B292A6 in the projection, 0x10B2B75A in the frustum) are redirected to a value of
// ours, height / width of the back buffer: the projection, the culling frustum and the matrices sent to the GPU then
// agree, whenever the camera was built. Each 8-byte `movss xmm,[reg+94h]` becomes `movss xmm,[absolute]`.
float g_engineInverseAspect = 0.5625f;
constexpr float kOriginalInverseAspect = 0.5625f;
std::atomic<bool> g_enginePatched{false};

float OriginalAspectRestoreFactor() noexcept {
    return g_enginePatched.load() ? kOriginalInverseAspect / g_engineInverseAspect : 1.0f;
}

struct AspectRead {
    uintptr_t rva;
    uint8_t original[8];
    uint8_t modrm;  // modrm byte of the replacement (absolute address, same destination register)
};

void PatchEngineAspect(unsigned width, unsigned height) noexcept {
    constexpr AspectRead kReads[] = {
        {0xB29209, {0xF3, 0x0F, 0x10, 0xBE, 0x94, 0x00, 0x00, 0x00}, 0x3D},  // movss xmm7,[esi+94h]
        {0xB292A6, {0xF3, 0x0F, 0x10, 0xB6, 0x94, 0x00, 0x00, 0x00}, 0x35},  // movss xmm6,[esi+94h]
        {0xB2B75A, {0xF3, 0x0F, 0x10, 0x93, 0x94, 0x00, 0x00, 0x00}, 0x15},  // movss xmm2,[ebx+94h]
    };
    const HMODULE module = GetModuleHandleW(L"g_SilentHill.sgl");
    if (module == nullptr) {
        log::Warn("Hor+ moteur : g_SilentHill.sgl introuvable");
        return;
    }
    const auto base = reinterpret_cast<uintptr_t>(module);
    // All three must match before any is changed.
    for (const AspectRead& read : kReads) {
        if (std::memcmp(reinterpret_cast<const void*>(base + read.rva), read.original, sizeof(read.original)) != 0) {
            log::Warn("Hor+ moteur : code inattendu en g_SilentHill.sgl+0x{:X}, aucun changement", read.rva);
            return;
        }
    }
    g_engineInverseAspect = static_cast<float>(height) / static_cast<float>(width);
    // The interface camera uses the same code with an inverse aspect of 1.0 (measured 28/10/2026: its orthographic x scale
    // fell to 0.42 of its value when every read was redirected, a 810 px wide menu). Only the world cameras (0.5625) must
    // be redirected, so each read goes through a small stub: load the field, and if it equals 0.5625 use ours instead.
    //     pushfd ; movss X,[R+94h] ; ucomiss X,[kOriginal] ; jne skip ; jp skip ; movss X,[ours] ; skip: popfd ; ret
    static const float kOriginalAspect = 0.5625f;
    auto* stubs = static_cast<uint8_t*>(VirtualAlloc(nullptr, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (stubs == nullptr) {
        log::Warn("Hor+ moteur : mémoire exécutable indisponible, aucun changement");
        return;
    }
    const auto originalAddress = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&kOriginalAspect));
    const auto oursAddress = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_engineInverseAspect));
    size_t index = 0;
    for (const AspectRead& read : kReads) {
        uint8_t* stub = stubs + 32 * index++;
        const uint8_t load = read.original[3];  // movss X,[R+disp32] : same destination register and base register
        const uint8_t absolute = read.modrm;    // same destination register, absolute address
        const uint8_t bytes[] = {0x9C,
                                 0xF3, 0x0F, 0x10, load, 0x94, 0x00, 0x00, 0x00,
                                 0x0F, 0x2E, absolute, 0, 0, 0, 0,
                                 0x75, 0x0A,
                                 0x7A, 0x08,
                                 0xF3, 0x0F, 0x10, absolute, 0, 0, 0, 0,
                                 0x9D,
                                 0xC3};
        std::memcpy(stub, bytes, sizeof(bytes));
        std::memcpy(stub + 12, &originalAddress, sizeof(originalAddress));
        std::memcpy(stub + 24, &oursAddress, sizeof(oursAddress));
        auto* code = reinterpret_cast<uint8_t*>(base + read.rva);
        DWORD previous = 0;
        if (!VirtualProtect(code, 8, PAGE_EXECUTE_READWRITE, &previous)) {
            log::Warn("Hor+ moteur : VirtualProtect a échoué en g_SilentHill.sgl+0x{:X}", read.rva);
            return;
        }
        const auto relative = static_cast<int32_t>(reinterpret_cast<uintptr_t>(stub) - (reinterpret_cast<uintptr_t>(code) + 5));
        uint8_t patched[8] = {0xE8, 0, 0, 0, 0, 0x0F, 0x1F, 0x00};  // call stub ; nop (3 bytes)
        std::memcpy(patched + 1, &relative, sizeof(relative));
        std::memcpy(code, patched, sizeof(patched));
        VirtualProtect(code, 8, previous, &previous);
        FlushInstructionCache(GetCurrentProcess(), code, 8);
    }
    FlushInstructionCache(GetCurrentProcess(), stubs, 128);
    g_enginePatched = true;
    log::Info("Hor+ moteur : lectures du ratio des caméras du monde redirigées, inverse du ratio {:.6f}",
              g_engineInverseAspect);
}

void SetBackBufferSize(unsigned width, unsigned height) noexcept {
    g_backWidth = static_cast<long>(width);
    g_backHeight = static_cast<long>(height);
    static std::atomic<bool> patched{false};
    if (GetConfig().horPlus && GetConfig().horPlusEngine && Active() && !patched.exchange(true)) {
        PatchEngineAspect(width, height);
    }
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
