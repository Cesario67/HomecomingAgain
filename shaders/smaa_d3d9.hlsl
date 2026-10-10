// SMAA pour Direct3D 9, sans le système d'effets D3DX : chaque point d'entrée est compilé séparément par fxc
// (vs_3_0 / ps_3_0) pendant la compilation de HomecomingAgain, voir CMakeLists.txt.
//
// Les fonctions DX9_* reprennent les enveloppes du SMAA.fx de démonstration de SMAA (Jimenez et al., 2011,
// licence BSD, voir third_party/smaa/LICENSE.txt). Différences : registres explicites au lieu des techniques et
// des samplers d'effet, taille d'un pixel passée en constante (c0) au lieu d'une macro, préréglage HIGH, pas de
// masque de stencil (le calcul des poids est fait sur tous les pixels, sans effet sur le résultat).

float2 hcaPixelSize : register(c0);

#define SMAA_PIXEL_SIZE hcaPixelSize
#define SMAA_PRESET_HIGH 1
#define SMAA_HLSL_3 1
#include "SMAA.h"

// Registres des samplers, fixés côté C++ (graphics/Smaa.cpp).
sampler2D colorTex : register(s0);   // image du jeu, filtrage linéaire
sampler2D edgesTex : register(s1);   // contours (passe 1), linéaire
sampler2D areaTex : register(s2);    // table AreaTex, linéaire
sampler2D searchTex : register(s3);  // table SearchTex, point
sampler2D blendTex : register(s4);   // poids de mélange (passe 2), linéaire

void EdgeDetectionVS(inout float4 position : POSITION, inout float2 texcoord : TEXCOORD0,
                     out float4 offset[3] : TEXCOORD1) {
    SMAAEdgeDetectionVS(position, position, texcoord, offset);
}

void BlendingWeightCalculationVS(inout float4 position : POSITION, inout float2 texcoord : TEXCOORD0,
                                 out float2 pixcoord : TEXCOORD1, out float4 offset[3] : TEXCOORD2) {
    SMAABlendingWeightCalculationVS(position, position, texcoord, pixcoord, offset);
}

void NeighborhoodBlendingVS(inout float4 position : POSITION, inout float2 texcoord : TEXCOORD0,
                            out float4 offset[2] : TEXCOORD1) {
    SMAANeighborhoodBlendingVS(position, position, texcoord, offset);
}

float4 LumaEdgeDetectionPS(float2 texcoord : TEXCOORD0, float4 offset[3] : TEXCOORD1) : COLOR {
    return SMAALumaEdgeDetectionPS(texcoord, offset, colorTex);
}

float4 BlendingWeightCalculationPS(float2 texcoord : TEXCOORD0, float2 pixcoord : TEXCOORD1,
                                   float4 offset[3] : TEXCOORD2) : COLOR {
    return SMAABlendingWeightCalculationPS(texcoord, pixcoord, offset, edgesTex, areaTex, searchTex, 0);
}

float4 NeighborhoodBlendingPS(float2 texcoord : TEXCOORD0, float4 offset[2] : TEXCOORD1) : COLOR {
    return SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);
}
