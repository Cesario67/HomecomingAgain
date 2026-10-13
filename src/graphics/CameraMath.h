#pragma once

#include <array>

namespace hca::graphics {

// Matrice 4x4 rangée comme le moteur l'envoie aux shaders : un registre (4 flottants) par ligne, et la ligne i donne
// la composante i du résultat (sortie = M x entrée, vecteurs colonnes). Relevé le 10/10/2026 : c0..c3 = mWorldViewProj
// (objet vers écran), c4..c6 = mWorld (objet vers monde, 3 lignes, la 4e vaut 0 0 0 1).
using Matrix4 = std::array<float, 16>;

// Fonctions pures (testées par tests/unit).
Matrix4 Multiply(const Matrix4& a, const Matrix4& b) noexcept;
// Inverse générale ; false si la matrice n'est pas inversible.
bool Inverse(const Matrix4& m, Matrix4& result) noexcept;
// Matrice de la caméra (monde vers écran) à partir des constantes d'un dessin : mWorldViewProj x inverse(mWorld).
// `world3x4` : les 3 lignes de mWorld. false si mWorld n'est pas inversible.
bool ViewProjectionFromDraw(const float* worldViewProj, const float* world3x4, Matrix4& viewProjection) noexcept;

}  // namespace hca::graphics
