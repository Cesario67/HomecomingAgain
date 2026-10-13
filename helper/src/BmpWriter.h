#pragma once

#include <cstdint>
#include <string>

namespace hca::dlss {

// Image de contrôle du jalon 3 : BMP 24 bits.
// Couleur : pixels BGRA 8 bits (format Vulkan B8G8R8A8_UNORM, celui de la scène du jeu).
bool WriteColorBmp(const std::wstring& path, const std::uint8_t* bgra, std::uint32_t width, std::uint32_t height);
// Profondeur : flottants 32 bits (z/w du moteur), étirés entre leur minimum et leur maximum (proche = clair).
bool WriteDepthBmp(const std::wstring& path, const float* depth, std::uint32_t width, std::uint32_t height);

}  // namespace hca::dlss
