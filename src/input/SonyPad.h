#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <Xinput.h>

namespace hca::input {

// Manettes PlayStation (DualShock 4, DualSense, DualSense Edge) sans Steam Input. Le moteur ne lit que XInput
// (XINPUT1_3, ordinaux 2 et 4) : quand aucune manette XInput n'est branchée, le mod lit la manette Sony par WinMM
// (joyGetPosEx) dans un thread à part et la présente au jeu comme une manette Xbox 360.
//
// Correspondance mesurée le 04/10/2026 sur une DualSense (Bluetooth) pour DPStabilityFix (même auteur, même
// licence) : X/Y stick gauche, Z/R stick droit, V gâchette L2, U gâchette R2 (0 au repos) ; boutons (base 0)
// 0 Carré, 1 Croix, 2 Rond, 3 Triangle, 4 L1, 5 R1, 6 L2, 7 R2, 8 Create, 9 Options, 10 L3, 11 R3 ; croix
// directionnelle sur le POV (centièmes de degré, 0 = haut). Pas encore vérifié dans ce jeu.

constexpr WORD kSonyVendorId = 0x054C;

// Plages des 6 axes WinMM d'une manette (JOYCAPS), souvent 0 à 65535.
struct AxisRanges {
    DWORD xMin, xMax, yMin, yMax, zMin, zMax, rMin, rMax, uMin, uMax, vMin, vMax;
};

AxisRanges RangesFromCaps(const JOYCAPSW& caps) noexcept;

// Fonction pure (testée par tests/unit) : état WinMM d'une manette Sony converti en état de manette Xbox 360.
XINPUT_GAMEPAD SonyToXInput(const JOYINFOEX& raw, const AxisRanges& ranges) noexcept;

// Démarre le thread de lecture (une seule fois). À appeler hors du verrou du chargeur (il charge winmm.dll).
void StartSonyPadReader() noexcept;

// Dernier état lu de la manette Sony, si une manette Sony répond. Sans allocation ni attente.
bool ReadSonyPad(XINPUT_GAMEPAD& gamepad) noexcept;

// Vrai tant que le jeu reçoit la manette Sony à la place d'une manette XInput (choix automatique des icônes).
void NoteSonyPadInUse(bool inUse) noexcept;
bool SonyPadInUse() noexcept;

}  // namespace hca::input
