#pragma once

#include <d3d9.h>

#include "graphics/CameraMath.h"

namespace hca::graphics::camera {

// Matrice de la caméra principale (monde vers écran) de l'image courante et de la précédente, pour les vecteurs de
// mouvement du DLSS (graphics/MotionVectors.h).
//
// Le moteur n'envoie pas cette matrice seule : chaque dessin reçoit mWorldViewProj (c0..c3) et, pour certains vertex
// shaders, mWorld (c4..c6) ; la caméra vaut mWorldViewProj x inverse(mWorld) (graphics/CameraMath.h). Elle est prise au
// dans les dessins d'un tel shader après le début de la pré-passe de profondeur (dont le shader n'a pas mWorld), pour
// exclure les cartes d'ombres dessinées avant. Les matrices qui ne sont pas des caméras en perspective (interface 2D,
// effets plein écran) sont rejetées, et la caméra retenue est celle sur laquelle le plus de dessins s'accordent.
// Constaté le 13/10/2026 : le premier dessin qualifié était une projection orthographique d'interface.

void OnVertexShaderCreated(const DWORD* bytecode, IDirect3DVertexShader9* shader) noexcept;
void OnSetVertexShader(IDirect3DVertexShader9* shader) noexcept;
void OnVertexShaderConstants(UINT startRegister, const float* values, UINT vectorCount) noexcept;
void OnSetRenderTarget(DWORD index, IDirect3DSurface9* surface, IDirect3DSurface9* depthPassSurface) noexcept;
void OnDraw() noexcept;
void OnPresent() noexcept;

// Fonctions pures (testées par tests/unit).
bool LooksLikePerspectiveCamera(const Matrix4& m) noexcept;
bool SameCamera(const Matrix4& a, const Matrix4& b) noexcept;

// Fin de la scène 3D : choisit la caméra de l'image (appelé par Current).
void OnSceneEnd() noexcept;

// Caméra de l'image en cours et de la précédente retenue par CommitFrame ; false tant que les deux ne sont pas connues.
bool Current(Matrix4& current, Matrix4& previous) noexcept;

// La caméra de l'image en cours devient la « précédente ». Appelé quand l'image a été transmise au processus DLSS (ou
// qu'elle n'avait pas de vecteurs) : quand une image est sautée, les vecteurs de la suivante doivent se rapporter à la
// dernière image transmise. Constaté le 13/10/2026 : 12 à 21 images sautées au début du jeu.
void CommitFrame() noexcept;

// Changement de plan depuis la caméra précédente (cinématique, porte, chargement) : le DLSS doit alors oublier son
// historique. Seuils : rotation de plus de kCutDegrees ou déplacement de plus de kCutDistance en une image (relevés du
// 13/10/2026 : au plus 6 à 7 degrés par image à la souris, moins d'une unité par image en marchant), ou plus de 5
// images sans scène transmise.
constexpr float kCutDegrees = 15.0f;
constexpr float kCutDistance = 20.0f;
bool IsCut() noexcept;

}  // namespace hca::graphics::camera
