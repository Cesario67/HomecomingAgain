#pragma once

#include <string>

namespace shhf {

// Reflet de SHHFix.ini. Les valeurs par défaut s'appliquent si le fichier ou une clé manque.
struct Config {
    // [Crash]
    bool crashDumps = true;
    bool fullMemoryDumps = false;   // dump complet (jusqu'à 2 Go) au lieu d'un dump réduit
    int dumpKeepCount = 10;

    // [Window]
    // Mode fenêtré du jeu (FullScreen=false dans Engine, fichier vars_pc.cfg) sans bordure ni barre de titre, étendu à
    // tout le moniteur : plein écran « sans bordure », sans périphérique perdu à l'Alt-Tab.
    bool borderless = true;

    // [Frames]
    bool frameStats = true;         // mesures de cadence, de Sleep et de mémoire dans le journal
    int reportIntervalSeconds = 10;
    int timerResolutionMs = 1;      // 0 = ne pas modifier la résolution du minuteur Windows
    // Limiteur d'images (0 = aucun). 0 tant que le verrou de cadence du jeu n'est pas analysé : des témoignages
    // parlent d'un 30 i/s codé en dur, et le correctif communautaire de 2018 débloque le 60 i/s (voir
    // docs/recherche-web.md). La valeur par défaut sera fixée après mesure en jeu.
    int frameLimitFps = 0;

    // [Debug]
    int logKeepCount = 10;
};

// Retourne false si le fichier .ini est absent (les valeurs par défaut sont alors utilisées).
bool LoadConfig(const std::wstring& iniPath);

const Config& GetConfig() noexcept;

}  // namespace shhf
