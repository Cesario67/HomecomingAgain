#pragma once

#include <d3d9.h>

#include <cstddef>
#include <cstdint>

namespace hca::graphics::flashicons {

// Icônes animées de l'invite à marteler (QTE) aux couleurs de la manette.
//
// Constat (20/10/2026, Steam build 6.30) : l'invite est l'élément d'interface `StrugglePrompt`, dont le script
// (GLOBAL.PAK, contrôleur StrugglePromptAnimationList) ne garde que les textures PC `pc_1flash` à `pc_4flash` : les
// variantes Xbox 360 et PS3 sont en commentaire et leurs textures absentes. Chaque `pc_Nflash` est formée de deux
// textures Direct3D 64x64 DXT3 à un seul niveau (grande et petite image), remplies au chargement par LockRect /
// UnlockRect, que le moteur alterne.
//
// Le mod reconnaît ces huit textures à l'empreinte de leurs pixels au premier UnlockRect (table `IdentifyFlashFrame`),
// garde une copie des pixels d'origine, et les réécrit avec ses propres images (graphics/FlashIconArt.h) quand la
// manette est le mode actif, ou remet l'original (icône PC) sinon. `[Input] AnimatedIcons=0` coupe la fonction.
void Install(IDirect3DDevice9* device);
// Type d'icônes courant (0 = PC, 1 = Xbox 360, 2 = PS3) : réécrit les textures reconnues si le type a changé.
// Appelé depuis le thread de rendu (Present).
void SetIconType(int type) noexcept;

// Fonctions pures (testées).
// FNV-1a 32 bits.
std::uint32_t Fnv1a(const std::uint8_t* data, std::size_t size) noexcept;
// Empreinte (Fnv1a des 4096 octets du niveau 0) d'une des huit textures pc_1flash à pc_4flash du jeu d'origine :
// `button` 0 à 3 (pc_1flash = A ... pc_4flash = Y), `frame` 0 ou 1. Faux si l'empreinte est inconnue.
bool IdentifyFlashFrame(std::uint32_t hash, int& button, int& frame) noexcept;

}  // namespace hca::graphics::flashicons
