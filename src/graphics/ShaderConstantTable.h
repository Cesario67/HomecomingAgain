#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hca::graphics {

// Une variable d'un shader D3D9 et les registres qu'elle occupe (ex. « WorldViewProj », c0 à c3).
struct ShaderConstant {
    std::string name;
    std::uint16_t registerSet = 0;  // 0 = bool (b), 1 = int (i), 2 = float (c), 3 = sampler (s)
    std::uint16_t registerIndex = 0;
    std::uint16_t registerCount = 0;
};

// Lit la table des constantes (bloc de commentaire « CTAB ») qu'ajoute le compilateur HLSL au bytecode d'un shader
// D3D9. Fonction pure, testée par tests/unit. Bytecode invalide ou sans table : liste vide. Les shaders compilés
// sans table (option /Qstrip_reflect) n'ont pas de noms : c'est à vérifier sur ceux du jeu.
//
// Format (documentation D3D9 « Shader Comment Token » et D3DXSHADER_CONSTANTTABLE) : un jeton de commentaire
// 0xFFFE avec la longueur en mots de 32 bits dans les bits 16 à 30, puis 'CTAB', puis l'en-tête (taille,
// créateur, version, nombre de constantes, position des descriptions...) ; les positions sont relatives au
// début de l'en-tête.
std::vector<ShaderConstant> ParseConstantTable(const std::uint32_t* tokens, std::size_t tokenCount);

}  // namespace hca::graphics
