# Homecoming Again

Mod de correction et d'amélioration pour *Silent Hill: Homecoming* sur PC (Steam), pour le faire fonctionner
correctement sur les machines actuelles.

**État : en développement, pas encore publié.** Les corrections ci-dessous sont testées en jeu sur une machine
(Windows 11, RTX 5070 Ti) avec la version Steam du jeu. Recherches : [docs/recherche-web.md](docs/recherche-web.md).
Travail en cours : [docs/a-faire.md](docs/a-faire.md).

## Ce qui est corrigé

### Stabilité et cadence

- **60 images par seconde** : le jeu est bloqué à 30 i/s par une valeur codée en dur dans son moteur (le réglage
  `fpsLimit` du jeu n'y change rien). Le mod remplace cette limite par la cadence choisie, sans modifier les fichiers
  du jeu.
- **Plein écran sans bordure** : le jeu tourne en fenêtre étendue à tout l'écran. Plus de plantage à l'Alt-Tab, qui
  touche le plein écran exclusif d'origine.
- **Blocages au chargement avec l'overlay Steam** : le jeu pouvait se figer pendant les chargements en fenêtré.
  Corrigé (traitement des messages de la fenêtre pendant les attentes, Direct3D protégé contre les appels simultanés).
- **Patch 4 Go** : le jeu peut utiliser 4 Go de mémoire au lieu de 2 (appliqué par le launcher, copie de l'original
  conservée et remise à la désinstallation).
- **Diagnostic** : en cas de plantage ou de blocage, un rapport et un fichier de diagnostic (minidump) sont écrits
  dans `Bin\HomecomingAgain`.

### Son

- **Sons dans une seule oreille** : sur un casque ou des enceintes stéréo, la plupart des sons (bruitages, voix,
  musique) sortaient d'un seul côté. C'est un défaut connu du portage PC. Le jeu mixe tout dans une seule sortie 5.1
  dont le placement des sons est faussé, et sa version de XAudio2 la convertit mal en stéréo. Le mod fait lui-même
  le mélange vers la stéréo correctement : chaque son retrouve sa place à gauche ou à droite, sans remplacer les
  fichiers audio ni installer de pilote.

### Graphismes

- **SMAA** : anticrénelage qui lisse les contours.
- **Filtrage anisotrope x16** : textures nettes au sol et vues de biais.
- **Occlusion ambiante (SSAO)** : léger assombrissement des coins et du contact entre les objets, calculé à partir
  de la profondeur du moteur et appliqué avant l'interface.
- **Ombres plus fines** : carte d'ombres en 4096x4096 au lieu de 2048x2048.
- **Grain de pellicule réglable** : désactivé, réduit de moitié ou comme à l'origine.

### Confort

- **Passer les cinématiques** : la touche Suppr passe les cinématiques en vidéo.
- **Launcher** (`HomecomingAgain.exe`) : tous les réglages, l'installation et la désinstallation, en français et en
  anglais.

## Installation

Lancer `HomecomingAgain.exe`, choisir le dossier du jeu, puis **Installer**. Le launcher copie le mod dans `Bin`,
applique le patch 4 Go et garde une copie des fichiers modifiés. **Désinstaller** remet tout comme avant.

## Réglages

Dans le launcher, ou dans `Bin\HomecomingAgain.ini` :

| Section | Réglage | Effet |
|---|---|---|
| `[Frames]` | `TargetFps=60` | Cadence du jeu (0 = 30 i/s d'origine) |
| `[Window]` | `Borderless=1` | Plein écran sans bordure |
| `[Graphics]` | `SMAA`, `Anisotropy` | Anticrénelage, filtrage des textures |
| `[Graphics]` | `SSAO`, `SsaoStrength` | Occlusion ambiante et son intensité |
| `[Graphics]` | `ShadowResolution=4096` | Taille de la carte d'ombres (2048 d'origine) |
| `[Graphics]` | `FilmGrain=100` | Grain de pellicule en % (0 = aucun) |
| `[Audio]` | `ChannelFix=1` | Son corrigé (0 = son d'origine) |
| `[Movies]` | `SkipKey=46` | Touche pour passer les cinématiques (46 = Suppr, 0 = aucune) |

## Limites connues

- Le plein écran exclusif plante toujours à l'Alt-Tab (jeu d'origine) : utiliser le mode sans bordure.
- Manettes : pas encore traitées.
- DLSS : en préparation (expérimental, inactif).

## Compilation

Build Tools for Visual Studio 2022 (charge C++), cible x86, et le SDK .NET 10 pour le launcher :

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

## Licence

GPL-3.0-or-later. Aucun fichier du jeu n'est inclus ni distribué : le dépôt ne contient que son propre code.

Permission supplémentaire au titre de l'article 7 de la GNU GPL version 3 (le texte anglais du README fait foi) : il
est permis de lier ce programme au SDK DLSS de NVIDIA et de distribuer le résultat. Les parties du SDK restent sous
la licence NVIDIA RTX SDKs et ne sont pas couvertes par la GPL ; leur code source n'a pas à être fourni.

## Code tiers

- Uses SMAA. Copyright (C) 2011 by Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro and Diego
  Gutierrez. Voir `third_party/smaa/LICENSE.txt`.
- Interfaces d'interopérabilité de DXVK (extrait de `d3d9_interfaces.h`), licence zlib. Copyright (c) 2017 Philip
  Rebohle, (c) 2019 Joshua Ashton, (c) 2019 Robin Kertels. Voir `third_party/dxvk/LICENSE.txt`.
- SDK DLSS de NVIDIA (`nvngx_dlss.dll` et la bibliothèque NGX liée à `HomecomingAgain.Dlss.exe`), licence NVIDIA RTX
  SDKs, voir `LICENSE-NVIDIA-DLSS.txt` dans le paquet. Absent du dépôt : téléchargé à la compilation. NVIDIA et DLSS
  sont des marques de NVIDIA Corporation ; ce projet n'est ni soutenu ni approuvé par NVIDIA.
