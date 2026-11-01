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

// Is the fixed 1280x720 area extended right now (render thread only)? Other fixes leave its projections alone.
bool LegacyAreaActive() noexcept;

// Fonctions pures (testées par tests/unit).
// Multiplies the x output row (the first 4 floats of 16) by `factor`: brings a projection built with another aspect
// back to the aspect the interface layout was made for.
void ScaleXRow(float* rows, float factor) noexcept;
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

// Journal documents and photos render black above 720p (reported by a player: fine at 1080p for them, dark at 1440p,
// black at 4K; black at 1080p here too). F11 captures of 24/10/2026 at 1280x720, 1920x1080 and 2560x1440 (verified):
// the picture is a lit quad (ps_1BE99E50, 4 vertices, 1280x720 viewport) with no ambient light (c8..c13 and c97 at 0)
// and a single spotlight whose position (c20) is (0, 0, -25) at 720p, (-22.222, 12.5, -25) at 1080p and
// (-44.444, 25, -25) at 1440p, i.e. (-(W - 1280) / 28.8, (H - 720) / 28.8, -25). Hypothesis: the light is pushed
// away from the picture and no longer lights it (checked in game only after this fix).
// If the c20 position carries exactly that offset for a `targetWidth` x `targetHeight` target (within 1 %), puts x and
// y back to 0 (the 720p value). Returns true if the position was changed.
bool RestoreInspectionLight(float* position, long targetWidth, long targetHeight) noexcept;

}  // namespace hca::graphics::uiclip
