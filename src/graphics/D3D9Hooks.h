#pragma once

#include <windows.h>

namespace shhf::graphics {

// Intercepte Direct3DCreate9 dans le moteur (g_SilentHill.sgl, qui importe d3d9.dll), puis IDirect3D9::CreateDevice et IDirect3DDevice9::Present
// / Reset par leur vtable. Pour l'instant : journalisation des paramètres de présentation et mesure de
// cadence, sans rien modifier.
void Install(HMODULE engineModule);

}  // namespace shhf::graphics
