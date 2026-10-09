# À faire

Points en attente, avec ce que l'on sait déjà. À tenir à jour : retirer un point une fois traité.

## Bloqué tant que le jeu n'est pas installé

Installation Steam en cours (09/10/2026) dans `E:\SteamLibrary\steamapps\common\Silent Hill Homecoming`.

1. **Analyse de `SilentHill.exe`** (lecture seule, désassemblage dans le scratchpad, résultats dans
   `docs/analyse-shh-exe.md`) : bitness, horodatage PE, SteamStub, imports (voir `docs/recherche-web.md`).
2. **Choix du chargeur** d'après les imports : DLL proxy (`d3d9.dll`, `dinput8.dll`, `xinput1_3.dll`...) ou autre.
   Outil à écrire : générateur de redirections (exports de la vraie DLL de `SysWOW64`).
3. Renseigner `kAnalyzedTimeDateStamp` dans `src/core/SystemInfo.cpp` et le nom exact de l'exécutable dans
   `src/dllmain.cpp`.

## Objectifs (demande du 09/10/2026)

1. **Plantages et cadence**, compatibilité avec les machines actuelles : cause des plantages (Windows 10/11,
   résolution, fréquence), verrou 30 i/s, saccades, drapeau 4 Go.
2. **Graphismes** : résolution et ratio (16:9, 21:9), champ de vision, anticrénelage, carte et interface.
   Voir ce qui est faisable avant de promettre.
3. **Manettes Xbox 360 / Xbox** : lecture XInput correcte, correspondance des boutons, sticks et gâchettes
   (réutiliser l'approche de `ControllerMapping` du projet Deadly Premonition).

## Fait

- Dépôt initialisé (09/10/2026) : modules `core`, `crash`, `framepacing` repris de DPStabilityFix et adaptés,
  `graphics/D3D9Hooks` simplifié (journalisation des paramètres, mesure de cadence).
- **Rien n'a encore été compilé ni lancé** : attente de la confirmation de l'utilisateur pour le premier build.
