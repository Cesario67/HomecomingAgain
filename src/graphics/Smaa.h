#pragma once

#include <d3d9.h>

namespace shhf::graphics {

// Anticrénelage SMAA (Jimenez et al., préréglage HIGH, détection des contours par luminance) appliqué à l'image
// finale juste avant Present. Shaders précompilés (shaders/smaa_d3d9.hlsl) : aucune dépendance à D3DX.
//
// Limite connue : appliqué après le dessin de l'interface, il lisse aussi un peu les textes. Le placer avant
// l'interface demandera d'identifier la passe d'interface du moteur.
//
// Toutes les ressources sont en D3DPOOL_DEFAULT : OnBeforeReset doit être appelé avant chaque Reset du
// périphérique (sinon Reset échoue). Elles sont recréées à la première image suivante.
class Smaa {
public:
    Smaa() = default;
    ~Smaa();
    Smaa(const Smaa&) = delete;
    Smaa& operator=(const Smaa&) = delete;

    // Applique l'effet au tampon arrière courant. Renvoie false (sans rien modifier) en cas d'échec.
    bool Apply(IDirect3DDevice9* device) noexcept;

    void OnBeforeReset() noexcept;

private:
    bool EnsureResources(IDirect3DDevice9* device, UINT width, UINT height, D3DFORMAT format) noexcept;
    void ReleaseSizedResources() noexcept;
    void ReleaseAll() noexcept;

    IDirect3DDevice9* device_ = nullptr;  // non possédé : périphérique pour lequel les ressources existent
    UINT width_ = 0;
    UINT height_ = 0;
    D3DFORMAT format_ = D3DFMT_UNKNOWN;
    bool failed_ = false;  // échec de création : ne pas réessayer à chaque image

    // Indépendants de la taille.
    IDirect3DVertexShader9* edgeVs_ = nullptr;
    IDirect3DVertexShader9* weightVs_ = nullptr;
    IDirect3DVertexShader9* blendVs_ = nullptr;
    IDirect3DPixelShader9* edgePs_ = nullptr;
    IDirect3DPixelShader9* weightPs_ = nullptr;
    IDirect3DPixelShader9* blendPs_ = nullptr;
    IDirect3DVertexDeclaration9* vertexDeclaration_ = nullptr;
    IDirect3DTexture9* areaTex_ = nullptr;
    IDirect3DTexture9* searchTex_ = nullptr;
    IDirect3DStateBlock9* savedState_ = nullptr;

    // Dépendants de la taille du tampon arrière.
    IDirect3DTexture9* colorTex_ = nullptr;
    IDirect3DSurface9* colorSurface_ = nullptr;
    IDirect3DTexture9* edgeTex_ = nullptr;
    IDirect3DSurface9* edgeSurface_ = nullptr;
    IDirect3DTexture9* blendTex_ = nullptr;
    IDirect3DSurface9* blendSurface_ = nullptr;
};

}  // namespace shhf::graphics
