#pragma once

#include <windows.h>

namespace hca::input {

// Diagnostic des icônes de boutons (13/10/2026) : chargement d'une texture par son nom, fonction du moteur en
// 0x10AED2B0 (repérée par sa référence à "__missingTexture", texture de remplacement quand le nom est introuvable),
// appelée uniquement par le relais 0x1004A890 (jmp). Convention cdecl, nom en troisième argument (texte). Le relais
// est redirigé vers une fonction qui note dans le journal les noms d'icônes de boutons demandés (une fois chacun),
// puis appelle l'original sans rien changer. Désactivable : `[Input] IconLog=0`.
void InstallTextureNameProbe(HMODULE engine);

// Fonction pure (testée par tests/unit) : le nom concerne-t-il une icône de bouton ?
bool IsButtonIconName(const char* name) noexcept;

}  // namespace hca::input
