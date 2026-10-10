#pragma once

#include <windows.h>

#include <cstdint>

namespace shhf::frames::waits {

// Relevé des attentes du thread de rendu (diagnostic du verrou de cadence) : pour chaque appelant, nombre
// d'appels et temps passé dans Sleep, SleepEx, WaitForSingleObject, WaitForMultipleObjects, et nombre d'appels
// à QueryPerformanceCounter (une boucle d'attente active s'y voit). Installé sur l'IAT du moteur.
void Install(HMODULE engineModule);

// Enregistre une attente du thread de rendu (ignorée pour les autres threads). Sans allocation.
void RecordWait(void* caller, const char* api, std::int64_t ticks) noexcept;

// Journalise les principaux appelants depuis le rapport précédent, puis remet le relevé à zéro. Appelé depuis le
// thread de rendu (rapport périodique de FrameMonitor).
void Report() noexcept;

}  // namespace shhf::frames::waits
