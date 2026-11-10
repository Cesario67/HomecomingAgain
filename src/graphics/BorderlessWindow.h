#pragma once

#include <windows.h>

namespace hca::graphics {

// Styles d'une fenêtre sans bordure ni barre de titre (fonctions pures, testées par tests/unit).
// `style` et `exStyle` sont ceux de la fenêtre du jeu en mode fenêtré.
DWORD BorderlessStyle(DWORD style) noexcept;
DWORD BorderlessExStyle(DWORD exStyle) noexcept;

// Rectangle of the borderless window for a `bufferWidth` x `bufferHeight` back buffer on `monitor`: the largest
// rectangle of the buffer's ratio that fits the monitor, centred. Direct3D 9 stretches the back buffer to the window,
// so a window of another ratio than the buffer distorts or crops the image (reported 10/11/2026: 1920x810 on a
// 1920x1080 screen was enlarged and cropped at the sides). The whole monitor when the ratios agree (within 1 %) or
// when the buffer size is unknown (0). Pure function, tested.
RECT FitWindowToMonitor(const RECT& monitor, unsigned bufferWidth, unsigned bufferHeight) noexcept;

// Retire le cadre de la fenêtre du jeu et la place sur le moniteur qui la contient, au ratio du tampon d'image
// Direct3D (`bufferWidth` x `bufferHeight`, voir FitWindowToMonitor ; 0 = tout le moniteur). Direct3D étire le
// tampon à la taille de la fenêtre. Ne fait rien si le handle n'est plus une fenêtre. Renvoie true si la fenêtre a
// été modifiée.
bool ApplyBorderless(HWND window, unsigned bufferWidth = 0, unsigned bufferHeight = 0) noexcept;

}  // namespace hca::graphics
