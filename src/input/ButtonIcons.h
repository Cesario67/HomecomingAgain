#pragma once

#include <windows.h>

namespace hca::input {

// Icônes de boutons affichées par l'interface (désassemblage des 13 et 14/10/2026, g_SilentHill.sgl Steam build 6.30).
//
// Type de manette : le moteur choisit entre trois jeux de textures (PC, Xbox 360, PS3) avec une fonction qui
// renvoie un type d'après un texte de sa configuration :
// - 0x104EC050 : compare ce texte à "x360" (renvoie 1), puis à "ps3" (renvoie 2), sinon renvoie 0 ;
// - appelée par 0x10510F29 (texture d'une icône : variantes PC, Xbox 360 et PS3 côte à côte, +0x88, +0x8C, +0x90)
//   et par 0x10906EEC / 0x10906F2D (icônes des sticks, Xbox 360 sauf si 2) ;
// - sur PC le texte ne vaut ni l'un ni l'autre : type 0, icônes PC.
// Le début de la fonction est remplacé par un saut vers la nôtre, après vérification de ses octets d'origine.
//
// Boutons A, B, X, Y : le moteur les nomme toujours "shv_buttonpccontrol1" à "4" (chiffres entourés, constaté) ou
// "pc_1flash" à "pc_4flash" (variante animée), noms renvoyés par 0x10986360. Renvoyer d'autres noms ne suffit pas :
// l'invite d'action en jeu (0x10907540) cherche ce nom dans une liste d'éléments créés au chargement de l'interface,
// qui ne contient que les noms PC (constaté : invite vide). On garde donc les noms PC et on change l'image : tout
// élément d'interface change de texture par son nom avec 0x1050E5C0 (thiscall, un argument : texte du moteur), via
// le relais 0x10054DCC (jmp), redirigé après vérification. Un nom PC y est remplacé par l'icône Xbox 360 ou PS3
// correspondante (présentes dans les données du jeu, Engine\assets_pc_b.xml).
//
// Réglage `[Input] ButtonIcons` : 0 = auto (Xbox 360 si une manette est branchée, sinon PC), 1 = PC (jeu d'origine,
// rien n'est modifié), 2 = Xbox 360, 3 = PS3.
void InstallButtonIcons(HMODULE engine);

// Fonction pure (testée par tests/unit) : type de manette rendu au moteur (0 = PC, 1 = Xbox 360, 2 = PS3).
int IconTypeFor(int setting, bool padConnected) noexcept;
// Fonction pure (testée) : nom de l'icône du bouton 0 à 3 (A, B, X, Y), fixe (0 à 3) ou animée (4 à 7), pour le
// type 0 = PC, 1 = Xbox 360, 2 = PS3.
const char* FaceIconName(int type, int button) noexcept;
// Fonction pure (testée) : icône à afficher à la place de la texture PC `name` pour le type 1 ou 2 ; nullptr si
// `name` n'est pas une icône A, B, X, Y du PC ou si le type est 0.
const char* ReplacementIconName(const char* name, int type) noexcept;
// Met à jour le type d'icônes selon le réglage et la manette (appelé à chaque Present).
void RefreshButtonIcons() noexcept;

}  // namespace hca::input
