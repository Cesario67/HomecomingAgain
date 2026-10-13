#pragma once

#include <d3d9.h>

#include <cstdint>

namespace hca::dlss {

// Jalon 2 du DLSS (docs/plan-dlss.md) : à chaque image, la scène et la profondeur du jeu (images Vulkan de DXVK) sont
// copiées sur la carte graphique dans deux images créées par le mod et exportables vers un autre processus (mémoire
// Vulkan exportée en handle Windows NT), puis un sémaphore « timeline » exporté lui aussi est incrémenté. Le futur
// processus DLSS 64 bits ouvrira ces handles (Direct3D 12 ou Vulkan) et attendra le compteur.
//
// Actif seulement sous DXVK et avec `[DLSS] ShareFrames=1` (expérimental). Ne bloque jamais le jeu : si la copie
// précédente occupe encore l'emplacement, l'image est sautée.

// Fonction pure (testée par tests/unit) : emplacement à utiliser pour l'image `frame` parmi `slots`.
constexpr std::uint32_t SlotForFrame(std::uint64_t frame, std::uint32_t slots) noexcept {
    return slots == 0 ? 0 : static_cast<std::uint32_t>(frame % slots);
}

// À chaque Present, avec les textures de la scène et de la profondeur du moteur (non possédées).
void ShareFrame(IDirect3DBaseTexture9* sceneColor, IDirect3DBaseTexture9* depth) noexcept;

}  // namespace hca::dlss
