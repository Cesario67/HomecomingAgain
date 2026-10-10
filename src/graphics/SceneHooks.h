#pragma once

#include <d3d9.h>

namespace hca::graphics::scene {

// Interception des méthodes du périphérique utiles aux effets insérés dans le rendu du moteur (résolution des
// ombres, SSAO). Les hooks ne font que déléguer à graphics/ShadowResolution et graphics/Ssao, puis appellent la
// fonction d'origine. À appeler une fois, juste après CreateDevice (avant que le moteur crée ses textures).
void Install(IDirect3DDevice9* device);

}  // namespace hca::graphics::scene
