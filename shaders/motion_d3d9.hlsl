// Vecteurs de mouvement pour le DLSS (mouvement de la caméra seulement), Direct3D 9 (vs_3_0 / ps_3_0).
// Écrit pour HomecomingAgain (GPL-3.0-or-later).
//
// Pour chaque pixel : la profondeur du moteur (texture R32F, z/w codé par le moteur, voir graphics/DepthEncoding.h) donne
// la position à l'écran en coordonnées normalisées,
// l'inverse de la caméra actuelle la ramène dans le monde, la caméra de l'image précédente la projette là où elle était.
// Sortie : écart en pixels (position précédente - position actuelle), convention des vecteurs de mouvement du DLSS.

sampler2D depthTex : register(s0);

float4 gInverseCurrent[4] : register(c0);  // inverse(caméra actuelle), une ligne par registre
float4 gPrevious[4] : register(c4);        // caméra de l'image précédente
float4 gScreen : register(c8);             // largeur, hauteur
float4 gDepth : register(c9);              // codage : facteur, décalage ; a de la projection (z/w à l'infini)

struct VsOut {
    float4 position : POSITION;
    float2 uv : TEXCOORD0;
};

VsOut MotionQuadVS(float4 position : POSITION, float2 uv : TEXCOORD0) {
    VsOut output;
    output.position = position;
    output.uv = uv;
    return output;
}

float4 Transform(float4 rows[4], float4 v) {
    return float4(dot(rows[0], v), dot(rows[1], v), dot(rows[2], v), dot(rows[3], v));
}

float4 MotionPS(float2 uv : TEXCOORD0) : COLOR0 {
    const float stored = tex2Dlod(depthTex, float4(uv, 0, 0)).r;
    // z/w décodé, borné à sa valeur à l'infini (ciel, effacé à 1.0) : le point est alors une direction (w = 0).
    const float depth = min((stored - gDepth.y) / gDepth.x, gDepth.z);
    const float4 clip = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
    // Coordonnées homogènes : pas de division par world.w, la projection suivante la fait.
    const float4 world = Transform(gInverseCurrent, clip);
    const float4 previous = Transform(gPrevious, world);
    const float2 previousNdc = previous.xy / previous.w;
    const float2 previousUv = float2(previousNdc.x * 0.5 + 0.5, 0.5 - previousNdc.y * 0.5);
    const float2 motion = (previousUv - uv) * gScreen.xy;
    return float4(motion, 0.0, 1.0);
}
