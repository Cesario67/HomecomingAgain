#pragma once

#include <windows.h>

namespace hca::input {

// Icônes de boutons affichées par l'interface. Le moteur choisit entre trois jeux de textures (PC, Xbox 360, PS3)
// avec une fonction qui renvoie un type de manette, d'après un texte de sa configuration (désassemblage du
// 13/10/2026, g_SilentHill.sgl Steam build 6.30) :
// - 0x104EC050 : compare ce texte à "x360" (renvoie 1), puis à "ps3" (renvoie 2), sinon renvoie 0 ;
// - appelée par 0x10510F29 (texture d'une icône : variantes PC, Xbox 360 et PS3 côte à côte, +0x88, +0x8C, +0x90)
//   et par 0x10906EEC / 0x10906F2D (icônes des sticks, Xbox 360 sauf si 2) ;
// - sur PC le texte ne vaut ni l'un ni l'autre : type 0, icônes PC (« 1 » entouré de rouge pour A, constaté).
// Le début de la fonction est remplacé par un saut vers la nôtre, après vérification de ses octets d'origine.
// Réglage `[Input] ButtonIcons` : 0 = auto (Xbox 360 si une manette est branchée, sinon PC), 1 = PC (jeu d'origine,
// rien n'est modifié), 2 = Xbox 360, 3 = PS3.
void InstallButtonIcons(HMODULE engine);

// Fonction pure (testée par tests/unit) : type de manette rendu au moteur (0 = PC, 1 = Xbox 360, 2 = PS3).
int IconTypeFor(int setting, bool padConnected) noexcept;
// Fonction pure (testée) : nom de l'icône du bouton 0 à 3 (A, B, X, Y), fixe (0 à 3) ou animée (4 à 7), pour le
// type 0 = PC, 1 = Xbox 360, 2 = PS3.
const char* FaceIconName(int type, int button) noexcept;
// Met à jour les noms des icônes A, B, X, Y selon le réglage et la manette (appelé à chaque Present).
void RefreshButtonIcons() noexcept;

}  // namespace hca::input
