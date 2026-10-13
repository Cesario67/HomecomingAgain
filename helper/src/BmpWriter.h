#pragma once

#include <cstdint>
#include <string>

namespace hca::dlss {

// Image de contrôle du jalon 3 : BMP 24 bits.
// Couleur : pixels BGRA 8 bits (format Vulkan B8G8R8A8_UNORM, celui de la scène du jeu).
bool WriteColorBmp(const std::wstring& path, const std::uint8_t* bgra, std::uint32_t width, std::uint32_t height);
// Profondeur : flottants 32 bits (z/w du moteur), étirés entre leur minimum et leur maximum (proche = clair).
bool WriteDepthBmp(const std::wstring& path, const float* depth, std::uint32_t width, std::uint32_t height);
// Vecteurs de mouvement : paires de demi-flottants (R16G16_SFLOAT, en pixels). Gris = immobile ; rouge et vert montrent
// le sens horizontal et vertical (1 pixel = 8 niveaux). `averageLength` et `maxLength` reçoivent l'amplitude en pixels.
bool WriteMotionBmp(const std::wstring& path, const std::uint16_t* halves, std::uint32_t width, std::uint32_t height,
                    float& averageLength, float& maxLength);
// Fonction pure (testée) : demi-flottant IEEE 754 vers flottant.
float HalfToFloat(std::uint16_t half) noexcept;

}  // namespace hca::dlss
