#pragma once

#include <windows.h>

namespace shhf::graphics {

// Styles d'une fenêtre sans bordure ni barre de titre (fonctions pures, testées par tests/unit).
// `style` et `exStyle` sont ceux de la fenêtre du jeu en mode fenêtré.
DWORD BorderlessStyle(DWORD style) noexcept;
DWORD BorderlessExStyle(DWORD exStyle) noexcept;

// Retire le cadre de la fenêtre du jeu et la place sur tout le moniteur qui la contient. Le tampon d'image
// Direct3D (par exemple 1920x1080) est étiré par Direct3D à la taille de la fenêtre. Ne fait rien si le
// handle n'est plus une fenêtre. Renvoie true si la fenêtre a été modifiée.
bool ApplyBorderless(HWND window) noexcept;

}  // namespace shhf::graphics
