#include "input/QteHold.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "input/QteHoldLogic.h"

namespace hca::input {

namespace {

constexpr std::uintptr_t kExpectedEngineBase = 0x10000000;
constexpr std::uintptr_t kPromptStart = 0x1092E870;
constexpr std::uintptr_t kPromptDestroy = 0x1092E7E0;
constexpr std::size_t kStartLength = 9;    // push esi, push edi, mov esi,ecx, call rel32
constexpr std::size_t kDestroyLength = 7;  // push -1, push handler
constexpr std::size_t kActiveFlagOffset = 0x30;
constexpr BYTE kStartPrologue[5] = {0x56, 0x57, 0x8B, 0xF1, 0xE8};
constexpr BYTE kDestroyPrologue[kDestroyLength] = {0x6A, 0xFF, 0x68, 0xA1, 0x25, 0xE7, 0x10};

using ThisCallFn = void(__fastcall*)(void* self, void* unused);

ThisCallFn g_startOriginal = nullptr;
ThisCallFn g_destroyOriginal = nullptr;
std::atomic<void*> g_prompt{nullptr};
std::atomic<bool> g_loggedActive{false};
int g_rate = 10;
bool g_enabled = false;

// Executable copy of the first instructions of `site`, followed by a jump to the rest of the function. A call rel32 at
// byte 4 (the start function) is re-targeted. Built once at install time.
void* BuildTrampoline(std::uintptr_t site, std::size_t length, bool relocateCallAt4) noexcept {
    BYTE* const code = static_cast<BYTE*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (code == nullptr) {
        return nullptr;
    }
    std::memcpy(code, reinterpret_cast<const void*>(site), length);
    if (relocateCallAt4) {
        std::int32_t rel = 0;
        std::memcpy(&rel, code + 5, sizeof(rel));
        const std::uintptr_t target = site + length + static_cast<std::uintptr_t>(rel);
        rel = static_cast<std::int32_t>(target - (reinterpret_cast<std::uintptr_t>(code) + length));
        std::memcpy(code + 5, &rel, sizeof(rel));
    }
    code[length] = 0xE9;
    const std::int32_t back =
        static_cast<std::int32_t>((site + length) - (reinterpret_cast<std::uintptr_t>(code) + length + 5));
    std::memcpy(code + length + 1, &back, sizeof(back));
    FlushInstructionCache(GetCurrentProcess(), code, 32);
    return code;
}

void __fastcall PromptStartHook(void* self, void* unused) {
    g_prompt.store(self);
    g_startOriginal(self, unused);
}

void __fastcall PromptDestroyHook(void* self, void* unused) {
    void* expected = self;
    g_prompt.compare_exchange_strong(expected, nullptr);
    g_destroyOriginal(self, unused);
}

bool IsPromptActive() noexcept {
    const BYTE* const prompt = static_cast<const BYTE*>(g_prompt.load());
    if (prompt == nullptr) {
        return false;
    }
    __try {
        return prompt[kActiveFlagOffset] != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

}  // namespace

void InstallQteHold(HMODULE engine) {
    const Config& config = GetConfig();
    if (!config.qteHold) {
        log::Info("Maintien pour les QTE désactivé ([Input] QteHold=0)");
        return;
    }
    if (reinterpret_cast<std::uintptr_t>(engine) != kExpectedEngineBase) {
        log::Warn("Maintien pour les QTE non appliqué : moteur chargé à une adresse inattendue");
        return;
    }
    if (std::memcmp(reinterpret_cast<const void*>(kPromptStart), kStartPrologue, sizeof(kStartPrologue)) != 0 ||
        std::memcmp(reinterpret_cast<const void*>(kPromptDestroy), kDestroyPrologue, sizeof(kDestroyPrologue)) != 0) {
        log::Warn("Maintien pour les QTE non appliqué : code inattendu (autre version du moteur ?)");
        return;
    }
    g_startOriginal = reinterpret_cast<ThisCallFn>(BuildTrampoline(kPromptStart, kStartLength, true));
    g_destroyOriginal = reinterpret_cast<ThisCallFn>(BuildTrampoline(kPromptDestroy, kDestroyLength, false));
    if (g_startOriginal == nullptr || g_destroyOriginal == nullptr ||
        !hooking::WriteJump(reinterpret_cast<void*>(kPromptStart), reinterpret_cast<const void*>(&PromptStartHook),
                            kStartLength) ||
        !hooking::WriteJump(reinterpret_cast<void*>(kPromptDestroy), reinterpret_cast<const void*>(&PromptDestroyHook),
                            kDestroyLength)) {
        log::Error("Maintien pour les QTE : installation impossible");
        return;
    }
    g_rate = qte::ClampRate(config.qteHoldRate);
    g_enabled = true;
    log::Info("Maintien pour les QTE appliqué : {} appuis par seconde tant que le bouton est tenu", g_rate);
}

WORD FilterQteButtons(WORD buttons) noexcept {
    if (!g_enabled) {
        return buttons;
    }
    const bool active = IsPromptActive();
    if (active != g_loggedActive.exchange(active)) {
        log::Info("QTE : invite {}", active ? "affichée" : "terminée");
    }
    return qte::ApplyHold(buttons, active, GetTickCount(), g_rate);
}

}  // namespace hca::input
