#include "engine/EngineSettings.h"

#include <windows.h>

#include <cstdint>

namespace shhf::engine {

namespace {

constexpr std::uintptr_t kEngineRoot = 0x111D2460;  // voir EngineSettings.h
constexpr std::uintptr_t kSubsystemsOffset = 0x38;
constexpr std::uintptr_t kEngineSettingsOffset = 0x08;
constexpr std::uintptr_t kFpsLimitOffset = 0xE4;
constexpr std::uintptr_t kMaxFpsLimitOffset = 0xE8;
constexpr DWORD kAnalyzedEngineTimeDateStamp = 0x490F50BF;

bool IsAnalyzedEngine() noexcept {
    const HMODULE engine = GetModuleHandleW(L"g_SilentHill.sgl");
    if (engine == nullptr || reinterpret_cast<std::uintptr_t>(engine) != 0x10000000) {
        return false;
    }
    const auto* base = reinterpret_cast<const BYTE*>(engine);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return nt->FileHeader.TimeDateStamp == kAnalyzedEngineTimeDateStamp;
}

// Lecture protégée par SEH (pas d'objet C++ dans cette fonction) : un pointeur encore nul pendant le
// chargement ne doit pas faire planter le jeu.
bool ReadUnsafe(int& fpsLimit, int& maxFpsLimit) noexcept {
    __try {
        const std::uintptr_t root = *reinterpret_cast<const std::uintptr_t*>(kEngineRoot);
        if (root == 0) {
            return false;
        }
        const std::uintptr_t subsystems = *reinterpret_cast<const std::uintptr_t*>(root + kSubsystemsOffset);
        if (subsystems == 0) {
            return false;
        }
        const std::uintptr_t settings = *reinterpret_cast<const std::uintptr_t*>(subsystems + kEngineSettingsOffset);
        if (settings == 0) {
            return false;
        }
        fpsLimit = *reinterpret_cast<const int*>(settings + kFpsLimitOffset);
        maxFpsLimit = *reinterpret_cast<const int*>(settings + kMaxFpsLimitOffset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

}  // namespace

bool ReadFpsLimits(int& fpsLimit, int& maxFpsLimit) noexcept {
    static const bool analyzed = IsAnalyzedEngine();
    return analyzed && ReadUnsafe(fpsLimit, maxFpsLimit);
}

}  // namespace shhf::engine
