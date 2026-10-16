#pragma once

#include <d3d9.h>

namespace hca::graphics::uiclip {

// Carte coupée au-delà de 1280x720 (signalé sur Reddit, vérifié le 16/10/2026 en 1920x1080). Relevé F11 sur l'écran
// de la carte : avant de la dessiner dans l'image 1920x1080, le jeu règle la zone d'affichage et la découpe sur
// 0 0 1280x720 (SetViewport puis SetScissorRect), valeurs fixes héritées du 720p : tout ce qui dépasse est coupé.
// La carte (maillage texturé et icônes) est projetée par mWorldViewProj en c0..c3 (shaders vs_15CDF738 et
// vs_1BE337F8 désassemblés : o0.x = v0.c0, o0.y = v0.c1, o0.w = v0.c3), envoyé en un appel c0 x4.
//
// Le mod étend la zone et la découpe à toute la cible, et compense la projection pendant ce temps : chaque point
// retombe au même pixel qu'avec la zone 1280x720 (la carte garde sa taille et sa place), mais rien n'est plus coupé
// avant les bords de l'image. Vaut pour toute résolution au-delà de 1280x720 (1080p, 1440p, 4K...).
// Désactivable : `[Graphics] FullScreenUiClip=0`.
void Install(IDirect3DDevice9* device);

// Fonctions pures (testées par tests/unit).
// La zone x, y, largeur, hauteur est-elle la zone 720p fixe du jeu, sur une cible plus grande ? Si oui, `width` et
// `height` reçoivent la taille de la cible.
bool ExpandLegacyRect(long x, long y, long& width, long& height, long targetWidth, long targetHeight) noexcept;
// Compense mWorldViewProj (16 flottants, une ligne de sortie x, y, z, w par registre) pour une zone d'affichage
// agrandie : `scaleX` = 1280 / largeur, `scaleY` = 720 / hauteur de la nouvelle zone.
void CompensateProjection(float* rows, float scaleX, float scaleY) noexcept;

}  // namespace hca::graphics::uiclip
