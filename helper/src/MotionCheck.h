#pragma once

#include <cstdint>

namespace hca::dlss {

// Contrôle objectif des vecteurs de mouvement : écart moyen de luminosité entre l'image courante et l'image précédente
// lue à l'endroit indiqué par le vecteur (position précédente = position + signe x vecteur). Si les vecteurs sont justes,
// l'écart avec (signX, signY) = (1, 1) est nettement plus faible qu'avec des vecteurs nuls (useMotion = false). Les
// autres signes servent à repérer une erreur de convention. Pixels BGRA 8 bits, vecteurs en demi-flottants (pixels).
// Fonction pure (testée), échantillonnée tous les `step` pixels.
float MeanWarpError(const std::uint8_t* current, const std::uint8_t* previous, const std::uint16_t* motion,
                    std::uint32_t width, std::uint32_t height, bool useMotion, float signX, float signY,
                    std::uint32_t step) noexcept;

}  // namespace hca::dlss
