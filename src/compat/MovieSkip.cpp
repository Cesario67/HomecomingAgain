#include "compat/MovieSkip.h"

#include <cstring>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"

namespace hca::compat {

namespace {

using GetProcAddressFn = FARPROC(WINAPI*)(HMODULE, LPCSTR);
using BinkNextFrameFn = void(__stdcall*)(void*);
using BinkGotoFn = void(__stdcall*)(void*, std::uint32_t, std::int32_t);

GetProcAddressFn g_getProcAddress = nullptr;
BinkNextFrameFn g_binkNextFrame = nullptr;
BinkGotoFn g_binkGoto = nullptr;
bool g_keyWasDown = false;

// Touche pressée (front montant) pendant que la fenêtre active appartient au jeu.
bool SkipKeyPressed() noexcept {
    const int key = GetConfig().movieSkipKey;
    const bool down = key != 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
    const bool pressed = down && !g_keyWasDown;
    g_keyWasDown = down;
    if (!pressed) {
        return false;
    }
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    return process == GetCurrentProcessId();
}

void __stdcall HookBinkNextFrame(void* bink) {
    g_binkNextFrame(bink);
    if (bink == nullptr || g_binkGoto == nullptr) {
        return;
    }
    BinkHeader header{};
    std::memcpy(&header, bink, sizeof(header));
    std::uint32_t target = 0;
    if (ShouldSkipTo(header, SkipKeyPressed(), target)) {
        g_binkGoto(bink, target, 0);
        log::Info("Cinématique passée : image {} sur {} -> {}", header.frameNum, header.frames, target);
    }
}

FARPROC WINAPI HookGetProcAddress(HMODULE module, LPCSTR name) {
    const FARPROC function = g_getProcAddress(module, name);
    // Un nom (et non un numéro d'ordre) : son adresse est au-delà des 64 premiers Ko.
    if (function == nullptr || reinterpret_cast<std::uintptr_t>(name) <= 0xFFFF) {
        return function;
    }
    if (std::strcmp(name, "_BinkNextFrame@4") == 0) {
        g_binkNextFrame = reinterpret_cast<BinkNextFrameFn>(function);
        if (g_binkGoto == nullptr) {
            g_binkGoto = reinterpret_cast<BinkGotoFn>(g_getProcAddress(module, "_BinkGoto@12"));
        }
        log::Info("Cinématiques : touche 0x{:02X} pour passer les vidéos ({})", GetConfig().movieSkipKey,
                  g_binkGoto != nullptr ? "prête" : "BinkGoto introuvable, désactivée");
        return reinterpret_cast<FARPROC>(&HookBinkNextFrame);
    }
    if (std::strcmp(name, "_BinkGoto@12") == 0) {
        g_binkGoto = reinterpret_cast<BinkGotoFn>(function);
    }
    return function;
}

}  // namespace

bool ShouldSkipTo(const BinkHeader& bink, bool keyPressed, std::uint32_t& targetFrame) noexcept {
    // Les images Bink sont numérotées à partir de 1 ; la dernière est `frames`.
    if (!keyPressed || bink.frames < 2 || bink.frameNum >= bink.frames) {
        return false;
    }
    targetFrame = bink.frames;
    return true;
}

void InstallMovieSkip(HMODULE engineModule) {
    if (GetConfig().movieSkipKey == 0) {
        return;
    }
    // Le moteur importe binkw32.dll par sa table d'imports (vérifié par dumpbin /imports le 10/10/2026). L'interception
    // par GetProcAddress reste en secours, au cas où une autre version charge Bink à la demande.
    if (hooking::PatchImport(engineModule, "binkw32.dll", "_BinkNextFrame@4", &HookBinkNextFrame, &g_binkNextFrame)) {
        if (const HMODULE bink = GetModuleHandleW(L"binkw32.dll"); bink != nullptr) {
            g_binkGoto = reinterpret_cast<BinkGotoFn>(GetProcAddress(bink, "_BinkGoto@12"));
        }
        log::Info("Cinématiques : touche 0x{:02X} pour passer les vidéos ({})", GetConfig().movieSkipKey,
                  g_binkGoto != nullptr ? "prête" : "BinkGoto introuvable, désactivée");
        return;
    }
    if (!hooking::PatchImport(engineModule, "kernel32.dll", "GetProcAddress", &HookGetProcAddress,
                              &g_getProcAddress)) {
        log::Warn("Cinématiques : fonctions de Bink non interceptées, touche pour passer indisponible");
    }
}

}  // namespace hca::compat