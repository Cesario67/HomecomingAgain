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
// avant les bords de l'image.
//
// Vaut pour toute résolution au-delà de 1280x720 (1080p, 1440p, 4K...).
//
// L'objet examiné après un ramassage est dessiné dans la même zone 1280x720 (relevés F11 du 18/10/2026 en 1080p et en
// 720p natif, vs_3_0 avec mWorldViewProj en c0). En 1080p, le jeu le place à côté de l'axe de la caméra, en bas à
// droite : vu de biais par une caméra à 90 degrés, il paraît étiré (signalé et vérifié en jeu). Sa projection n'est
// donc pas compensée : l'objet est replacé sur l'axe (RecenterProjection) et dessiné sur toute la cible, comme en 720p.
// On distingue la carte par sa caméra verticale (LooksStraightDown).
// Désactivable : `[Graphics] FullScreenUiClip=0`.
void Install(IDirect3DDevice9* device);

// Fonctions pures (testées par tests/unit).
// La zone x, y, largeur, hauteur est-elle la zone 720p fixe du jeu, sur une cible plus grande ? Si oui, `width` et
// `height` reçoivent la taille de la cible.
bool ExpandLegacyRect(long x, long y, long& width, long& height, long targetWidth, long targetHeight) noexcept;
// La projection (16 flottants, une ligne de sortie par registre) est-elle celle d'une caméra verticale, vue de
// dessus ? C'est le cas de la carte, pas de l'objet examiné après un ramassage, dessiné dans la même zone 720p.
bool LooksStraightDown(const float* rows) noexcept;
// Projection de l'objet examiné pour une zone agrandie (même scaleX, scaleY que ci-dessous) : l'objet, placé par le
// jeu en bas à droite de l'axe de la caméra, y est replacé. Seule sa position change, pas son orientation.
void RecenterProjection(float* rows, float scaleX, float scaleY) noexcept;
// Compense mWorldViewProj (16 flottants, une ligne de sortie x, y, z, w par registre) pour une zone d'affichage
// agrandie : `scaleX` = 1280 / largeur, `scaleY` = 720 / hauteur de la nouvelle zone.
void CompensateProjection(float* rows, float scaleX, float scaleY) noexcept;

// Perspective de la vue d'inspection des objets, cassée au-delà de 720p (signalé et mesuré le 20/10/2026, relevés
// F11 en 1280x720 et 1920x1080, couteau de combat, 8 relevés). Même zone 1280x720 que la carte, mais le moteur
// agrandit le modèle de hauteur / 720 (1,5 en 1080p) dans les quatre lignes de mWorldViewProj, sans éloigner la
// caméra : la norme du bloc 3x3 de la ligne w (c3) vaut 1,000 en 720p et 1,500 en 1080p, quelles que soient
// l'orientation et la distance. Le rapport taille / distance grimpe de 50 % et l'extrémité proche de l'objet
// grossit démesurément. La carte porte la même échelle mais à une distance de 50 (objets : 10 à 17) : elle est
// laissée telle quelle (hypothèse : sa perspective est faible et le joueur la juge correcte).
// Si la norme de la ligne w vaut `scale` (à 1 % près, `scale` > 1) et que la distance (constante de w) est
// inférieure à 30, divise par `scale` le bloc 3x3 des quatre lignes (x, y, z, w), constantes inchangées : la matrice
// redevient celle du 720p (relevés : 0,1456 / 0,5433 en 720p, 0,2184 / 0,815 en 1080p, même moment de l'animation).
// Pour un objet, à appeler avant RecenterProjection (qui ne lit que la constante de w, inchangée). Renvoie vrai si la
// matrice a été modifiée.
bool RestoreObjectPerspective(float* rows, float scale) noexcept;

}  // namespace hca::graphics::uiclip
