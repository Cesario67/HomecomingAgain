#pragma once

#include <d3d9.h>

#include <cstdint>

namespace hca::graphics::jitter {

// Décalage de caméra (jitter) pour le DLSS (docs/plan-dlss.md) : à chaque image, la projection est décalée d'une
// fraction de pixel selon la suite de Halton (bases 2 et 3, 16 phases, comme le recommande NVIDIA). Le DLSS s'en sert
// pour reconstruire les détails plus fins que le pixel.
//
// Le décalage s'applique à mWorldViewProj (c0..c3 des vertex shaders, voir docs/plan-dlss.md 4 bis) : ligne 0 +=
// dx x ligne 3 et ligne 1 += dy x ligne 3, soit un déplacement constant à l'écran (en coordonnées normalisées) quelle
// que soit la profondeur. Seulement entre le début de la pré-passe de profondeur et la fin de la scène 3D (ni cartes
// d'ombres, ni post-traitements, ni interface), et seulement pour les matrices en perspective (ligne 3 non nulle sur
// x, y, z). La caméra lue pour les vecteurs de mouvement (graphics/CameraTracker.h) reste celle d'origine.
//
// Actif avec `[DLSS] ShareFrames=1` et `[DLSS] Jitter` > 0 (amplitude en pixels, 1 = normal). Sans DLSS, l'image
// tremble légèrement : désactivé par défaut.

struct Offset {
    float x = 0.0f;  // pixels, vers la droite
    float y = 0.0f;  // pixels, vers le bas
};

// Fonctions pures (testées par tests/unit).
float Halton(std::uint32_t index, std::uint32_t base) noexcept;
// Décalage de l'image `frame`, dans [-0.5, 0.5) pixel avant mise à l'échelle.
Offset OffsetForFrame(std::uint64_t frame, float amplitude) noexcept;
// Applique un décalage (en coordonnées normalisées) à une matrice de 4 registres ; false si elle n'est pas en
// perspective (laissée telle quelle).
bool ApplyToMatrix(float* registers, float ndcX, float ndcY) noexcept;

// `allowed` : faux quand le DLSS ne renvoie pas ses images au jeu (rien pour compenser le décalage).
void OnSetRenderTarget(DWORD index, IDirect3DSurface9* surface, IDirect3DSurface9* depthPassSurface,
                       bool allowed) noexcept;
void OnSceneEnd() noexcept;
void OnPresent() noexcept;

// Envoi de constantes de vertex shader du moteur : true si `copy` (même taille que `values`) a été rempli avec la
// version décalée à envoyer à la place.
bool Adjust(UINT startRegister, const float* values, UINT vectorCount, float* copy) noexcept;

// Décalage appliqué à l'image en cours (pixels), pour le DLSS.
Offset Current() noexcept;

}  // namespace hca::graphics::jitter
