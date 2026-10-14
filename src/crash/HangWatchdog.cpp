#include "crash/HangWatchdog.h"

#include <cstdint>

#include "core/Config.h"
#include "core/Log.h"
#include "crash/CrashHandler.h"
#include "crash/HangClock.h"
#include "framepacing/FrameMonitor.h"

namespace hca::crash {

namespace {

constexpr DWORD kPollMs = 250;

DWORD g_mainThreadId = 0;

// Le jeu est-il au premier plan (fenêtre active de notre processus, non réduite) ? Sinon il ne présente plus
// d'images, sans être bloqué.
bool GameInForeground() noexcept {
    const HWND foreground = GetForegroundWindow();
    if (foreground == nullptr || IsIconic(foreground)) {
        return false;
    }
    DWORD process = 0;
    GetWindowThreadProcessId(foreground, &process);
    return process == GetCurrentProcessId();
}

DWORD WINAPI WatchdogThread(LPVOID) {
    const std::int64_t frequency = frames::TicksPerSecond();
    const std::int64_t threshold = frequency * GetConfig().hangSeconds;
    std::int64_t reportedFor = 0;     // image après laquelle le blocage en cours a déjà été signalé
    std::int64_t lastBackground = 0;  // dernier instant où le jeu n'était pas au premier plan
    for (;;) {
        Sleep(kPollMs);
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        if (!GameInForeground()) {
            lastBackground = now.QuadPart;
            continue;
        }
        const std::int64_t last = frames::LastPresentTicks();
        if (last == 0 || last == reportedFor) {
            continue;  // pas encore d'image, ou blocage en cours déjà signalé
        }
        const std::int64_t elapsed = HangElapsed(now.QuadPart, last, lastBackground);
        if (elapsed < threshold) {
            continue;
        }
        reportedFor = last;
        log::Crash("=== BLOCAGE : aucune image depuis {:.1f} s (jeu au premier plan) ===",
                   static_cast<double>(elapsed) / static_cast<double>(frequency));
        LogThreadStack(g_mainThreadId, "Thread principal");
        const DWORD render = frames::RenderThreadId();
        if (render != g_mainThreadId) {
            LogThreadStack(render, "Thread de rendu");
        }
        WriteHangDump();
    }
}

}  // namespace

void StartHangWatchdog(DWORD mainThreadId) noexcept {
    if (GetConfig().hangSeconds <= 0) {
        log::Info("Détecteur de blocage désactivé (HangSeconds=0)");
        return;
    }
    g_mainThreadId = mainThreadId;
    const HANDLE thread = CreateThread(nullptr, 0, &WatchdogThread, nullptr, 0, nullptr);
    if (thread == nullptr) {
        log::Warn("Détecteur de blocage : création du thread impossible (erreur {})", GetLastError());
        return;
    }
    CloseHandle(thread);
    log::Info("Détecteur de blocage actif : diagnostic après {} s sans image", GetConfig().hangSeconds);
}

}  // namespace hca::crash
