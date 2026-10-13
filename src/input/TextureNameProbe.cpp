#include "input/TextureNameProbe.h"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <set>
#include <string>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"

namespace hca::input {

namespace {

constexpr std::uintptr_t kExpectedEngineBase = 0x10000000;
constexpr std::uintptr_t kRelay = 0x1004A890;          // jmp 10AED2B0
constexpr std::uintptr_t kLoadByName = 0x10AED2B0;     // chargement d'une texture par son nom
constexpr std::size_t kMaxLogged = 80;

std::uintptr_t g_original = kLoadByName;
std::mutex g_lock;
std::set<std::string> g_seen;

// Lecture protégée : l'argument n'est pas garanti valide (fonction vue seulement au désassemblage).
bool CopyName(const char* name, char (&buffer)[96]) noexcept {
    __try {
        std::size_t i = 0;
        for (; i + 1 < sizeof(buffer) && name[i] != '\0'; ++i) {
            buffer[i] = name[i];
        }
        buffer[i] = '\0';
        return i > 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void __cdecl OnTextureRequested(const char* name) {
    char buffer[96]{};
    if (name == nullptr || !CopyName(name, buffer) || !IsButtonIconName(buffer)) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_lock);
    if (g_seen.size() < kMaxLogged && g_seen.insert(buffer).second) {
        log::Info("Icônes de boutons : texture demandée « {} »", buffer);
    }
}

// Relais : appelle OnTextureRequested avec le 3e argument puis saute à l'original (pile intacte).
__declspec(naked) void TextureRelay() {
    __asm {
        pushad
        push dword ptr [esp + 44]   // 32 (pushad) + 4 (retour) + 8 : 3e argument
        call OnTextureRequested
        add esp, 4
        popad
        jmp dword ptr [g_original]
    }
}

}  // namespace

bool IsButtonIconName(const char* name) noexcept {
    if (name == nullptr) {
        return false;
    }
    return std::strstr(name, "button") != nullptr || std::strstr(name, "flash") != nullptr ||
           std::strncmp(name, "pc_", 3) == 0;
}

void InstallTextureNameProbe(HMODULE engine) {
    if (!GetConfig().iconLog || engine == nullptr ||
        reinterpret_cast<std::uintptr_t>(engine) != kExpectedEngineBase) {
        return;
    }
    // Le relais doit être exactement « jmp 10AED2B0 ».
    std::uint8_t expected[5] = {0xE9};
    const auto rel = static_cast<std::int32_t>(kLoadByName - (kRelay + 5));
    std::memcpy(expected + 1, &rel, 4);
    if (std::memcmp(reinterpret_cast<const void*>(kRelay), expected, 5) != 0) {
        log::Warn("Icônes de boutons : relais de chargement des textures différent, diagnostic non installé");
        return;
    }
    std::uint8_t jump[5] = {0xE9};
    const auto target = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&TextureRelay) -
                                                  static_cast<std::intptr_t>(kRelay + 5));
    std::memcpy(jump + 1, &target, 4);
    const bool ok = hooking::WriteCode(reinterpret_cast<void*>(kRelay), jump, 5);
    log::Info("Icônes de boutons : journal des textures d'icônes demandées {}", ok ? "actif" : "impossible");
}

}  // namespace hca::input
