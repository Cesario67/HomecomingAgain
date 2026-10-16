#pragma once

#include <cstdint>

namespace hca::input {

// État d'interface du moteur (g_SilentHill.sgl Steam build 6.30, désassemblage du 14/10/2026) : mot +0x220 de l'objet
// renvoyé par 0x104F7170, soit [0x11597320] quand son mot +0x14 a le bit 0x8000. Lu par le moteur avant de choisir
// une icône (0x109076B0, 0x10907540, 0x10986360), qui en teste le bit 0. Valeurs relevées (journaux des 14 et
// 16/10/2026) : 0 à l'écran titre, 1 brièvement après un chargement, 2 en jeu comme dans les menus : ne distingue
// pas le jeu des menus. -1 : objet absent ou illisible.
int EngineUiState() noexcept;
// Octet +0x178 de [[[0x111D2460]+0x38]+8] : non nul quand le moteur affiche les icônes de la manette. -1 : illisible.
int EnginePadFlag() noexcept;

}  // namespace hca::input
