#pragma once

#include <d3d9.h>

namespace hca::graphics::grain {

// Intensité du grain de pellicule du jeu, réglable par `[Graphics] FilmGrain` (en %, 100 = jeu d'origine, 0 = aucun).
//
// Constaté par relevé d'images et désassemblage (10/10/2026, Steam build 6.30) : le grain est ajouté par un seul pixel
// shader de fin de rendu, reconnaissable à sa variable `grainTexture` (s2). Sa dernière instruction calcule
// `couleur + couleur * grain * c14.z`, où c14 = `mNoiseDimensionIntensity` (c14.z = 1 dans le prologue). Le correctif
// multiplie c14.z par le réglage pendant que ce shader est actif ; la teinte et le contraste (c0, c1) ne changent pas.

constexpr UINT kIntensityRegister = 14;

// Fonctions pures (testées par tests/unit).
bool IsGrainShader(const DWORD* bytecode) noexcept;
// Corrige sur place c14.z dans un envoi de `vectorCount` registres à partir de `startRegister`. Renvoie true si modifié.
bool ScaleIntensity(UINT startRegister, float* values, UINT vectorCount, float scale) noexcept;

// Appelés par les hooks du périphérique (graphics/SceneHooks.cpp).
bool Active() noexcept;
void OnPixelShaderCreated(const DWORD* bytecode, IDirect3DPixelShader9* shader) noexcept;
// Renvoie true si le shader du grain devient (ou cesse d'être) actif : c14 doit alors être renvoyé, corrigé ou non.
bool OnSetPixelShader(IDirect3DPixelShader9* shader) noexcept;
bool GrainShaderBound() noexcept;
// Dernière valeur de c14 envoyée par le jeu (non corrigée), pour la renvoyer au changement de shader.
void RememberIntensity(UINT startRegister, const float* values, UINT vectorCount) noexcept;
bool LastIntensity(float out[4]) noexcept;
float Scale() noexcept;

}  // namespace hca::graphics::grain
