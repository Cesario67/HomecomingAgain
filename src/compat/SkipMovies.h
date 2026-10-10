#pragma once

#include <windows.h>

#include <string_view>

namespace hca::compat {

// Option de test `[Debug] SkipMovies` : le moteur ne peut plus ouvrir ses vidéos Bink (Engine\movies\*.bik, dont la
// cinématique du brancard au début du prologue), pour arriver plus vite en jeu pendant les essais. Les fichiers du jeu
// ne sont pas modifiés : CreateFileA/W du moteur répondent « fichier introuvable » pour ces vidéos.
//
// Fonction pure (testée par tests/unit) : le chemin désigne-t-il une vidéo Bink ?
bool IsMoviePath(std::wstring_view path) noexcept;

void InstallSkipMovies(HMODULE engineModule);

}  // namespace hca::compat
