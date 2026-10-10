#include "compat/WindowThreadLock.h"

#include <atomic>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"

namespace hca::compat {

namespace {

using EnterCriticalSectionFn = void(WINAPI*)(LPCRITICAL_SECTION);

EnterCriticalSectionFn g_enterCriticalSection = nullptr;
std::atomic<DWORD> g_windowThread{0};
std::atomic<long> g_contendedWaits{0};
std::atomic<long> g_messagesPumped{0};

// Traite les messages envoyés (SendMessage) en attente pour ce thread, sans retirer les messages postés.
bool PumpSentMessages() noexcept {
    MSG message{};
    // PeekMessage traite les messages envoyés en interne et ne les signale pas : on regarde d'abord la file.
    const bool pending = (GetQueueStatus(QS_SENDMESSAGE) & QS_SENDMESSAGE) != 0;
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
    return pending;
}

void WINAPI HookEnterCriticalSection(LPCRITICAL_SECTION section) {
    if (GetCurrentThreadId() != g_windowThread.load(std::memory_order_relaxed)) {
        g_enterCriticalSection(section);
        return;
    }
    if (TryEnterCriticalSection(section)) {
        return;
    }
    // Verrou tenu par un autre thread : attendre en restant disponible pour les messages envoyés.
    if (g_contendedWaits.fetch_add(1, std::memory_order_relaxed) == 0) {
        log::Info("Thread de la fenêtre : première attente d'un verrou du moteur tenu par un autre thread");
    }
    for (;;) {
        if (PumpSentMessages() && g_messagesPumped.fetch_add(1, std::memory_order_relaxed) == 0) {
            log::Info("Thread de la fenêtre : message envoyé traité pendant l'attente d'un verrou (cas de "
                      "l'interblocage avec l'overlay Steam)");
        }
        if (TryEnterCriticalSection(section)) {
            return;
        }
        // Réveil dès qu'un message est envoyé à ce thread, sinon au bout d'1 ms pour réessayer le verrou.
        MsgWaitForMultipleObjectsEx(0, nullptr, 1, QS_SENDMESSAGE, 0);
    }
}

}  // namespace

void InstallWindowThreadLock(HMODULE engineModule) {
    if (!GetConfig().windowThreadLock) {
        log::Info("Attente de verrou avec traitement des messages désactivée (WindowThreadLock=0)");
        return;
    }
    if (!hooking::PatchImport(engineModule, "kernel32.dll", "EnterCriticalSection", &HookEnterCriticalSection,
                              &g_enterCriticalSection)) {
        log::Warn("EnterCriticalSection du moteur non intercepté : interblocage possible avec l'overlay Steam");
        return;
    }
    log::Info("Attente de verrou avec traitement des messages installée (thread de la fenêtre)");
}

void SetWindowThread(DWORD threadId) noexcept {
    g_windowThread.store(threadId, std::memory_order_relaxed);
}

}  // namespace hca::compat
