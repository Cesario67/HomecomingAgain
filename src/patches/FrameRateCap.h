#pragma once

#include <windows.h>

namespace hca::patches {

// Durée d'image visée par le limiteur du moteur, en millisecondes, pour une cadence donnée (fonction pure,
// testée par tests/unit). `fps` hors de [1, 1000] : 0 (pas de durée valide).
float FramePeriodMs(int fps) noexcept;

// Remplace le plafond de 30 i/s codé en dur dans le moteur par `[Frames] TargetFps`.
//
// g_SilentHill.sgl (horodatage PE 0x490F50BF, Steam build 6.30), vérifié par désassemblage le 10/10/2026 :
// la fonction 0x10A4CC90 attend, à chaque image, que la durée écoulée depuis la précédente atteigne la valeur
// lue par `fld dword ptr ds:[0x1108E804]` (instruction en 0x10A4CC99, octets D9 05 04 E8 08 11). Ce flottant
// de la section en lecture seule vaut 33,32333 ms, quel que soit fpsLimit de vars_pc.cfg : c'est le verrou
// 30 i/s. Repéré grâce au relevé des attentes (SleepEx appelé depuis 0x1080BAF6, 93 % du temps de rendu).
// Le correctif redirige l'opérande de cette seule instruction vers une variable du mod ; le fichier du jeu
// n'est pas modifié. Si les octets ne correspondent pas (autre version), rien n'est fait.
void ApplyFrameRateCap(HMODULE engineModule);

}  // namespace hca::patches
