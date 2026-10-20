#pragma once

#include <cstdint>

namespace hca::graphics {

// Dessin des images d'une icône animée de bouton (invite à marteler), 64x64 pixels au format 0xAARRGGBB.
//
// Les icônes PC `pc_1flash` à `pc_4flash` sont deux images de 64x64 (DXT3) que le moteur fait alterner : un grand
// chiffre dans un anneau, puis le même plus petit (vérifié le 20/10/2026 par décodage des textures du pak). Les
// animations Xbox 360 et PS3 d'origine sont absentes de la version PC, le mod dessine ici les siennes (formes et
// couleurs d'après l'usage courant des manettes, aucun élément du jeu repris).
//
// type : 1 = Xbox 360 (A vert, B rouge, X bleu, Y jaune), 2 = PS3 (croix, rond, carré, triangle).
// button : 0 à 3 (A, B, X, Y ou croix, rond, carré, triangle).
// frame : 0 = grande image, 1 = petite image, plus sombre.
constexpr int kFlashIconSize = 64;
void RenderFlashFrame(int type, int button, int frame, std::uint32_t* argb) noexcept;

}  // namespace hca::graphics
