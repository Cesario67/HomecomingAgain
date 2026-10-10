#pragma once

#include <windows.h>

namespace hca::crash {

// Installe notre filtre d'exceptions non gérées (minidump + rapport dans le journal), puis intercepte
// SetUnhandledExceptionFilter dans SilentHill.exe : le filtre du jeu est mémorisé et appelé après le nôtre
// au lieu de le remplacer.
void Install(HMODULE gameModule, HMODULE engineModule);

// Charge dbghelp.dll hors du chemin de plantage (appelé une fois le chargement du jeu terminé).
void PreloadDbgHelp() noexcept;

// Diagnostic de blocage : journalise registres et adresses de code présentes sur la pile d'un autre thread
// (suspendu le temps de la copie), et écrit un minidump de tous les threads sans exception.
void LogThreadStack(DWORD threadId, const char* label) noexcept;
void WriteHangDump() noexcept;

// Vérifie qu'un autre module n'a pas remplacé notre filtre ; le réinstalle sinon.
void EnsureInstalled() noexcept;

}  // namespace hca::crash
