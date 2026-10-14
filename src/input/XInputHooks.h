#pragma once

#include <windows.h>

namespace hca::input {

// Manette lue par le moteur : g_SilentHill.sgl lit les manettes par XINPUT1_3.dll, importée par ordinaux (vérifié,
// docs/analyse-shh-exe.md) : 2 = XInputGetState. Interceptée dans l'IAT du moteur et transmise telle quelle ; l'état
// de la première manette alimente la traduction en touches dans les menus et le choix automatique des icônes
// (input/MenuBridge.h), et son stick droit commande le correctif de caméra (patches/CameraFix.h). Installée si
// `[Input] MenuPad=1`, `ButtonIcons=0` (auto) ou `[Camera] CameraFix=1`.
void InstallXInputHooks(HMODULE engine);

}  // namespace hca::input
