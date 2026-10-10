#pragma once

#include <d3d9.h>

namespace hca::graphics::shadows {

// Résolution des ombres multipliée par `[Graphics] ShadowResolution` / 2048.
//
// Constaté par relevé d'images (10/10/2026, Steam build 6.30) : le moteur dessine les ombres dans un atlas unique,
// texture 2048x2048 R32F (cible de rendu) avec son tampon de profondeur 2048x2048 D24S8, découpé en carrés de
// 1024x1024 par SetViewport. Les shaders qui reçoivent les ombres (variable `shadowMap`, s12) lisent l'atlas en
// coordonnées 0..1 (indépendantes de la taille) et filtrent avec un décalage d'un texel, `mLightInfo2PowFilterScale`
// .zw (registres c45 à c54) = 1/2048.
//
// Le correctif crée l'atlas en plus grand, agrandit les viewports dans la même proportion quand l'atlas est la cible,
// et divise le décalage du filtre pour qu'il reste d'un texel (ombres plus nettes, pas seulement plus lisses).

constexpr UINT kOriginalAtlasSize = 2048;

// Fonctions pures (testées par tests/unit). `size` : taille demandée par le réglage (2048 = jeu d'origine).
UINT AtlasSizeFor(int configuredSize) noexcept;
bool IsAtlasTexture(UINT width, UINT height, DWORD usage, D3DFORMAT format) noexcept;
bool IsAtlasDepth(UINT width, UINT height, D3DFORMAT format) noexcept;
D3DVIEWPORT9 ScaleViewport(const D3DVIEWPORT9& viewport, UINT atlasSize) noexcept;
// Corrige sur place les décalages de filtre trouvés dans `vectorCount` registres à partir de `startRegister`.
// Renvoie true si une valeur a été modifiée.
bool ScaleFilterConstants(UINT startRegister, float* values, UINT vectorCount, UINT atlasSize) noexcept;

// Appelés par les hooks du périphérique (graphics/SceneHooks.cpp).
void OnTextureCreated(IDirect3DTexture9* texture, UINT width, UINT height, DWORD usage, D3DFORMAT format) noexcept;
UINT CreationSize(UINT width, UINT height, DWORD usage, D3DFORMAT format, bool depth) noexcept;
void OnSetRenderTarget(DWORD index, IDirect3DSurface9* surface) noexcept;
bool AdjustViewport(const D3DVIEWPORT9& viewport, D3DVIEWPORT9& adjusted) noexcept;
UINT ActiveAtlasSize() noexcept;

}  // namespace hca::graphics::shadows
