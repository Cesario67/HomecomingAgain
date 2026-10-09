#pragma once

#include <windows.h>

namespace shhf::crash {

// Installe notre filtre d'exceptions non gérées (minidump + rapport dans le journal), puis intercepte
// SetUnhandledExceptionFilter dans SilentHill.exe : le filtre du jeu est mémorisé et appelé après le nôtre
// au lieu de le remplacer.
void Install(HMODULE gameModule, HMODULE engineModule);

// Charge dbghelp.dll hors du chemin de plantage (appelé une fois le chargement du jeu terminé).
void PreloadDbgHelp() noexcept;

// Vérifie qu'un autre module n'a pas remplacé notre filtre ; le réinstalle sinon.
void EnsureInstalled() noexcept;

}  // namespace shhf::crash
