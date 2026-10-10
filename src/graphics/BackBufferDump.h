#pragma once

#include <d3d9.h>

namespace hca::graphics::dump {

// Diagnostic `[Debug] BackBufferDump` : enregistre l'image sur le point d'être affichée (tampon arrière, après nos
// effets) dans HomecomingAgain\captures\backbuffer_<horodatage>.bmp, toutes les 15 secondes, 8 fois au plus.
// Sert quand l'écran reste noir : distingue une image mal dessinée d'une image mal présentée (D3D9On12, fenêtre).
// Copie vers la mémoire système : lent, réservé au diagnostic.
void OnPresent(IDirect3DDevice9* device) noexcept;

}  // namespace hca::graphics::dump
