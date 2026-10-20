#include "graphics/FlashIcons.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "graphics/Dxt3.h"
#include "graphics/FlashIconArt.h"

namespace hca::graphics::flashicons {

namespace {

using CreateTextureFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                                    IDirect3DTexture9**, HANDLE*);
using LockRectFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9*, UINT, D3DLOCKED_RECT*, const RECT*, DWORD);
using UnlockRectFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9*, UINT);

// Indices dans les vtables COM (ordre de d3d9.h, IUnknown compris).
constexpr size_t kCreateTextureIndex = 23;  // IDirect3DDevice9::CreateTexture
constexpr size_t kLockRectIndex = 19;       // IDirect3DTexture9::LockRect
constexpr size_t kUnlockRectIndex = 20;     // IDirect3DTexture9::UnlockRect

constexpr UINT kSize = kFlashIconSize;
constexpr std::size_t kLevelBytes = static_cast<std::size_t>(kSize / 4) * (kSize / 4) * 16;  // 4096
constexpr D3DFORMAT kDxt3 = static_cast<D3DFORMAT>(0x33545844);  // 'DXT3'

// Empreintes FNV-1a des 4096 octets de niveau 0 de pc_1flash à pc_4flash (images 0 et 1), relevées le 20/10/2026 dans
// GLOBAL.PAK du jeu Steam build 6.30 (pc_Nflash.f01.tga : 105156736 + 10240 * (N - 1), puis + 4096), et retrouvées
// à l'identique dans le journal du jeu au remplissage des textures Direct3D.
struct KnownFrame {
    std::uint32_t hash;
    int button;
    int frame;
};
constexpr std::array<KnownFrame, 8> kKnownFrames = {{
    {0x62400812, 0, 0},
    {0x52FD93FE, 0, 1},
    {0x201BD8EA, 1, 0},
    {0x7955182B, 1, 1},
    {0x69C8409D, 2, 0},
    {0xB0FE4EA2, 2, 1},
    {0xA5D1A0E1, 3, 0},
    {0xC782C21B, 3, 1},
}};

CreateTextureFn g_createTexture = nullptr;
LockRectFn g_lockRect = nullptr;
UnlockRectFn g_unlockRect = nullptr;
std::atomic<bool> g_unlockPatched{false};

// Textures 64x64 DXT3 à un niveau (candidates) : table de pointeurs sans verrou, en insertion seule. UnlockRect est
// appelé pour toutes les textures du jeu, le test doit rester très court.
constexpr std::size_t kSlots = 4096;
std::array<std::atomic<std::uintptr_t>, kSlots> g_candidates{};

bool IsCandidate(std::uintptr_t pointer) noexcept {
    std::size_t index = (pointer >> 4) * 2654435761u % kSlots;
    for (std::size_t probe = 0; probe < kSlots; ++probe, index = (index + 1) % kSlots) {
        const std::uintptr_t current = g_candidates[index].load(std::memory_order_acquire);
        if (current == pointer) {
            return true;
        }
        if (current == 0) {
            return false;
        }
    }
    return false;
}

void AddCandidate(std::uintptr_t pointer) noexcept {
    std::size_t index = (pointer >> 4) * 2654435761u % kSlots;
    for (std::size_t probe = 0; probe < kSlots; ++probe, index = (index + 1) % kSlots) {
        std::uintptr_t current = g_candidates[index].load(std::memory_order_acquire);
        if (current == pointer) {
            return;
        }
        if (current == 0 && g_candidates[index].compare_exchange_strong(current, pointer)) {
            return;
        }
        if (current == pointer) {
            return;
        }
    }
}

// Texture reconnue : on en garde une référence (elle ne peut plus être libérée sous nos pieds) et ses pixels d'origine.
struct Tracked {
    IDirect3DTexture9* texture = nullptr;
    int button = 0;
    int frame = 0;
    std::vector<std::uint8_t> original;
};

std::mutex g_lock;
std::vector<Tracked> g_tracked;
int g_type = 0;  // type d'icônes voulu (0 = PC), sous g_lock
std::atomic<int> g_wanted{0};  // même valeur, lisible sans verrou
std::array<std::vector<std::uint8_t>, 2 * 4 * 2> g_art;  // [(type - 1) * 8 + button * 2 + frame], rempli à la demande

const std::vector<std::uint8_t>& Art(int type, int button, int frame) {
    std::vector<std::uint8_t>& data = g_art[static_cast<std::size_t>((type - 1) * 8 + button * 2 + frame)];
    if (data.empty()) {
        std::array<std::uint32_t, kSize * kSize> pixels{};
        RenderFlashFrame(type, button, frame, pixels.data());
        data.resize(kLevelBytes);
        EncodeDxt3(pixels.data(), kSize, kSize, data.data());
    }
    return data;
}

// Écrit `bytes` (4096 octets) dans le niveau 0 de la texture, avec le LockRect d'origine.
bool WriteLevel(IDirect3DTexture9* texture, const std::uint8_t* bytes) noexcept {
    D3DLOCKED_RECT locked{};
    if (FAILED(g_lockRect(texture, 0, &locked, nullptr, 0)) || locked.pBits == nullptr) {
        return false;
    }
    std::copy(bytes, bytes + kLevelBytes, static_cast<std::uint8_t*>(locked.pBits));
    g_unlockRect(texture, 0);
    return true;
}

// À appeler sous g_lock.
void Apply(const Tracked& tracked) noexcept {
    try {
        if (g_type == 1 || g_type == 2) {
            WriteLevel(tracked.texture, Art(g_type, tracked.button, tracked.frame).data());
        } else {
            WriteLevel(tracked.texture, tracked.original.data());
        }
    } catch (...) {
    }
}

void Register(IDirect3DTexture9* texture, int button, int frame, const std::uint8_t* pixels) noexcept {
    try {
        std::lock_guard<std::mutex> guard(g_lock);
        const auto existing = std::find_if(g_tracked.begin(), g_tracked.end(),
                                           [&](const Tracked& t) { return t.texture == texture; });
        if (existing == g_tracked.end()) {
            texture->AddRef();
            g_tracked.push_back({texture, button, frame, std::vector<std::uint8_t>(pixels, pixels + kLevelBytes)});
            log::Info("Icônes animées : texture {} reconnue (pc_{}flash, image {})", static_cast<const void*>(texture),
                      button + 1, frame + 1);
            Apply(g_tracked.back());
        } else {
            // Le moteur a rechargé la texture : ses pixels d'origine reviennent, on les remplace de nouveau.
            existing->original.assign(pixels, pixels + kLevelBytes);
            Apply(*existing);
        }
    } catch (...) {
    }
}

HRESULT STDMETHODCALLTYPE HookUnlockRect(IDirect3DTexture9* texture, UINT level) {
    const HRESULT result = g_unlockRect(texture, level);
    if (level != 0 || FAILED(result) || !IsCandidate(reinterpret_cast<std::uintptr_t>(texture))) {
        return result;
    }
    // Relecture du niveau 0 pour l'identifier ; les verrous passent par les fonctions d'origine, sans repasser ici.
    D3DLOCKED_RECT locked{};
    if (SUCCEEDED(g_lockRect(texture, 0, &locked, nullptr, D3DLOCK_READONLY)) && locked.pBits != nullptr) {
        const auto* pixels = static_cast<const std::uint8_t*>(locked.pBits);
        int button = 0;
        int frame = 0;
        const bool known = IdentifyFlashFrame(Fnv1a(pixels, kLevelBytes), button, frame);
        std::vector<std::uint8_t> copy;
        if (known) {
            copy.assign(pixels, pixels + kLevelBytes);
        }
        g_unlockRect(texture, 0);
        if (known) {
            Register(texture, button, frame, copy.data());
        }
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateTexture(IDirect3DDevice9* device, UINT width, UINT height, UINT levels, DWORD usage,
                                            D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** texture,
                                            HANDLE* sharedHandle) {
    const HRESULT result = g_createTexture(device, width, height, levels, usage, format, pool, texture, sharedHandle);
    if (SUCCEEDED(result) && texture != nullptr && *texture != nullptr && width == kSize && height == kSize &&
        format == kDxt3 && levels == 1 && pool == D3DPOOL_MANAGED && usage == 0) {
        AddCandidate(reinterpret_cast<std::uintptr_t>(*texture));
        // Les textures partagent leur vtable : une seule modification suffit.
        if (!g_unlockPatched.exchange(true)) {
            void** vtable = *reinterpret_cast<void***>(*texture);
            g_lockRect = reinterpret_cast<LockRectFn>(vtable[kLockRectIndex]);
            if (!hooking::PatchVtable(*texture, kUnlockRectIndex, &HookUnlockRect, &g_unlockRect)) {
                log::Warn("Icônes animées : interception d'UnlockRect impossible");
            }
        }
    }
    return result;
}

}  // namespace

std::uint32_t Fnv1a(const std::uint8_t* data, std::size_t size) noexcept {
    std::uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < size; ++i) {
        hash = (hash ^ data[i]) * 16777619u;
    }
    return hash;
}

bool IdentifyFlashFrame(std::uint32_t hash, int& button, int& frame) noexcept {
    for (const KnownFrame& known : kKnownFrames) {
        if (known.hash == hash) {
            button = known.button;
            frame = known.frame;
            return true;
        }
    }
    return false;
}

void SetIconType(int type) noexcept {
    if (g_wanted.load(std::memory_order_relaxed) == type) {
        return;  // appelé à chaque image : rien à faire si le type n'a pas changé
    }
    try {
        std::lock_guard<std::mutex> guard(g_lock);
        g_wanted.store(type, std::memory_order_relaxed);
        g_type = type;
        for (const Tracked& tracked : g_tracked) {
            Apply(tracked);
        }
        log::Info("Icônes animées : type {} appliqué à {} texture(s)", type, g_tracked.size());
    } catch (...) {
    }
}

void Install(IDirect3DDevice9* device) {
    if (!GetConfig().animatedIcons || GetConfig().buttonIcons == 1) {
        return;
    }
    const bool ok = hooking::PatchVtable(device, kCreateTextureIndex, &HookCreateTexture, &g_createTexture);
    log::Info("Icônes animées de l'invite à marteler : {}", ok ? "installées" : "interception de CreateTexture impossible");
}

}  // namespace hca::graphics::flashicons
