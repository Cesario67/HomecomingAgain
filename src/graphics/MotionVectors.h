#pragma once

#include <d3d9.h>

namespace hca::graphics {

// Vecteurs de mouvement de la caméra pour le DLSS (shaders/motion_d3d9.hlsl), calculés à la fin de la scène 3D à partir
// de la profondeur du moteur et des caméras de l'image courante et de la précédente (graphics/CameraTracker.h). Texture
// G16R16F de la taille de l'écran, en pixels (position précédente - position actuelle). Les objets qui bougent
// eux-mêmes (personnages, monstres) n'y sont pas : seulement le mouvement de la caméra.
//
// Ressources en D3DPOOL_DEFAULT : OnBeforeReset doit être appelé avant chaque Reset.
class MotionVectors {
public:
    MotionVectors() = default;
    ~MotionVectors();
    MotionVectors(const MotionVectors&) = delete;
    MotionVectors& operator=(const MotionVectors&) = delete;

    // Dessine les vecteurs de l'image ; renvoie la texture (possédée par cet objet) ou nullptr.
    IDirect3DTexture9* Render(IDirect3DDevice9* device, IDirect3DTexture9* depth) noexcept;
    void OnBeforeReset() noexcept;

private:
    bool EnsureResources(IDirect3DDevice9* device, UINT width, UINT height) noexcept;
    void ReleaseAll() noexcept;

    IDirect3DDevice9* device_ = nullptr;  // non possédé
    UINT width_ = 0;
    UINT height_ = 0;
    bool failed_ = false;
    bool logged_ = false;
    IDirect3DVertexShader9* quadVs_ = nullptr;
    IDirect3DPixelShader9* motionPs_ = nullptr;
    IDirect3DVertexDeclaration9* vertexDeclaration_ = nullptr;
    IDirect3DStateBlock9* savedState_ = nullptr;
    IDirect3DTexture9* target_ = nullptr;
    IDirect3DSurface9* targetSurface_ = nullptr;
};

MotionVectors& GetMotionVectors() noexcept;

}  // namespace hca::graphics
