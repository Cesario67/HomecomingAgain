#pragma once

#include <windows.h>

#include <array>
#include <cstdint>

namespace hca::input {

// Le menu principal du jeu ignore la croix et le stick de la manette (constaté le 13/10/2026 : les entrées arrivent
// bien au moteur par XInput, voir input/XInputHooks.h, mais le menu ne réagit qu'au clavier). Contournement : quand
// aucune scène 3D n'est dessinée (menus, chargements), l'état de la manette est traduit en touches du clavier, ajoutées
// à celles que le moteur lit par DirectInput (input/DirectInputHooks.h). En jeu, rien n'est ajouté : les flèches y
// ont d'autres fonctions (binds_pc_mjs.cfg).
//
// Correspondance : croix et stick gauche -> flèches, A et Start -> Entrée, B -> Échap.

// Codes de touches DirectInput (DIK_*).
constexpr std::uint8_t kKeyUp = 0xC8;
constexpr std::uint8_t kKeyDown = 0xD0;
constexpr std::uint8_t kKeyLeft = 0xCB;
constexpr std::uint8_t kKeyRight = 0xCD;
constexpr std::uint8_t kKeyReturn = 0x1C;
constexpr std::uint8_t kKeyEscape = 0x01;

// Touches enfoncées, indexées par code DIK.
using KeySet = std::array<bool, 256>;

// Fonction pure (testée par tests/unit) : touches correspondant à l'état de la manette. Le stick compte au-delà de la
// moitié de sa course ; `stickHeld` mémorise les directions déjà enfoncées pour ne les relâcher qu'en dessous d'un
// tiers (évite les rebonds autour du seuil).
KeySet KeysForPad(WORD buttons, SHORT stickX, SHORT stickY, KeySet& stickHeld) noexcept;

// État partagé entre la lecture de la manette (thread du moteur) et la lecture du clavier.
void OnPadState(WORD buttons, SHORT stickX, SHORT stickY) noexcept;
void OnPadDisconnected() noexcept;
// Première manette branchée (dernière lecture du moteur) ; sert aussi aux icônes de boutons (input/ButtonIcons.h).
bool PadConnected() noexcept;
// Détection du menu : appelés par les hooks Direct3D, à chaque fin de scène 3D et à chaque Present. Menu (traduction
// active) avant la première scène 3D, ou après kMenuDelayMs sans scène 3D : menu principal, long chargement.
// Le délai était d'une demi-seconde (30 images) : signalé le 16/10/2026, A ou B mettaient parfois le jeu en pause en
// pleine partie. Hypothèse : la détection de la scène décroche par moments en jeu ; A et B devenaient alors Entrée et
// Échap, qui sont « Start » et « pause » au clavier (binds_pc_mjs.cfg : COMMAND_UI_START KEY_RETURN,
// COMMAND_GAME_START KEY_ESCAPE). Avec 5 s, un décrochage passager ne suffit plus.
constexpr std::uint64_t kMenuDelayMs = 5000;
// Fonction pure (testée par tests/unit) : `lastSceneMs` = 0 si aucune scène 3D n'a encore été dessinée.
bool MenuActive(std::uint64_t nowMs, std::uint64_t lastSceneMs) noexcept;
void OnSceneDrawn() noexcept;
void OnPresent() noexcept;
bool InMenu() noexcept;
// Touches à ajouter au clavier maintenant (aucune en jeu ou si la fonction est désactivée).
KeySet CurrentKeys() noexcept;

}  // namespace hca::input
