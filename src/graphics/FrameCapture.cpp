#include "graphics/FrameCapture.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Strings.h"
#include "graphics/InternalDraw.h"
#include "graphics/ShaderConstantTable.h"

namespace hca::graphics::capture {

namespace {

// Indices dans la vtable de IDirect3DDevice9 (ordre de déclaration de d3d9.h, IUnknown compris).
constexpr size_t kCreateTextureIndex = 23;
constexpr size_t kCreateRenderTargetIndex = 28;
constexpr size_t kCreateDepthStencilSurfaceIndex = 29;
constexpr size_t kStretchRectIndex = 34;
constexpr size_t kSetRenderTargetIndex = 37;
constexpr size_t kSetDepthStencilSurfaceIndex = 39;
constexpr size_t kBeginSceneIndex = 41;
constexpr size_t kEndSceneIndex = 42;
constexpr size_t kClearIndex = 43;
constexpr size_t kSetTransformIndex = 44;
constexpr size_t kSetViewportIndex = 47;
constexpr size_t kSetRenderStateIndex = 57;
constexpr size_t kSetScissorRectIndex = 75;
constexpr size_t kSetTextureIndex = 65;
constexpr size_t kDrawPrimitiveIndex = 81;
constexpr size_t kDrawIndexedPrimitiveIndex = 82;
constexpr size_t kDrawPrimitiveUPIndex = 83;
constexpr size_t kDrawIndexedPrimitiveUPIndex = 84;
constexpr size_t kSetVertexShaderIndex = 92;
constexpr size_t kSetVertexShaderConstantFIndex = 94;
constexpr size_t kSetPixelShaderIndex = 107;
constexpr size_t kSetPixelShaderConstantFIndex = 109;

constexpr int kFramesPerCapture = 2;

using CreateTextureFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                                    IDirect3DTexture9**, HANDLE*);
using CreateSurfaceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE,
                                                    DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
using StretchRectFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*,
                                                  IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE);
using SetRenderTargetFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);
using SetDepthStencilSurfaceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DSurface9*);
using SceneFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using ClearFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD);
using SetTransformFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
using SetViewportFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const D3DVIEWPORT9*);
using SetScissorRectFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*);
using SetRenderStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD);
using SetTextureFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
using DrawPrimitiveFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
using DrawIndexedPrimitiveFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT,
                                                           UINT);
using DrawPrimitiveUPFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
using DrawIndexedPrimitiveUPFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, UINT,
                                                             const void*, D3DFORMAT, const void*, UINT);
using SetVertexShaderFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DVertexShader9*);
using SetPixelShaderFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DPixelShader9*);
using SetShaderConstantFFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, const float*, UINT);

CreateTextureFn g_createTexture = nullptr;
CreateSurfaceFn g_createRenderTarget = nullptr;
CreateSurfaceFn g_createDepthStencilSurface = nullptr;
StretchRectFn g_stretchRect = nullptr;
SetRenderTargetFn g_setRenderTarget = nullptr;
SetDepthStencilSurfaceFn g_setDepthStencilSurface = nullptr;
SceneFn g_beginScene = nullptr;
SceneFn g_endScene = nullptr;
ClearFn g_clear = nullptr;
SetTransformFn g_setTransform = nullptr;
SetViewportFn g_setViewport = nullptr;
SetScissorRectFn g_setScissorRect = nullptr;
SetRenderStateFn g_setRenderState = nullptr;
SetTextureFn g_setTexture = nullptr;
DrawPrimitiveFn g_drawPrimitive = nullptr;
DrawIndexedPrimitiveFn g_drawIndexedPrimitive = nullptr;
DrawPrimitiveUPFn g_drawPrimitiveUP = nullptr;
DrawIndexedPrimitiveUPFn g_drawIndexedPrimitiveUP = nullptr;
SetVertexShaderFn g_setVertexShader = nullptr;
SetPixelShaderFn g_setPixelShader = nullptr;
SetShaderConstantFFn g_setVertexShaderConstantF = nullptr;
SetShaderConstantFFn g_setPixelShaderConstantF = nullptr;

std::atomic<bool> g_recording{false};

// Tout ce qui suit est protégé par g_mutex. Le relevé est un outil de diagnostic : allocations et écritures de
// fichier y sont acceptées (elles n'ont lieu que pendant les deux images enregistrées).
std::mutex g_mutex;
std::string g_buffer;
std::filesystem::path g_captureDir;
std::unordered_set<const void*> g_savedShaders;
int g_framesLeft = 0;
std::uint32_t g_eventIndex = 0;
bool g_keyWasDown = false;

bool Recording() noexcept {
    return !t_internalDraw && g_recording.load(std::memory_order_relaxed);
}

template <typename... Args>
void Record(std::format_string<Args...> format, Args&&... args) noexcept {
    try {
        std::lock_guard lock(g_mutex);
        if (!g_recording.load(std::memory_order_relaxed)) {
            return;
        }
        g_buffer += std::format("{:6} ", g_eventIndex++);
        g_buffer += std::format(format, std::forward<Args>(args)...);
        g_buffer += '\n';
    } catch (...) {
    }
}

std::string DescribeSurface(IDirect3DSurface9* surface) {
    if (surface == nullptr) {
        return "aucune";
    }
    D3DSURFACE_DESC desc{};
    if (FAILED(surface->GetDesc(&desc))) {
        return std::format("{} (description indisponible)", static_cast<const void*>(surface));
    }
    std::string result = std::format("{} {}x{} format {} usage 0x{:X} msaa {}", static_cast<const void*>(surface),
                                     desc.Width, desc.Height, static_cast<int>(desc.Format), desc.Usage,
                                     static_cast<int>(desc.MultiSampleType));
    IDirect3DTexture9* container = nullptr;
    if (SUCCEEDED(surface->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&container))) &&
        container != nullptr) {
        result += std::format(" (texture {})", static_cast<const void*>(container));
        container->Release();
    }
    return result;
}

std::string DescribeTexture(IDirect3DBaseTexture9* texture) {
    if (texture == nullptr) {
        return "aucune";
    }
    if (texture->GetType() != D3DRTYPE_TEXTURE) {
        return std::format("{} type {}", static_cast<const void*>(texture), static_cast<int>(texture->GetType()));
    }
    D3DSURFACE_DESC desc{};
    if (FAILED(static_cast<IDirect3DTexture9*>(texture)->GetLevelDesc(0, &desc))) {
        return std::format("{}", static_cast<const void*>(texture));
    }
    return std::format("{} {}x{} format {} usage 0x{:X}", static_cast<const void*>(texture), desc.Width, desc.Height,
                       static_cast<int>(desc.Format), desc.Usage);
}

std::string FormatFloats(const float* values, UINT vectorCount) {
    std::string result;
    for (UINT i = 0; i < vectorCount; ++i) {
        result += std::format(" [{:.5g} {:.5g} {:.5g} {:.5g}]", values[i * 4], values[i * 4 + 1], values[i * 4 + 2],
                              values[i * 4 + 3]);
    }
    return result;
}

// Enregistre le bytecode d'un shader la première fois qu'il apparaît dans le relevé, et journalise sa table de
// constantes (noms et registres) si le compilateur l'a laissée.
template <typename Shader>
void SaveShader(Shader* shader, const char* kind) {
    if (shader == nullptr) {
        return;
    }
    {
        std::lock_guard lock(g_mutex);
        if (!g_savedShaders.insert(shader).second) {
            return;
        }
    }
    UINT size = 0;
    if (FAILED(shader->GetFunction(nullptr, &size)) || size == 0 || size % 4 != 0) {
        return;
    }
    std::vector<std::uint32_t> tokens(size / 4);
    if (FAILED(shader->GetFunction(tokens.data(), &size))) {
        return;
    }
    const std::string name = std::format("{}_{:08X}", kind, reinterpret_cast<std::uintptr_t>(shader));
    std::filesystem::path directory;
    {
        std::lock_guard lock(g_mutex);
        directory = g_captureDir;
    }
    std::ofstream(directory / (name + ".bin"), std::ios::binary)
        .write(reinterpret_cast<const char*>(tokens.data()), static_cast<std::streamsize>(size));
    std::string table;
    for (const ShaderConstant& constant : ParseConstantTable(tokens.data(), tokens.size())) {
        static constexpr char kSets[] = {'b', 'i', 'c', 's'};
        const char set = constant.registerSet < 4 ? kSets[constant.registerSet] : '?';
        table += std::format("    {}{}..{}{} {}\n", set, constant.registerIndex, set,
                             constant.registerIndex + constant.registerCount - 1, constant.name);
    }
    std::lock_guard lock(g_mutex);
    std::ofstream(directory / "shaders.txt", std::ios::app)
        << name << " (version 0x" << std::format("{:08X}", tokens[0]) << ", " << size << " octets)\n"
        << (table.empty() ? std::string("    pas de table de constantes\n") : table);
}

// --- Créations de ressources : journalisées en permanence (relevé activé), elles ont lieu au chargement. ---

HRESULT STDMETHODCALLTYPE HookCreateTexture(IDirect3DDevice9* device, UINT width, UINT height, UINT levels, DWORD usage,
                                            D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** texture,
                                            HANDLE* sharedHandle) {
    const HRESULT result = g_createTexture(device, width, height, levels, usage, format, pool, texture, sharedHandle);
    if ((usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) != 0 && !t_internalDraw) {
        log::Info("Relevé : CreateTexture {}x{} format {} usage 0x{:X} -> {} (0x{:08X})", width, height,
                  static_cast<int>(format), usage,
                  static_cast<const void*>(SUCCEEDED(result) && texture != nullptr ? *texture : nullptr),
                  static_cast<unsigned long>(result));
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateRenderTarget(IDirect3DDevice9* device, UINT width, UINT height, D3DFORMAT format,
                                                 D3DMULTISAMPLE_TYPE multisample, DWORD quality, BOOL lockable,
                                                 IDirect3DSurface9** surface, HANDLE* sharedHandle) {
    const HRESULT result =
        g_createRenderTarget(device, width, height, format, multisample, quality, lockable, surface, sharedHandle);
    if (!t_internalDraw) {
        log::Info("Relevé : CreateRenderTarget {}x{} format {} msaa {} -> {}", width, height, static_cast<int>(format),
                  static_cast<int>(multisample),
                  static_cast<const void*>(SUCCEEDED(result) && surface != nullptr ? *surface : nullptr));
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateDepthStencilSurface(IDirect3DDevice9* device, UINT width, UINT height,
                                                        D3DFORMAT format, D3DMULTISAMPLE_TYPE multisample,
                                                        DWORD quality, BOOL discard, IDirect3DSurface9** surface,
                                                        HANDLE* sharedHandle) {
    const HRESULT result = g_createDepthStencilSurface(device, width, height, format, multisample, quality, discard,
                                                       surface, sharedHandle);
    if (!t_internalDraw) {
        log::Info("Relevé : CreateDepthStencilSurface {}x{} format {} msaa {} discard {} -> {}", width, height,
                  static_cast<int>(format), static_cast<int>(multisample), discard,
                  static_cast<const void*>(SUCCEEDED(result) && surface != nullptr ? *surface : nullptr));
    }
    return result;
}

// --- Appels enregistrés pendant un relevé seulement. ---

HRESULT STDMETHODCALLTYPE HookStretchRect(IDirect3DDevice9* device, IDirect3DSurface9* source, const RECT* sourceRect,
                                          IDirect3DSurface9* dest, const RECT* destRect, D3DTEXTUREFILTERTYPE filter) {
    const HRESULT result = g_stretchRect(device, source, sourceRect, dest, destRect, filter);
    if (Recording()) {
        // The result and the rectangles are logged because a scaled copy (source and destination sizes differ) can
        // fail or come out black while an equal-size copy works (ultrawide investigation, 27/10/2026).
        Record("StretchRect {} -> {} filtre {} rects {} {} -> 0x{:08X}", DescribeSurface(source),
               DescribeSurface(dest), static_cast<int>(filter), sourceRect != nullptr ? "source" : "source entière",
               destRect != nullptr ? "destination" : "destination entière", static_cast<unsigned long>(result));
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookSetRenderTarget(IDirect3DDevice9* device, DWORD index, IDirect3DSurface9* surface) {
    if (Recording()) {
        Record("SetRenderTarget {} : {}", index, DescribeSurface(surface));
    }
    return g_setRenderTarget(device, index, surface);
}

HRESULT STDMETHODCALLTYPE HookSetDepthStencilSurface(IDirect3DDevice9* device, IDirect3DSurface9* surface) {
    if (Recording()) {
        Record("SetDepthStencilSurface {}", DescribeSurface(surface));
    }
    return g_setDepthStencilSurface(device, surface);
}

HRESULT STDMETHODCALLTYPE HookBeginScene(IDirect3DDevice9* device) {
    if (Recording()) {
        Record("BeginScene");
    }
    return g_beginScene(device);
}

HRESULT STDMETHODCALLTYPE HookEndScene(IDirect3DDevice9* device) {
    if (Recording()) {
        Record("EndScene");
    }
    return g_endScene(device);
}

HRESULT STDMETHODCALLTYPE HookClear(IDirect3DDevice9* device, DWORD count, const D3DRECT* rects, DWORD flags,
                                    D3DCOLOR color, float z, DWORD stencil) {
    if (Recording()) {
        Record("Clear rects {} options 0x{:X} couleur 0x{:08X} z {} stencil {}", count, flags, color, z, stencil);
    }
    return g_clear(device, count, rects, flags, color, z, stencil);
}

HRESULT STDMETHODCALLTYPE HookSetTransform(IDirect3DDevice9* device, D3DTRANSFORMSTATETYPE state,
                                           const D3DMATRIX* matrix) {
    if (Recording() && matrix != nullptr) {
        Record("SetTransform {}{}", static_cast<int>(state), FormatFloats(&matrix->_11, 4));
    }
    return g_setTransform(device, state, matrix);
}

HRESULT STDMETHODCALLTYPE HookSetScissorRect(IDirect3DDevice9* device, const RECT* rect) {
    if (Recording() && rect != nullptr) {
        Record("SetScissorRect {} {} {} {}", rect->left, rect->top, rect->right, rect->bottom);
    }
    return g_setScissorRect(device, rect);
}

HRESULT STDMETHODCALLTYPE HookSetRenderState(IDirect3DDevice9* device, D3DRENDERSTATETYPE state, DWORD value) {
    if (Recording() && state == D3DRS_SCISSORTESTENABLE) {
        Record("SetRenderState SCISSORTESTENABLE {}", value);
    }
    return g_setRenderState(device, state, value);
}

HRESULT STDMETHODCALLTYPE HookSetViewport(IDirect3DDevice9* device, const D3DVIEWPORT9* viewport) {
    if (Recording() && viewport != nullptr) {
        Record("SetViewport {} {} {}x{} z {}..{}", viewport->X, viewport->Y, viewport->Width, viewport->Height,
               viewport->MinZ, viewport->MaxZ);
    }
    return g_setViewport(device, viewport);
}

HRESULT STDMETHODCALLTYPE HookSetTexture(IDirect3DDevice9* device, DWORD stage, IDirect3DBaseTexture9* texture) {
    if (Recording()) {
        Record("SetTexture {} : {}", stage, DescribeTexture(texture));
    }
    return g_setTexture(device, stage, texture);
}

HRESULT STDMETHODCALLTYPE HookDrawPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, UINT start, UINT count) {
    if (Recording()) {
        Record("DrawPrimitive type {} primitives {}", static_cast<int>(type), count);
    }
    return g_drawPrimitive(device, type, start, count);
}

HRESULT STDMETHODCALLTYPE HookDrawIndexedPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertex,
                                                   UINT minIndex, UINT vertices, UINT startIndex, UINT count) {
    if (Recording()) {
        Record("DrawIndexedPrimitive type {} sommets {} primitives {}", static_cast<int>(type), vertices, count);
    }
    return g_drawIndexedPrimitive(device, type, baseVertex, minIndex, vertices, startIndex, count);
}

HRESULT STDMETHODCALLTYPE HookDrawPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, UINT count,
                                              const void* data, UINT stride) {
    if (Recording()) {
        Record("DrawPrimitiveUP type {} primitives {} pas {}", static_cast<int>(type), count, stride);
    }
    return g_drawPrimitiveUP(device, type, count, data, stride);
}

HRESULT STDMETHODCALLTYPE HookDrawIndexedPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, UINT minIndex,
                                                     UINT vertices, UINT count, const void* indices,
                                                     D3DFORMAT indexFormat, const void* data, UINT stride) {
    if (Recording()) {
        Record("DrawIndexedPrimitiveUP type {} sommets {} primitives {}", static_cast<int>(type), vertices, count);
    }
    return g_drawIndexedPrimitiveUP(device, type, minIndex, vertices, count, indices, indexFormat, data, stride);
}

HRESULT STDMETHODCALLTYPE HookSetVertexShader(IDirect3DDevice9* device, IDirect3DVertexShader9* shader) {
    if (Recording()) {
        Record("SetVertexShader vs_{:08X}", reinterpret_cast<std::uintptr_t>(shader));
        try {
            SaveShader(shader, "vs");
        } catch (...) {
        }
    }
    return g_setVertexShader(device, shader);
}

HRESULT STDMETHODCALLTYPE HookSetPixelShader(IDirect3DDevice9* device, IDirect3DPixelShader9* shader) {
    if (Recording()) {
        Record("SetPixelShader ps_{:08X}", reinterpret_cast<std::uintptr_t>(shader));
        try {
            SaveShader(shader, "ps");
        } catch (...) {
        }
    }
    return g_setPixelShader(device, shader);
}

HRESULT STDMETHODCALLTYPE HookSetVertexShaderConstantF(IDirect3DDevice9* device, UINT start, const float* data,
                                                       UINT count) {
    if (Recording() && data != nullptr) {
        Record("SetVertexShaderConstantF c{} x{}{}", start, count, FormatFloats(data, count));
    }
    return g_setVertexShaderConstantF(device, start, data, count);
}

HRESULT STDMETHODCALLTYPE HookSetPixelShaderConstantF(IDirect3DDevice9* device, UINT start, const float* data,
                                                      UINT count) {
    if (Recording() && data != nullptr) {
        Record("SetPixelShaderConstantF c{} x{}{}", start, count, FormatFloats(data, count));
    }
    return g_setPixelShaderConstantF(device, start, data, count);
}

void WriteBuffer() {
    // Sous g_mutex.
    std::ofstream(g_captureDir / "frames.txt", std::ios::app) << g_buffer;
    g_buffer.clear();
}

bool KeyPressed() noexcept {
    const bool down = (GetAsyncKeyState(GetConfig().frameCaptureKey) & 0x8000) != 0;
    const bool pressed = down && !g_keyWasDown;
    g_keyWasDown = down;
    return pressed;
}

}  // namespace

void Install(IDirect3DDevice9* device) {
    bool ok = true;
    ok &= hooking::PatchVtable(device, kCreateTextureIndex, &HookCreateTexture, &g_createTexture);
    ok &= hooking::PatchVtable(device, kCreateRenderTargetIndex, &HookCreateRenderTarget, &g_createRenderTarget);
    ok &= hooking::PatchVtable(device, kCreateDepthStencilSurfaceIndex, &HookCreateDepthStencilSurface,
                               &g_createDepthStencilSurface);
    ok &= hooking::PatchVtable(device, kStretchRectIndex, &HookStretchRect, &g_stretchRect);
    ok &= hooking::PatchVtable(device, kSetRenderTargetIndex, &HookSetRenderTarget, &g_setRenderTarget);
    ok &= hooking::PatchVtable(device, kSetDepthStencilSurfaceIndex, &HookSetDepthStencilSurface,
                               &g_setDepthStencilSurface);
    ok &= hooking::PatchVtable(device, kBeginSceneIndex, &HookBeginScene, &g_beginScene);
    ok &= hooking::PatchVtable(device, kEndSceneIndex, &HookEndScene, &g_endScene);
    ok &= hooking::PatchVtable(device, kClearIndex, &HookClear, &g_clear);
    ok &= hooking::PatchVtable(device, kSetTransformIndex, &HookSetTransform, &g_setTransform);
    ok &= hooking::PatchVtable(device, kSetViewportIndex, &HookSetViewport, &g_setViewport);
    ok &= hooking::PatchVtable(device, kSetScissorRectIndex, &HookSetScissorRect, &g_setScissorRect);
    ok &= hooking::PatchVtable(device, kSetRenderStateIndex, &HookSetRenderState, &g_setRenderState);
    ok &= hooking::PatchVtable(device, kSetTextureIndex, &HookSetTexture, &g_setTexture);
    ok &= hooking::PatchVtable(device, kDrawPrimitiveIndex, &HookDrawPrimitive, &g_drawPrimitive);
    ok &= hooking::PatchVtable(device, kDrawIndexedPrimitiveIndex, &HookDrawIndexedPrimitive, &g_drawIndexedPrimitive);
    ok &= hooking::PatchVtable(device, kDrawPrimitiveUPIndex, &HookDrawPrimitiveUP, &g_drawPrimitiveUP);
    ok &= hooking::PatchVtable(device, kDrawIndexedPrimitiveUPIndex, &HookDrawIndexedPrimitiveUP,
                               &g_drawIndexedPrimitiveUP);
    ok &= hooking::PatchVtable(device, kSetVertexShaderIndex, &HookSetVertexShader, &g_setVertexShader);
    ok &= hooking::PatchVtable(device, kSetPixelShaderIndex, &HookSetPixelShader, &g_setPixelShader);
    ok &= hooking::PatchVtable(device, kSetVertexShaderConstantFIndex, &HookSetVertexShaderConstantF,
                               &g_setVertexShaderConstantF);
    ok &= hooking::PatchVtable(device, kSetPixelShaderConstantFIndex, &HookSetPixelShaderConstantF,
                               &g_setPixelShaderConstantF);
    if (ok) {
        log::Info("Relevé d'images prêt : touche 0x{:02X} pour enregistrer {} images", GetConfig().frameCaptureKey,
                  kFramesPerCapture);
    } else {
        log::Error("Relevé d'images : interception incomplète, relevé désactivé");
    }
}

void OnPresent(IDirect3DDevice9* /*device*/) {
    std::lock_guard lock(g_mutex);
    if (g_recording.load(std::memory_order_relaxed)) {
        g_buffer += std::format("====== Present (fin de l'image {}) ======\n", kFramesPerCapture - g_framesLeft + 1);
        WriteBuffer();
        if (--g_framesLeft <= 0) {
            g_recording.store(false, std::memory_order_relaxed);
            log::Info("Relevé d'images terminé : {}", WideToUtf8(g_captureDir.wstring()));
        }
        return;
    }
    if (!KeyPressed() || g_createTexture == nullptr) {
        return;
    }
    std::error_code error;
    g_captureDir = std::filesystem::path(GetPaths().modDir) / L"captures" / FileTimestamp();
    std::filesystem::create_directories(g_captureDir, error);
    if (error) {
        log::Warn("Relevé d'images : dossier impossible à créer ({})", error.message());
        return;
    }
    g_savedShaders.clear();
    g_buffer.clear();
    g_eventIndex = 0;
    g_framesLeft = kFramesPerCapture;
    g_recording.store(true, std::memory_order_relaxed);
    log::Info("Relevé d'images démarré : {}", WideToUtf8(g_captureDir.wstring()));
}

}  // namespace hca::graphics::capture
