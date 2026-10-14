# Camera Fix (Emertels)

Origine : https://github.com/Emertels/Silent-Hill-5-Camera-Fix, version 1.0 (12/10/2026), licence MIT
(`LICENSE.txt`, copyright 2026 Emerson Teles).

Ce qui en est repris dans HomecomingAgain (`src/patches/CameraFix.cpp`, `src/patches/CameraTuning.cpp`) :

- les sites modifiés dans le moteur et le rôle des registres et champs qu'il y lit ;
- les calculs et constantes de calibrage (vitesse horizontale, gain vertical, visée) ;
- la logique de son code ajouté au moteur, réécrite en C++ et en assembleur en ligne.

Rien n'est copié de ses binaires : le Camera Fix distribue un `g_SilentHill.sgl` modifié, construit sur une
installation Unknown Project 3.20, que HomecomingAgain ne télécharge ni n'utilise. Les modifications sont faites en
mémoire, après vérification des instructions d'origine du moteur Steam (build 6.30), désassemblé pour l'occasion.
