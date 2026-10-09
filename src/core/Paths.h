#pragma once

#include <string>

namespace shhf {

struct Paths {
    std::wstring gameExe;     // ...\SilentHill.exe
    std::wstring gameDir;     // dossier du jeu
    std::wstring iniFile;     // ...\SHHFix.ini
    std::wstring modDir;      // ...\SHHFix
    std::wstring logsDir;     // ...\SHHFix\logs
    std::wstring dumpsDir;    // ...\SHHFix\crashdumps
};

// Calcule les chemins à partir de l'exécutable hôte et crée les dossiers du mod.
bool InitPaths();

const Paths& GetPaths() noexcept;

}  // namespace shhf
