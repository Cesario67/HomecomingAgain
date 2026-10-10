#pragma once

#include <d3d9.h>

namespace hca::dlss {

// Accès aux images Vulkan du jeu quand il tourne sous DXVK (d3d9.dll de DXVK placé dans Bin), par l'interface
// d'interopérabilité officielle de DXVK (third_party/dxvk). Base du partage des images avec le processus DLSS 64 bits
// (docs/plan-dlss.md). Sans DXVK, ne fait rien.
//
// Jalon 1 (actuel) : récupérer l'interface et les images de la scène et de la profondeur, et les journaliser.

// Après CreateDevice. Renvoie true si le périphérique est un périphérique DXVK.
bool OnDeviceCreated(IDirect3DDevice9* device) noexcept;
bool Available() noexcept;

// Une fois par session, dès que les textures de la scène et de la profondeur du moteur sont connues.
void Probe(IDirect3DBaseTexture9* sceneColor, IDirect3DBaseTexture9* depth) noexcept;

}  // namespace hca::dlss
