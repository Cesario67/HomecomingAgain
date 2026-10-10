#pragma once

#include <string>

namespace hca {

struct Paths {
    std::wstring gameExe;     // ...\SilentHill.exe
    std::wstring gameDir;     // dossier du jeu
    std::wstring iniFile;     // ...\HomecomingAgain.ini
    std::wstring modDir;      // ...\HomecomingAgain
    std::wstring logsDir;     // ...\HomecomingAgain\logs
    std::wstring dumpsDir;    // ...\HomecomingAgain\crashdumps
};

// Calcule les chemins à partir de l'exécutable hôte et crée les dossiers du mod.
bool InitPaths();

const Paths& GetPaths() noexcept;

}  // namespace hca
