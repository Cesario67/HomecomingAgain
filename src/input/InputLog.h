#pragma once

#include <windows.h>


namespace hca::input {

// DIAGNOSTIC TEMPORAIRE (16/10/2026) : pauses intempestives en appuyant sur A en pleine partie, alors que la
// traduction manette vers clavier était arrêtée (journal : mode « en jeu »). Note, avec l'heure : les appuis sur les
// boutons lus par XInput et chaque
// changement d'état du moteur (input/EngineUiState.h).
// `[Debug] InputLog=0` le coupe. À retirer une fois la cause trouvée.
void LogPadButtons(WORD buttons) noexcept;
void LogEngineState() noexcept;

}  // namespace hca::input

namespace hca::input {

// DIAGNOSTIC TEMPORAIRE (17/10/2026, icône absente de l'introduction) : icônes demandées par commande et textures
// d'icônes appliquées aux éléments d'interface, une fois par combinaison.
void LogIconRequest(unsigned command, const char* engineName, const char* shownName, bool padMode) noexcept;
void LogTextureApplied(const char* name, const char* replacement) noexcept;

}  // namespace hca::input
