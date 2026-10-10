#pragma once

#include <d3d9.h>

namespace hca::graphics::capture {

// Relevé d'images pour préparer le DLSS (docs/plan-dlss.md, étape 1). Actif seulement avec [Debug]
// FrameCapture=1. Une touche (F11 par défaut) enregistre les deux images suivantes dans
// HomecomingAgain\captures\<horodatage>\ : chaque changement de cible de rendu et de tampon de profondeur,
// effacement, viewport, texture, shader, constantes de shader et dessin, dans l'ordre. Les shaders rencontrés
// sont enregistrés (bytecode) avec leur table de constantes quand elle existe. But : trouver le tampon de
// profondeur, les registres des matrices de caméra, la fin de la scène 3D et le début de l'interface.
//
// Hors relevé, les hooks ne font qu'un test de booléen avant d'appeler la fonction d'origine.

// Intercepte les méthodes du périphérique utiles au relevé (vtable). À appeler une fois, après CreateDevice.
void Install(IDirect3DDevice9* device);

// Appelé au début de Present, avant nos propres dessins : lit la touche, termine ou démarre un relevé.
void OnPresent(IDirect3DDevice9* device);

// Nos propres dessins (SMAA, SSAO) n'apparaissent pas dans le relevé : voir graphics/InternalDraw.h.

}  // namespace hca::graphics::capture
