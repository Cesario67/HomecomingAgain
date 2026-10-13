#pragma once

#include <windows.h>

namespace hca::input {

// Lecture du clavier par le moteur : DINPUT8.dll!DirectInput8Create (IAT de g_SilentHill.sgl, vérifié,
// docs/analyse-shh-exe.md) -> IDirectInput8::CreateDevice (vtable, index 3) -> pour GUID_SysKeyboard,
// IDirectInputDevice8::GetDeviceState (index 9) et GetDeviceData (index 10). Les touches de la manette traduites
// dans les menus (input/MenuBridge.h) y sont ajoutées : état des 256 touches pour GetDeviceState, événements
// d'appui et de relâchement pour GetDeviceData (lecture en mémoire tampon). Le journal indique laquelle des deux le
// moteur utilise. Désactivable : `[Input] MenuPad=0`.
void InstallDirectInputHooks(HMODULE engine);

}  // namespace hca::input
