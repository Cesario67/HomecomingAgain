#include "framepacing/WaitProfiler.h"

#include <intrin.h>

#include <algorithm>
#include <array>
#include <cstdint>

#include "core/Hooking.h"
#include "core/Log.h"
#include "core/SystemInfo.h"
#include "engine/EngineSettings.h"
#include "framepacing/FrameMonitor.h"

namespace hca::frames::waits {

namespace {

using WaitForSingleObjectFn = DWORD(WINAPI*)(HANDLE, DWORD);
using WaitForMultipleObjectsFn = DWORD(WINAPI*)(DWORD, const HANDLE*, BOOL, DWORD);
using SleepExFn = DWORD(WINAPI*)(DWORD, BOOL);
using QueryPerformanceCounterFn = BOOL(WINAPI*)(LARGE_INTEGER*);

WaitForSingleObjectFn g_waitForSingleObject = nullptr;
WaitForMultipleObjectsFn g_waitForMultipleObjects = nullptr;
SleepExFn g_sleepEx = nullptr;
QueryPerformanceCounterFn g_queryPerformanceCounter = nullptr;

struct Entry {
    void* caller = nullptr;
    const char* api = nullptr;
    std::uint32_t calls = 0;
    std::int64_t ticks = 0;
};

// Écrit et lu uniquement par le thread de rendu : pas de verrou.
constexpr size_t kMaxEntries = 48;
std::array<Entry, kMaxEntries> g_entries{};
size_t g_entryCount = 0;
std::uint32_t g_dropped = 0;

std::int64_t Now() noexcept {
    LARGE_INTEGER counter{};
    ::QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

bool IsRenderThread() noexcept {
    const DWORD render = RenderThreadId();
    return render != 0 && render == GetCurrentThreadId();
}

DWORD WINAPI HookWaitForSingleObject(HANDLE handle, DWORD milliseconds) {
    void* const caller = _ReturnAddress();
    const std::int64_t start = Now();
    const DWORD result = g_waitForSingleObject(handle, milliseconds);
    RecordWait(caller, "WaitForSingleObject", Now() - start);
    return result;
}

DWORD WINAPI HookWaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL waitAll, DWORD milliseconds) {
    void* const caller = _ReturnAddress();
    const std::int64_t start = Now();
    const DWORD result = g_waitForMultipleObjects(count, handles, waitAll, milliseconds);
    RecordWait(caller, "WaitForMultipleObjects", Now() - start);
    return result;
}

DWORD WINAPI HookSleepEx(DWORD milliseconds, BOOL alertable) {
    void* const caller = _ReturnAddress();
    const std::int64_t start = Now();
    const DWORD result = g_sleepEx(milliseconds, alertable);
    RecordWait(caller, "SleepEx", Now() - start);
    return result;
}

BOOL WINAPI HookQueryPerformanceCounter(LARGE_INTEGER* counter) {
    RecordWait(_ReturnAddress(), "QueryPerformanceCounter", 0);
    return g_queryPerformanceCounter(counter);
}

template <typename Fn>
void Hook(HMODULE module, const char* name, Fn replacement, Fn* original) {
    if (!hooking::PatchImport(module, "kernel32.dll", name, replacement, original)) {
        log::Warn("Relevé des attentes : {} non intercepté", name);
    }
}

}  // namespace

void Install(HMODULE engineModule) {
    Hook(engineModule, "WaitForSingleObject", &HookWaitForSingleObject, &g_waitForSingleObject);
    Hook(engineModule, "WaitForMultipleObjects", &HookWaitForMultipleObjects, &g_waitForMultipleObjects);
    Hook(engineModule, "SleepEx", &HookSleepEx, &g_sleepEx);
    Hook(engineModule, "QueryPerformanceCounter", &HookQueryPerformanceCounter, &g_queryPerformanceCounter);
    log::Info("Relevé des attentes du thread de rendu actif");
}

void RecordWait(void* caller, const char* api, std::int64_t ticks) noexcept {
    if (!IsRenderThread()) {
        return;
    }
    for (size_t i = 0; i < g_entryCount; ++i) {
        Entry& entry = g_entries[i];
        if (entry.caller == caller && entry.api == api) {
            ++entry.calls;
            entry.ticks += ticks;
            return;
        }
    }
    if (g_entryCount == kMaxEntries) {
        ++g_dropped;
        return;
    }
    g_entries[g_entryCount++] = Entry{caller, api, 1, ticks};
}

void Report() noexcept {
    try {
        int fpsLimit = 0;
        int maxFpsLimit = 0;
        if (engine::ReadFpsLimits(fpsLimit, maxFpsLimit)) {
            log::Info("  Moteur : fpsLimit={} maxFPSLimit={}", fpsLimit, maxFpsLimit);
        }
        std::sort(g_entries.begin(), g_entries.begin() + static_cast<std::ptrdiff_t>(g_entryCount),
                  [](const Entry& a, const Entry& b) { return a.ticks != b.ticks ? a.ticks > b.ticks : a.calls > b.calls; });
        const double ticksPerMs = static_cast<double>(TicksPerSecond()) / 1000.0;
        const size_t shown = std::min<size_t>(g_entryCount, 6);
        for (size_t i = 0; i < shown; ++i) {
            const Entry& entry = g_entries[i];
            log::Info("  Attente rendu : {} depuis {} : {} appels, {:.1f} ms", entry.api,
                      sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(entry.caller)), entry.calls,
                      ticksPerMs > 0.0 ? static_cast<double>(entry.ticks) / ticksPerMs : 0.0);
        }
        if (g_dropped > 0) {
            log::Info("  Attente rendu : {} appels non classés (table pleine)", g_dropped);
        }
    } catch (...) {
    }
    g_entryCount = 0;
    g_dropped = 0;
}

}  // namespace hca::frames::waits
