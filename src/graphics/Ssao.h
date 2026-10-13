#pragma once

#include <d3d9.h>

#include "graphics/Projection.h"

namespace hca::graphics {

// Occlusion ambiante en espace écran (shaders/ssao_d3d9.hlsl), appliquée à l'image de la scène avant les
// post-traitements du moteur (flou lumineux) et avant l'interface.
//
// Constaté par relevé d'images (10/10/2026, Steam build 6.30) :
// - avant la scène, le moteur écrit z/w (projection perspective) dans une texture R32F de la taille de l'écran,
//   cible de rendu, effacée à 1 (shader de pré-passe : mad oC0, z/w, mCommonDepthCoefficients.x, .y, avec x = 1,
//   y = 0) ;
// - la scène est dessinée dans une texture A8R8G8B8 de la taille de l'écran ;
// - les post-traitements commencent par une cible A16B16G16R16F de la taille de l'écran : c'est là qu'on insère
//   l'effet, sur la cible encore active (la scène) ;
// - la projection de la caméra est envoyée en c222..c225 (variable `mProj`) par certains vertex shaders.
//
// Ressources en D3DPOOL_DEFAULT : OnBeforeReset doit être appelé avant chaque Reset.
class Ssao {
public:
    Ssao() = default;
    ~Ssao();
    Ssao(const Ssao&) = delete;
    Ssao& operator=(const Ssao&) = delete;

    void OnTextureCreated(IDirect3DTexture9* texture, UINT width, UINT height, DWORD usage, D3DFORMAT format) noexcept;
    // Appelé avant que le jeu change de cible : applique l'effet au bon moment.
    void OnSetRenderTarget(IDirect3DDevice9* device, DWORD index, IDirect3DSurface9* surface) noexcept;
    void OnVertexShaderConstants(UINT startRegister, const float* values, UINT vectorCount) noexcept;
    void OnPresent() noexcept;
    void OnBeforeReset() noexcept;

    // Textures du moteur repérées (non possédées), pour l'interopérabilité DXVK (dlss/DxvkInterop.h). La texture de
    // la scène n'est connue qu'après une première application de l'effet.
    IDirect3DTexture9* DepthTexture() const noexcept { return depthTexture_; }
    IDirect3DSurface9* DepthSurface() const noexcept { return depthSurface_; }
    IDirect3DTexture9* SceneTexture() const noexcept { return sceneTexture_; }
    // Vrai une fois, juste après le dernier OnSetRenderTarget si la scène 3D vient de se terminer (avant les
    // post-traitements et l'interface).
    bool ConsumeSceneReady() noexcept {
        const bool ready = sceneReady_;
        sceneReady_ = false;
        return ready;
    }

private:
    bool Apply(IDirect3DDevice9* device) noexcept;
    bool EnsureResources(IDirect3DDevice9* device, UINT width, UINT height) noexcept;
    void ReleaseAll() noexcept;
    void LogDiagnostics(IDirect3DDevice9* device) noexcept;

    // Texture de profondeur du moteur et sa surface (non possédées : le moteur les crée et les libère). Elles ne sont
    // utilisées que pendant une image où le moteur vient de s'en servir, donc vivantes.
    IDirect3DTexture9* depthTexture_ = nullptr;
    IDirect3DSurface9* depthSurface_ = nullptr;
    IDirect3DTexture9* sceneTexture_ = nullptr;
    bool depthWritten_ = false;
    bool applied_ = false;
    bool sceneReady_ = false;
    Projection projection_;
    bool projectionSeen_ = false;
    bool failed_ = false;
    bool loggedFirstApply_ = false;
    int diagnosticsLeft_ = 5;
    unsigned applyCount_ = 0;

    IDirect3DDevice9* device_ = nullptr;  // non possédé
    UINT width_ = 0;
    UINT height_ = 0;
    IDirect3DVertexShader9* quadVs_ = nullptr;
    IDirect3DPixelShader9* occlusionPs_ = nullptr;
    IDirect3DPixelShader9* blurPs_ = nullptr;
    IDirect3DVertexDeclaration9* vertexDeclaration_ = nullptr;
    IDirect3DStateBlock9* savedState_ = nullptr;
    IDirect3DTexture9* aoTex_ = nullptr;
    IDirect3DSurface9* aoSurface_ = nullptr;
};

Ssao& GetSsao() noexcept;

}  // namespace hca::graphics
