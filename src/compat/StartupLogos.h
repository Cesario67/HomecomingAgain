#pragma once

#include <windows.h>

namespace hca::compat {

// Logos de démarrage raccourcis (`[Movies] SkipLogos`). Constaté le 14/10/2026 (Steam build 6.30) : environ 20 s de
// logos avant l'écran titre (Konami, Double Helix, mentions légales), que les touches ne passent pas, alors que le
// jeu a fini de lire ses données au bout de 3 s (relevé des fichiers ouverts et des octets lus).
//
// Les logos sont dessinés par la boucle d'écran de chargement de g_SilentHill.sgl, en 0x104EB4D0 (vérifié par
// désassemblage et par le relevé des pauses : 30 pauses de 33 ms par seconde depuis 0x104EB528 pendant les logos,
// plus aucune à l'écran titre). Tant que l'octet 0x11596FCC vaut 1, elle fait avancer l'écran d'un pas fixe lu par
// `fld dword ptr ds:[0x10FF5C68]` en 0x104EB4F0 (0,0333 s), le dessine, puis attend 33 ms. L'animation avance donc
// d'un pas par image, quelle que soit l'horloge.
//
// Le mod redirige l'opérande de cette instruction vers sa propre variable, qui vaut 10 pas au démarrage : les logos
// défilent 10 fois plus vite. La même boucle sert aux écrans de chargement en jeu : dès qu'elle s'arrête pour la
// première fois (octet repassé à 0, à l'écran titre), la variable reprend le pas d'origine. Arrêt de sécurité au
// bout de 30 s. Octets vérifiés avant modification ; sinon rien n'est fait.
void InstallStartupLogos(HMODULE engineModule);

// Fonction pure (testée par tests/unit) : suivi de la première séance de la boucle d'écran de chargement. Renvoie
// vrai quand elle vient de se terminer (déjà vue active, inactive maintenant).
class FirstLoadingScreen {
public:
    bool Update(bool active) noexcept;

private:
    bool seenActive_ = false;
    bool finished_ = false;
};

}  // namespace hca::compat
