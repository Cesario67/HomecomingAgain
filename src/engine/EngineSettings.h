#pragma once

namespace hca::engine {

// Lit les réglages de cadence du moteur en mémoire (valeurs réellement utilisées, après chargement de
// vars_pc.cfg). Renvoie false si les structures ne sont pas encore créées ou si la version du moteur diffère.
//
// g_SilentHill.sgl, horodatage PE 0x490F50BF (Steam, build 6.30), vérifié par désassemblage le 10/10/2026 :
// - 0x111D2460 : pointeur global vers la racine du moteur ; [racine+0x38] : table des sous-systèmes ;
// - [table+0x08] : objet des réglages « Engine » ; fpsLimit en +0xE4 (int), maxFPSLimit en +0xE8 (int) :
//   enregistrés en 0x10A27F6B et 0x10A27FA8 (chaînes « Engine FPS limit », « Max Engine FPS limit ») ;
// - 0x101266CF : à chaque image, fpsLimit est ramené à maxFPSLimit s'il vaut 0 ou le dépasse.
bool ReadFpsLimits(int& fpsLimit, int& maxFpsLimit) noexcept;

}  // namespace hca::engine
