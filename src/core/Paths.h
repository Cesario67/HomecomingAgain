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

// Calcule les chemins à partir de l'exécutable hôte et crée les dossiers du mod. En cas d'échec de création,
// les chemins restent renseignés (pour signaler l'erreur).
bool InitPaths();

// Crée le dossier s'il n'existe pas. Faux si la création échoue ou si le chemin existe sans être un dossier.
bool EnsureDirectory(const std::wstring& path);

const Paths& GetPaths() noexcept;

}  // namespace hca
