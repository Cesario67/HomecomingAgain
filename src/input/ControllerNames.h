#pragma once

#include <windows.h>

#include <string_view>

namespace hca::input {

// Noms des boutons et des axes de la manette sur l'écran des commandes (« Button 16 », « Left Y Axis » à l'origine).
// Désassemblage du 16/10/2026 (g_SilentHill.sgl Steam build 6.30) :
// - 0x100E95D0 renvoie le nom d'affichage d'une entrée (type 0 clavier, 1 axe, 2 bouton) ; seul appelant : l'écran des
//   commandes (0x1093616B, par le relais 0x10079122). Les noms du fichier de commandes (BUTTON_0, AXIS_X) viennent
//   d'une autre fonction (0x100E9570) et d'autres tables : ils ne changent pas ;
// - noms d'affichage dans .data : 6 axes en 0x111C04C0 (« Left X Axis »...), 32 boutons en 0x111C0568 (« Button 0 »...) ;
// - correspondance avec XInput (0x10A40A63 et suivantes) : boutons 0 à 3 = A, B, X, Y ; 4 et 5 inutilisés ; 6, 7 = LB,
//   RB ; 8, 9 = Start, Back ; 10, 11 = clic des sticks gauche et droit ; 12 à 15 = croix haut, bas, gauche, droite ;
//   16, 17 = gâchettes gauche et droite. Axes 0, 1 = stick gauche X, Y ; 3, 4 = stick droit X, Y.
// Le mod fait pointer ces entrées vers des noms lisibles (Xbox 360 ou PlayStation 3 selon les icônes choisies, en
// français si Steam lance le jeu en français, en anglais sinon), et remet les noms d'origine en mode PC. Chaque entrée
// est vérifiée (texte d'origine) avant d'être modifiée.
void InstallControllerNames(HMODULE engine);
// Applique les noms du type d'icônes (0 = PC : noms d'origine, 1 = Xbox 360, 2 = PS3) ; appelé quand il change.
void ApplyControllerNames(int type) noexcept;

// Fonctions pures (testées par tests/unit).
// Nom lisible d'un axe (`axis` vrai, index 0 à 5) ou d'un bouton (index 0 à 31) ; nullptr : garder le nom d'origine.
const char* ControllerInputLabel(bool axis, int index, int type, bool french) noexcept;
// Le manifeste Steam du jeu (appmanifest_19000.acf) indique-t-il le français ?
bool ManifestIsFrench(std::string_view manifest) noexcept;

}  // namespace hca::input
