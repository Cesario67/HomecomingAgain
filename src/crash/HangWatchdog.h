#pragma once

#include <windows.h>

namespace shhf::crash {

// Détecteur de blocage : un thread vérifie toutes les 250 ms qu'une image a été présentée récemment. Après
// `[Crash] HangSeconds` sans image, il journalise une fois l'état du thread principal (celui de la fenêtre) et
// du thread de rendu, puis écrit un minidump. Windows ferme un jeu qui ne répond plus (« Application Hang ») sans
// laisser de trace exploitable : c'est ce qui manquait au premier essai en fenêtré 1080p.
void StartHangWatchdog(DWORD mainThreadId) noexcept;

}  // namespace shhf::crash
