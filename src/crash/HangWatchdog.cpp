#include "crash/HangWatchdog.h"

#include <cstdint>

#include "core/Config.h"
#include "core/Log.h"
#include "crash/CrashHandler.h"
#include "framepacing/FrameMonitor.h"

namespace hca::crash {

namespace {

constexpr DWORD kPollMs = 250;

DWORD g_mainThreadId = 0;

DWORD WINAPI WatchdogThread(LPVOID) {
    const std::int64_t frequency = frames::TicksPerSecond();
    const std::int64_t threshold = frequency * GetConfig().hangSeconds;
    std::int64_t reportedFor = 0;  // image après laquelle le blocage en cours a déjà été signalé
    for (;;) {
        Sleep(kPollMs);
        const std::int64_t last = frames::LastPresentTicks();
        if (last == 0 || last == reportedFor) {
            continue;  // pas encore d'image, ou blocage en cours déjà signalé
        }
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        if (now.QuadPart - last < threshold) {
            continue;
        }
        reportedFor = last;
        log::Crash("=== BLOCAGE : aucune image depuis {:.1f} s ===",
                   static_cast<double>(now.QuadPart - last) / static_cast<double>(frequency));
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
