#include "compat/StartupLogos.h"

#include <cstdint>
#include <cstring>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"

namespace hca::compat {

namespace {

constexpr std::uintptr_t kExpectedEngineBase = 0x10000000;
constexpr std::uintptr_t kStepInstruction = 0x104EB4F0;  // fld dword ptr ds:[0x10FF5C68]
constexpr std::uint8_t kExpectedBytes[] = {0xD9, 0x05, 0x68, 0x5C, 0xFF, 0x10};
constexpr std::uintptr_t kOriginalStep = 0x10FF5C68;      // 0,0333 s
constexpr std::uintptr_t kLoopActive = 0x11596FCC;        // octet testé en 0x104EB4B9 et 0x104EB536
constexpr float kFactor = 10.0f;
constexpr DWORD kPollMs = 50;
constexpr DWORD kMaxMs = 30000;

float g_step = 0.0f;

bool LoopActive() noexcept {
    __try {
        return *reinterpret_cast<const volatile std::uint8_t*>(kLoopActive) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

DWORD WINAPI WatchThread(LPVOID) {
    const float original = *reinterpret_cast<const float*>(kOriginalStep);
    FirstLoadingScreen screen;
    const ULONGLONG start = GetTickCount64();
    for (;;) {
        Sleep(kPollMs);
        const bool finished = screen.Update(LoopActive());
        const bool timeout = GetTickCount64() - start > kMaxMs;
        if (finished || timeout) {
            g_step = original;
            log::Info("Logos de démarrage : pas d'animation rendu normal après {:.1f} s ({})",
                      static_cast<double>(GetTickCount64() - start) / 1000.0, finished ? "écran titre" : "délai maximal");
            return 0;
        }
    }
}

}  // namespace

bool FirstLoadingScreen::Update(bool active) noexcept {
    if (finished_) {
        return false;
    }
    if (active) {
        seenActive_ = true;
        return false;
    }
    if (seenActive_) {
        finished_ = true;
        return true;
    }
    return false;
}

void InstallStartupLogos(HMODULE engineModule) {
    if (!GetConfig().skipLogos || engineModule == nullptr) {
        return;
    }
    if (reinterpret_cast<std::uintptr_t>(engineModule) != kExpectedEngineBase ||
        std::memcmp(reinterpret_cast<const void*>(kStepInstruction), kExpectedBytes, sizeof(kExpectedBytes)) != 0) {
        log::Warn("Logos de démarrage : code du moteur différent de celui attendu (autre version ?), non modifié");
        return;
    }
    g_step = *reinterpret_cast<const float*>(kOriginalStep) * kFactor;
    const auto address = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_step));
    std::uint8_t instruction[6] = {0xD9, 0x05};
    std::memcpy(instruction + 2, &address, 4);
    if (!hooking::WriteCode(reinterpret_cast<void*>(kStepInstruction), instruction, sizeof(instruction))) {
        log::Warn("Logos de démarrage : écriture impossible, logos d'origine");
        return;
    }
    if (const HANDLE thread = CreateThread(nullptr, 0, &WatchThread, nullptr, 0, nullptr); thread != nullptr) {
        CloseHandle(thread);
    } else {
        g_step = *reinterpret_cast<const float*>(kOriginalStep);
    }
    log::Info("Logos de démarrage : animation {} fois plus rapide jusqu'à l'écran titre", static_cast<int>(kFactor));
}

}  // namespace hca::compat
