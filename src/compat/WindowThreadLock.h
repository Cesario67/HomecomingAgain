#pragma once

#include <windows.h>

namespace shhf::compat {

// Évite l'interblocage du chargement en fenêtré avec l'overlay Steam (voir docs/analyse-shh-exe.md, 10/10/2026).
//
// Pendant un chargement, le thread de l'écran de chargement présente les images en tenant un verrou du moteur
// (section critique), tandis que le thread de la fenêtre attend ce verrou (EnterCriticalSection appelé en
// 0x10891F07). En fenêtré, l'overlay Steam, appelé dans Present depuis le thread de chargement, peut envoyer de façon
// synchrone des messages à la fenêtre (SendMessage, ou fonctions de fenêtre appelées depuis un autre thread) : il
// attend le thread de la fenêtre, qui attend le verrou. Interblocage.
//
// Correctif : EnterCriticalSection du moteur est intercepté. Pour le seul thread de la fenêtre, quand le verrou est
// déjà pris, l'attente continue de traiter les messages ENVOYÉS par les autres threads (comme Windows le fait
// pendant un SendMessage), sans toucher aux messages postés (pas de traitement des entrées au milieu du code du
// moteur). Les autres threads et le cas sans contention passent directement à la fonction d'origine.
void InstallWindowThreadLock(HMODULE engineModule);

// Thread qui possède la fenêtre du jeu (celui qui crée le périphérique Direct3D).
void SetWindowThread(DWORD threadId) noexcept;

}  // namespace shhf::compat
