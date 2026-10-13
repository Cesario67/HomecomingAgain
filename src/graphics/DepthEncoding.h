#pragma once

#include <d3d9.h>

namespace hca::graphics::depth {

// Codage de la texture de profondeur du moteur (pré-passe R32F, voir docs/plan-dlss.md). Son pixel shader écrit
// (z/w) x c98.x + c98.y, c98 étant mCommonDepthCoefficients : [0.9 0 0.1 0.9] au relevé du 13/10/2026 (shader
// ps_1A5D2188 désassemblé avec fxc /dumpbin, viewport de la passe z 0..0.9). La valeur lue n'est donc pas z/w : z/w =
// (valeur - y) / x. L'effacement à 1.0 marque le ciel.
struct Encoding {
    float scale = 0.9f;  // valeur relevée, utilisée tant que le moteur n'a pas envoyé c98
    float bias = 0.0f;
};

constexpr UINT kCoefficientsRegister = 98;

// Reconnaît c98 dans un envoi de constantes de pixel shader (fonction pure, testée par tests/unit).
bool ParseEncoding(UINT startRegister, const float* values, UINT vectorCount, Encoding& encoding) noexcept;

// Appelé pour chaque envoi de constantes de pixel shader du moteur.
void OnPixelShaderConstants(UINT startRegister, const float* values, UINT vectorCount) noexcept;

// Dernier codage envoyé par le moteur.
Encoding Current() noexcept;

}  // namespace hca::graphics::depth
