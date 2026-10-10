#pragma once

#include <d3d9.h>

namespace hca::graphics::on12 {

// Exécution du jeu par D3D9On12 (couche de Windows qui traduit Direct3D 9 en Direct3D 12), `[Graphics] D3D9On12`.
//
// Pourquoi (docs/plan-dlss.md) : le DLSS a besoin des images du jeu dans une API récente, et le jeu étant 32 bits,
// dans un autre processus (DLSS n'existe qu'en 64 bits). D3D9Ex, prévu au départ, refuse D3DPOOL_MANAGED que le
// moteur utilise ; D3D9On12 l'accepte, et ses ressources sont des ressources Direct3D 12, partageables entre
// processus avec des fences. Présent en 32 bits dans Windows 10 et 11 (d3d9.dll exporte Direct3DCreate9On12).
//
// Renvoie nullptr si la couche est indisponible : l'appelant crée alors l'objet Direct3D 9 habituel.
IDirect3D9* Create(UINT sdkVersion) noexcept;

// Après CreateDevice : journalise si le périphérique passe bien par D3D9On12.
void LogDevice(IDirect3DDevice9* device) noexcept;

}  // namespace hca::graphics::on12
