// Vecteurs de mouvement pour le DLSS (mouvement de la caméra seulement), Direct3D 9 (vs_3_0 / ps_3_0).
// Écrit pour HomecomingAgain (GPL-3.0-or-later).
//
// Pour chaque pixel : la profondeur du moteur (z/w, texture R32F) donne la position à l'écran en coordonnées normalisées,
// l'inverse de la caméra actuelle la ramène dans le monde, la caméra de l'image précédente la projette là où elle était.
// Sortie : écart en pixels (position précédente - position actuelle), convention des vecteurs de mouvement du DLSS.

sampler2D depthTex : register(s0);

float4 gInverseCurrent[4] : register(c0);  // inverse(caméra actuelle), une ligne par registre
float4 gPrevious[4] : register(c4);        // caméra de l'image précédente
float4 gScreen : register(c8);             // largeur, hauteur

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
    const float depth = tex2Dlod(depthTex, float4(uv, 0, 0)).r;
    const float4 clip = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
    float4 world = Transform(gInverseCurrent, clip);
    world /= world.w;
    const float4 previous = Transform(gPrevious, world);
    const float2 previousNdc = previous.xy / previous.w;
    const float2 previousUv = float2(previousNdc.x * 0.5 + 0.5, 0.5 - previousNdc.y * 0.5);
    const float2 motion = (previousUv - uv) * gScreen.xy;
    return float4(motion, 0.0, 1.0);
}
