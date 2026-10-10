// Occlusion ambiante en espace écran (SSAO) pour Silent Hill: Homecoming, Direct3D 9 (vs_3_0 / ps_3_0).
// Écrit pour HomecomingAgain (GPL-3.0-or-later). Méthode classique : échantillons dans un hémisphère orienté par la
// normale reconstruite à partir de la profondeur, puis flou qui respecte les bords (profondeur).
//
// Entrées (vérifiées par relevé d'images, voir src/graphics/Ssao.h) :
// - depthTex : la texture R32F que le moteur remplit avant la scène, valeur z/w de la projection (1 = ciel).
// - gProj : xScale, yScale, A, B de la projection du jeu (z/w = A + B / zVue).

sampler2D depthTex : register(s0);
sampler2D aoTex : register(s1);

float4 gProj : register(c0);     // xScale, yScale, A, B
float4 gScreen : register(c1);   // 1/largeur, 1/hauteur, largeur, hauteur
float4 gParams : register(c2);   // rayon (unités du jeu), intensité, début et fin du fondu en distance

struct VsOut {
    float4 position : POSITION;
    float2 uv : TEXCOORD0;
};

VsOut QuadVS(float4 position : POSITION, float2 uv : TEXCOORD0) {
    VsOut output;
    output.position = position;
    output.uv = uv;
    return output;
}

float RawDepth(float2 uv) {
    return tex2Dlod(depthTex, float4(uv, 0, 0)).r;
}

float ViewZ(float raw) {
    return gProj.w / (raw - gProj.z);
}

float3 ViewPos(float2 uv, float z) {
    return float3((uv.x * 2.0 - 1.0) / gProj.x * z, (1.0 - uv.y * 2.0) / gProj.y * z, z);
}

float3 ViewPosAt(float2 uv) {
    return ViewPos(uv, ViewZ(RawDepth(uv)));
}

// Bruit en gradient entrelacé (Jimenez 2014) : rotation différente pour chaque pixel, effacée par le flou.
float InterleavedNoise(float2 pixel) {
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

static const int kSampleCount = 12;
static const float3 kKernel[kSampleCount] = {
    float3(0.536, 0.214, 0.293), float3(-0.412, 0.337, 0.198), float3(0.121, -0.583, 0.402),
    float3(-0.237, -0.198, 0.112), float3(0.689, -0.142, 0.488), float3(-0.611, -0.402, 0.355),
    float3(0.084, 0.733, 0.271), float3(-0.115, 0.091, 0.871), float3(0.302, 0.401, 0.624),
    float3(-0.802, 0.155, 0.402), float3(0.215, -0.233, 0.168), float3(-0.344, 0.611, 0.523)};

float4 OcclusionPS(float2 uv : TEXCOORD0, float2 pixel : VPOS) : COLOR0 {
    const float raw = RawDepth(uv);
    // Ciel (1), rien de dessiné, ou valeur hors de la plage attendue : pas d'occlusion plutôt qu'une valeur fausse.
    if (!(raw > 0.0001 && raw < 0.99999)) {
        return 1.0;
    }
    const float3 p = ViewPos(uv, ViewZ(raw));

    // Normale : différence avec le voisin le plus proche en profondeur de chaque côté (évite les faux bords).
    const float2 px = float2(gScreen.x, 0.0);
    const float2 py = float2(0.0, gScreen.y);
    const float3 left = p - ViewPosAt(uv - px);
    const float3 right = ViewPosAt(uv + px) - p;
    const float3 up = p - ViewPosAt(uv - py);
    const float3 down = ViewPosAt(uv + py) - p;
    const float3 dx = abs(left.z) < abs(right.z) ? left : right;
    const float3 dy = abs(up.z) < abs(down.z) ? up : down;
    const float3 rawNormal = cross(dy, dx);
    float3 normal = dot(rawNormal, rawNormal) > 1e-12 ? normalize(rawNormal) : float3(0.0, 0.0, -1.0);
    // La normale doit faire face à la caméra (p va de la caméra au point) : sinon les échantillons partent dans la
    // surface et tout paraît caché (image noire constatée le 10/10/2026).
    normal = dot(normal, p) > 0.0 ? -normal : normal;

    const float angle = InterleavedNoise(pixel) * 6.2831853;
    const float3 random = float3(cos(angle), sin(angle), 0.0);
    const float3 tangent = normalize(random - normal * dot(random, normal));
    const float3 bitangent = cross(normal, tangent);

    const float radius = gParams.x;
    float occlusion = 0.0;
    for (int i = 0; i < kSampleCount; ++i) {
        const float3 offset = kKernel[i];
        const float3 s = p + (tangent * offset.x + bitangent * offset.y + normal * offset.z) * radius;
        const float2 suv = float2(s.x * gProj.x / s.z * 0.5 + 0.5, 0.5 - s.y * gProj.y / s.z * 0.5);
        const float sceneZ = ViewZ(RawDepth(suv));
        const float range = saturate(radius / max(abs(p.z - sceneZ), 0.0001));
        occlusion += (sceneZ < s.z - radius * 0.03 ? 1.0 : 0.0) * range;
    }
    float ao = 1.0 - occlusion / kSampleCount;
    ao = ao >= 0.0 && ao <= 1.0 ? ao : 1.0;  // NaN ou valeur aberrante : pas d'effet
    ao = pow(saturate(ao), gParams.y);
    // Fondu avec la distance : le brouillard du jeu masque déjà le lointain.
    ao = lerp(ao, 1.0, saturate((p.z - gParams.z) / max(gParams.w - gParams.z, 1.0)));
    // Plancher : l'effet n'assombrit jamais de plus de 60 %, même mal réglé.
    ao = max(ao, 0.4);
    return float4(ao, ao, ao, 1.0);
}

// Flou 4x4 pondéré par l'écart de profondeur, puis sortie multipliée sur l'image (mélange ZERO / SRCCOLOR,
// ou remplacement de l'image en mode de réglage, voir Ssao.cpp).
float4 BlurPS(float2 uv : TEXCOORD0) : COLOR0 {
    const float centerZ = ViewZ(RawDepth(uv));
    float sum = 0.0;
    float weights = 0.0;
    for (int y = -2; y < 2; ++y) {
        for (int x = -2; x < 2; ++x) {
            const float2 suv = uv + (float2(x, y) + 0.5) * gScreen.xy;
            const float z = ViewZ(RawDepth(suv));
            const float w = 1.0 / (0.001 + abs(z - centerZ) / max(centerZ, 1.0) * 50.0);
            sum += tex2Dlod(aoTex, float4(suv, 0, 0)).r * w;
            weights += w;
        }
    }
    float ao = weights > 0.0 ? sum / weights : 1.0;
    ao = ao >= 0.0 && ao <= 1.0 ? ao : 1.0;
    return float4(ao, ao, ao, 1.0);
}
