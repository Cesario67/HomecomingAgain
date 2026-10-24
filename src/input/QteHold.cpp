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
constexpr std::uintptr_t kPromptShow = 0x1094E3F0;
constexpr std::uintptr_t kPromptHide = 0x1094E4A0;
constexpr std::size_t kPrologueLength = 7;  // push -1, push SEH handler
constexpr BYTE kShowPrologue[kPrologueLength] = {0x6A, 0xFF, 0x68, 0x25, 0x53, 0xE7, 0x10};
constexpr BYTE kHidePrologue[kPrologueLength] = {0x6A, 0xFF, 0x68, 0x37, 0x53, 0xE7, 0x10};
// Safety net in case a prompt ends without going through the hide handler.
constexpr DWORD kPromptTimeoutMs = 20000;

using ShowFn = void(__fastcall*)(void* self, void* unused, DWORD argument);
using HideFn = void(__fastcall*)(void* self, void* unused);

ShowFn g_showOriginal = nullptr;
HideFn g_hideOriginal = nullptr;
std::atomic<DWORD> g_activeUntil{0};
std::atomic<bool> g_loggedActive{false};
int g_rate = 10;
bool g_enabled = false;

// Executable copy of the first `length` bytes of `site` (no relative instruction among them), followed by a jump to
// the rest of the function. Built once at install time.
void* BuildTrampoline(std::uintptr_t site, std::size_t length) noexcept {
    BYTE* const code = static_cast<BYTE*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (code == nullptr) {
        return nullptr;
    }
    std::memcpy(code, reinterpret_cast<const void*>(site), length);
    code[length] = 0xE9;
    const std::int32_t back =
        static_cast<std::int32_t>((site + length) - (reinterpret_cast<std::uintptr_t>(code) + length + 5));
    std::memcpy(code + length + 1, &back, sizeof(back));
    FlushInstructionCache(GetCurrentProcess(), code, 32);
    return code;
}

void __fastcall PromptShowHook(void* self, void* unused, DWORD argument) {
    g_activeUntil.store(GetTickCount() + kPromptTimeoutMs);
    g_showOriginal(self, unused, argument);
}

void __fastcall PromptHideHook(void* self, void* unused) {
    g_activeUntil.store(0);
    g_hideOriginal(self, unused);
}

bool IsPromptActive() noexcept {
    return static_cast<LONG>(g_activeUntil.load() - GetTickCount()) > 0;
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
    if (std::memcmp(reinterpret_cast<const void*>(kPromptShow), kShowPrologue, kPrologueLength) != 0 ||
        std::memcmp(reinterpret_cast<const void*>(kPromptHide), kHidePrologue, kPrologueLength) != 0) {
        log::Warn("Maintien pour les QTE non appliqué : code inattendu (autre version du moteur ?)");
        return;
    }
    g_showOriginal = reinterpret_cast<ShowFn>(BuildTrampoline(kPromptShow, kPrologueLength));
    g_hideOriginal = reinterpret_cast<HideFn>(BuildTrampoline(kPromptHide, kPrologueLength));
    if (g_showOriginal == nullptr || g_hideOriginal == nullptr ||
        !hooking::WriteJump(reinterpret_cast<void*>(kPromptShow), reinterpret_cast<const void*>(&PromptShowHook),
                            kPrologueLength) ||
        !hooking::WriteJump(reinterpret_cast<void*>(kPromptHide), reinterpret_cast<const void*>(&PromptHideHook),
                            kPrologueLength)) {
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
