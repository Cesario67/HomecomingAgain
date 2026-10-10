#pragma once

namespace hca::graphics {

// Vrai pendant nos propres dessins (SMAA, SSAO) : nos hooks du périphérique (filtrage anisotrope, ombres, relevé)
// ne doivent pas toucher à nos appels, seulement à ceux du jeu. Par thread, car le moteur appelle Direct3D depuis
// deux threads pendant les chargements.
inline thread_local bool t_internalDraw = false;

// Équivalent C# : un bloc using qui remet le drapeau à false en sortie, même en cas de retour anticipé.
class InternalDrawScope {
public:
    InternalDrawScope() noexcept { t_internalDraw = true; }
    ~InternalDrawScope() { t_internalDraw = false; }
    InternalDrawScope(const InternalDrawScope&) = delete;
    InternalDrawScope& operator=(const InternalDrawScope&) = delete;
};

}  // namespace hca::graphics
