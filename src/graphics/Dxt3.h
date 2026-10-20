#pragma once

#include <cstddef>
#include <cstdint>

namespace hca::graphics {

// Compression DXT3 (BC2) : alpha explicite sur 4 bits, couleur en deux points 5:6:5 et quatre niveaux. Pixels au format
// 0xAARRGGBB, ligne par ligne ; largeur et hauteur multiples de 4. La sortie fait (largeur / 4) * (hauteur / 4) * 16
// octets, blocs rangés ligne par ligne (comme la texture D3DFMT_DXT3 de Direct3D 9).
std::size_t Dxt3Size(int width, int height) noexcept;
void EncodeDxt3(const std::uint32_t* argb, int width, int height, std::uint8_t* out) noexcept;
// Décompression (pour les tests et le contrôle visuel).
void DecodeDxt3(const std::uint8_t* data, int width, int height, std::uint32_t* argb) noexcept;

}  // namespace hca::graphics
