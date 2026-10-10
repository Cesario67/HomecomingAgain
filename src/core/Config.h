#pragma once

#include <string>

namespace hca {

// Reflet de HomecomingAgain.ini. Les valeurs par défaut s'appliquent si le fichier ou une clé manque.
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
    // Taille de l'atlas d'ombres : 2048 (jeu d'origine), 4096 ou 8192 (voir graphics/ShadowResolution.h).
    int shadowResolution = 4096;
    // Occlusion ambiante en espace écran (voir graphics/Ssao.h). Rayon en unités du jeu, intensité en %, fondu
    // entre deux distances (unités du jeu), mode de réglage qui affiche l'occlusion seule.
    bool ssao = true;
    int ssaoRadius = 40;
    int ssaoStrength = 100;
    int ssaoFadeStart = 1500;
    int ssaoFadeEnd = 4000;
    bool ssaoDebug = false;

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
    // Relevé d'images pour préparer le DLSS (voir graphics/FrameCapture.h). Touche : code de touche virtuelle
    // Windows (0x7A = F11).
    bool frameCapture = false;
    int frameCaptureKey = 0x7A;
};

// Retourne false si le fichier .ini est absent (les valeurs par défaut sont alors utilisées).
bool LoadConfig(const std::wstring& iniPath);

const Config& GetConfig() noexcept;

}  // namespace hca
