#pragma once

#include <windows.h>

#include <cstdint>
#include <initializer_list>

namespace hca::patches {

// Triches ([Cheats] God et UnlimitedAmmo), à partir des variables de débogage du moteur. Analyse complète :
// docs/analyse-shh-exe.md, section « Triches ». g_SilentHill.sgl Steam build 6.30 (horodatage PE 0x490F50BF),
// vérifié par désassemblage le 18/10/2026 :
// - mode dieu : octet `god` en [[[0x111D2460]+0x38]+8]+0xF5, lu par IsGodMode (0x10143590, octets vérifiés au
//   démarrage). Le jeu le met lui-même à 1 puis le restaure pendant certains scripts, et le remet à 0 en 0x1012497D
//   (événement non identifié) : le mod le remet donc à 1 à chaque image tant que l'option est active, et n'y touche
//   pas sinon. Le code des armes saute aussi la consommation de munitions quand IsGodMode est vrai (0x102A5A5D) ;
// - munitions illimitées : octet +0xA11 du personnage renvoyé par 0x10143420 (fonction sans argument, octets vérifiés
//   au démarrage), celui qu'écrit la variable `unlimitedammo` (0x10319600, octets vérifiés au démarrage), lu par le
//   code des armes (0x102A523D, 0x102A5A5D...).
// Vérifié en jeu le 18/10/2026 : mode dieu et munitions illimitées fonctionnent. Hypothèse restante : que 0x10143420
// puisse toujours être appelée sans risque depuis Present (le moteur rendrait depuis son thread principal).

namespace cheats {

// Suit une chaîne de pointeurs : value = read(root), puis value = read(value + offset) pour chaque offset. Renvoie 0
// dès qu'une lecture échoue ou donne un pointeur nul. `read(address, value)` renvoie false si la lecture échoue.
// Fonction pure (testée par tests/unit) : la lecture est fournie par l'appelant.
template <typename ReadFn>
std::uintptr_t FollowPointers(std::uintptr_t root, std::initializer_list<std::uint32_t> offsets, ReadFn read) {
    std::uintptr_t value = 0;
    if (!read(root, value) || value == 0) {
        return 0;
    }
    for (const std::uint32_t offset : offsets) {
        if (!read(value + offset, value) || value == 0) {
            return 0;
        }
    }
    return value;
}

}  // namespace cheats

// Vérifie les octets des fonctions du moteur et active les triches demandées par le .ini.
void InstallCheats(HMODULE engineModule);

// Appelée à chaque image (hook de Present) : remet les octets à 1 si besoin. Sans allocation ni E/S.
void ApplyCheatsEachFrame() noexcept;

}  // namespace hca::patches
