#pragma once

#include <string>

namespace shhf {

// Reflet de SHHFix.ini. Les valeurs par défaut s'appliquent si le fichier ou une clé manque.
struct Config {
    // [Crash]
    bool crashDumps = true;
    bool fullMemoryDumps = false;   // dump complet (jusqu'à 2 Go) au lieu d'un dump réduit
    int dumpKeepCount = 10;
    // Secondes sans image avant de journaliser l'état des threads et d'écrire un minidump (0 = désactivé).
    int hangSeconds = 3;

    // [Window]
    // Mode fenêtré du jeu (FullScreen=false dans Engine, fichier vars_pc.cfg) sans bordure ni barre de titre, étendu à
    // tout le moniteur : plein écran « sans bordure », sans périphérique perdu à l'Alt-Tab.
    bool borderless = true;

    // [Graphics]
    // Anticrénelage SMAA appliqué à l'image finale (voir graphics/Smaa.h).
    bool smaa = true;
    // Filtrage anisotrope forcé sur les textures filtrées en linéaire (0 ou 1 = jeu d'origine, 2 à 16).
    int anisotropy = 16;

    // [Frames]
    bool frameStats = true;         // mesures de cadence, de Sleep et de mémoire dans le journal
    int reportIntervalSeconds = 10;
    int timerResolutionMs = 1;      // 0 = ne pas modifier la résolution du minuteur Windows
    // Limiteur d'images (0 = aucun). 0 tant que le verrou de cadence du jeu n'est pas analysé : des témoignages
    // parlent d'un 30 i/s codé en dur, et le correctif communautaire de 2018 débloque le 60 i/s (voir
    // docs/recherche-web.md). La valeur par défaut sera fixée après mesure en jeu.
    int frameLimitFps = 0;
    // Cadence visée par le limiteur du moteur, qui est codé à 30 i/s en dur (voir patches/FrameRateCap.h).
    // 0 = jeu d'origine. fpsLimit de vars_pc.cfg n'a aucun effet sur ce limiteur (vérifié le 10/10/2026).
    int targetFps = 60;

    // [Compat]
    // Le thread de la fenêtre traite les messages envoyés pendant qu'il attend un verrou du moteur : évite
    // l'interblocage du chargement en fenêtré avec l'overlay Steam (voir compat/WindowThreadLock.h).
    bool windowThreadLock = true;
    // Périphérique Direct3D créé avec D3DCREATE_MULTITHREADED : le moteur l'utilise depuis deux threads pendant
    // les chargements sans ce drapeau (interblocage constaté dans le pilote NVIDIA, voir graphics/D3D9Hooks.cpp).
    bool d3dMultithreaded = true;

    // [Debug]
    int logKeepCount = 10;
};

// Retourne false si le fichier .ini est absent (les valeurs par défaut sont alors utilisées).
bool LoadConfig(const std::wstring& iniPath);

const Config& GetConfig() noexcept;

}  // namespace shhf
