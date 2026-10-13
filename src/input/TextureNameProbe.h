#pragma once

#include <cstdint>

namespace hca::input {

// Diagnostic des icônes de boutons (14/10/2026, g_SilentHill.sgl Steam build 6.30), alimenté par la redirection du
// changement de texture des éléments d'interface (voir ButtonIcons.h). Note dans le journal, une fois par
// combinaison, chaque icône de bouton appliquée : nom demandé, remplaçant éventuel, appelant, poignée de texture
// (+0x80 de l'élément) et ressource chargée (+0x58 de la poignée, lue par 0x1050E690 ; hypothèse : 0 si absente).
// Désactivable : `[Input] IconLog=0`.
void OnTextureApplied(void* item, const char* requested, const char* applied, std::uintptr_t caller) noexcept;

// Note dans le journal chaque changement du mode d'entrée qui décide entre icônes clavier et manette (à chaque
// Present ; n'écrit que lors d'un changement).
void WatchInputMode() noexcept;

// Fonction pure (testée par tests/unit) : le nom concerne-t-il une icône de bouton ?
bool IsButtonIconName(const char* name) noexcept;

}  // namespace hca::input
