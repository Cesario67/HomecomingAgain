#pragma once

#include <windows.h>

#include <cstdint>

namespace hca::compat {

// Touche pour passer les cinématiques en vidéo (`[Movies] SkipKey`, Suppr par défaut, 0 = désactivé).
//
// Constaté le 10/10/2026 (Steam build 6.30) : les cinématiques sont des vidéos Bink (Engine\movies\*.bik) lues par
// binkw32.dll, que le moteur charge à la demande et dont il récupère les fonctions par GetProcAddress (_BinkOpen@8,
// _BinkNextFrame@4, _BinkGoto@12...). Le mod enveloppe _BinkNextFrame@4 : touche pressée, fenêtre du jeu active, il
// saute à la dernière image (BinkGoto), et le moteur voit la vidéo se terminer. Les cinématiques calculées en temps
// réel par le moteur ne sont pas concernées.

// Début de la structure BINK publique (bink.h de RAD : largeur, hauteur, nombre d'images, image courante).
struct BinkHeader {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t frames;
    std::uint32_t frameNum;
};

// Fonction pure (testée par tests/unit) : faut-il sauter à la fin ? Pas si la vidéo y est déjà ou est vide.
bool ShouldSkipTo(const BinkHeader& bink, bool keyPressed, std::uint32_t& targetFrame) noexcept;

void InstallMovieSkip(HMODULE engineModule);

}  // namespace hca::compat
