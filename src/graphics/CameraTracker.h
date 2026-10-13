#pragma once

#include <d3d9.h>

#include "graphics/CameraMath.h"

namespace hca::graphics::camera {

// Matrice de la caméra principale (monde vers écran) de l'image courante et de la précédente, pour les vecteurs de
// mouvement du DLSS (graphics/MotionVectors.h).
//
// Le moteur n'envoie pas cette matrice seule : chaque dessin reçoit mWorldViewProj (c0..c3) et, pour certains vertex
// shaders, mWorld (c4..c6) ; la caméra vaut mWorldViewProj x inverse(mWorld) (graphics/CameraMath.h). Elle est prise au
// premier dessin d'un tel shader après le début de la pré-passe de profondeur (dont le shader n'a pas mWorld), pour être
// sûr d'avoir la caméra principale et pas celle d'une lumière (cartes d'ombres dessinées avant).

void OnVertexShaderCreated(const DWORD* bytecode, IDirect3DVertexShader9* shader) noexcept;
void OnSetVertexShader(IDirect3DVertexShader9* shader) noexcept;
void OnVertexShaderConstants(UINT startRegister, const float* values, UINT vectorCount) noexcept;
void OnSetRenderTarget(DWORD index, IDirect3DSurface9* surface, IDirect3DSurface9* depthPassSurface) noexcept;
void OnDraw() noexcept;
void OnPresent() noexcept;

// Caméra de l'image en cours et de la précédente ; false tant que les deux ne sont pas connues.
bool Current(Matrix4& current, Matrix4& previous) noexcept;

}  // namespace hca::graphics::camera
