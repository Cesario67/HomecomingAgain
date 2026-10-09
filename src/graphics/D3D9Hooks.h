#pragma once

#include <windows.h>

namespace shhf::graphics {

// Intercepte Direct3DCreate9 dans SilentHill.exe, puis IDirect3D9::CreateDevice et IDirect3DDevice9::Present
// / Reset par leur vtable. Pour l'instant : journalisation des paramètres de présentation et mesure de
// cadence, sans rien modifier (le jeu est Direct3D 9 d'après les témoignages, à vérifier sur l'exécutable).
void Install(HMODULE gameModule);

}  // namespace shhf::graphics
