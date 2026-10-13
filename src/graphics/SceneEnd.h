#pragma once

namespace hca::graphics {

// Repère la fin de la scène 3D (avant les post-traitements et l'interface) dans la suite des cibles de rendu d'une
// image, quel que soit le format de la scène. Relevés F11 (Steam build 6.30) :
// - 13/10/2026, shaders du jeu en basse qualité : pré-passe de profondeur (R32F), scène en A8R8G8B8, puis
//   post-traitements en A16B16G16R16F ;
// - 13/10/2026, shaders en haute qualité (vars_pc.cfg LowQualityShaders=false) : la scène elle-même est en
//   A16B16G16R16F, et les post-traitements dans une autre texture A16B16G16R16F plein écran.
// Règle : la cible de la scène est celle posée juste après la pré-passe ; la scène se termine au premier passage de
// cette cible vers une autre cible plein écran qui n'est pas la pré-passe. Les cibles plus petites (reflets, flou,
// cartes d'ombres) sont ignorées. Fonction pure sur l'identité des surfaces (testée par tests/unit).
class SceneEndDetector {
public:
    void OnPresent() noexcept {
        depthPassSeen_ = false;
        afterDepth_ = false;
        ended_ = false;
        current_ = nullptr;
    }

    // `surface` : nouvelle cible 0 ; `isDepthPass` : c'est la texture de la pré-passe ; `fullScreen` : même taille
    // qu'elle. Renvoie true une seule fois par image, au changement de cible qui termine la scène.
    bool OnSetRenderTarget(const void* surface, bool isDepthPass, bool fullScreen) noexcept {
        const void* previous = current_;
        current_ = surface;
        if (surface == nullptr) {
            return false;
        }
        if (isDepthPass) {
            depthPassSeen_ = true;
            afterDepth_ = true;
            return false;
        }
        if (afterDepth_) {
            afterDepth_ = false;
            scene_ = surface;
            return false;
        }
        if (ended_ || !depthPassSeen_ || previous != scene_ || surface == scene_ || !fullScreen) {
            return false;
        }
        ended_ = true;
        return true;
    }

    // Cible de la scène de l'image en cours (celle qui vient de se terminer quand OnSetRenderTarget renvoie true).
    const void* Scene() const noexcept { return scene_; }

private:
    const void* current_ = nullptr;
    const void* scene_ = nullptr;
    bool depthPassSeen_ = false;
    bool afterDepth_ = false;
    bool ended_ = false;
};

}  // namespace hca::graphics
