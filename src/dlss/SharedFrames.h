#pragma once

#include <d3d9.h>

#include <cstdint>

namespace hca::dlss {

// Partage des images avec le processus DLSS 64 bits (docs/plan-dlss.md). À la fin de la scène 3D, avant les
// post-traitements et l'interface, la scène et la profondeur du jeu (images Vulkan de DXVK) sont copiées sur la carte
// graphique dans un anneau de kSharedSlots paires d'images créées par le mod et exportées (handles Windows NT).
//
// Deux compteurs (sémaphores « timeline ») exportés eux aussi :
// - « produites » : le mod le passe à v quand l'image v est copiée, dans l'emplacement SlotForFrame(v) ;
// - « consommées » : le processus DLSS le passe à v quand il a fini avec l'image v.
// Le mod n'écrit dans un emplacement que si l'image qui l'occupait a été consommée, sans jamais attendre : sinon
// l'image est sautée. Un processus DLSS lent ou planté ne peut donc pas geler le jeu.
//
// Actif seulement sous DXVK et avec `[DLSS] ShareFrames=1` (expérimental).

constexpr std::uint32_t kSharedSlots = 3;

// Fonctions pures (testées par tests/unit).
constexpr std::uint32_t SlotForFrame(std::uint64_t frame, std::uint32_t slots) noexcept {
    return slots == 0 ? 0 : static_cast<std::uint32_t>(frame % slots);
}
// L'emplacement de l'image `value` est libre si l'image qui l'occupait (value - slots) a été consommée.
constexpr bool SlotIsFree(std::uint64_t value, std::uint64_t consumed, std::uint32_t slots) noexcept {
    return value <= slots || consumed + slots >= value;
}

// À la fin de la scène 3D, avec les textures de la scène et de la profondeur du moteur (non possédées).
void ShareFrame(IDirect3DBaseTexture9* sceneColor, IDirect3DBaseTexture9* depth) noexcept;

}  // namespace hca::dlss
