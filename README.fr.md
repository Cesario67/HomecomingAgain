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
- **Ombres plus fines** : carte d'ombres en 8192x8192 au lieu de 2048x2048, avec un décalage de pente adapté.
- **Grain de pellicule réglable** : désactivé, réduit de moitié ou comme à l'origine.
- **Carte entière au-delà du 720p** : le jeu limitait l'écran de la carte à une zone fixe de 1280x720 ; le mod l'étend
  à toute l'image (1080p, 1440p, 4K...) en gardant la carte à sa taille et à sa place d'origine.
- **DLSS (DLAA, expérimental)** : anticrénelage par l'IA de NVIDIA, plus stable et plus net que le SMAA, à la
  résolution d'origine. Cartes NVIDIA RTX uniquement ; l'option du launcher installe aussi DXVK (Direct3D 9 sur
  Vulkan). Le jeu étant 32 bits et le DLSS 64 bits, un petit programme 64 bits (`HomecomingAgain.Dlss.exe`) reçoit les
  images du jeu, applique le DLSS et les lui rend avant l'interface. Désactivé par défaut.
- **Shaders du jeu en haute qualité** : le SSAO et le DLSS fonctionnent aussi dans ce mode (scène en 16 bits).

### Manette

- **Icônes de boutons Xbox ou PlayStation** : les menus, l'inventaire, l'écran des commandes et les invites en jeu
  (Ouvrir, Sauvegarder...) affichent A, B, X, Y (ou croix, rond, carré, triangle) au lieu des chiffres 1 à 4 de la
  version PC. Les icônes viennent des données du jeu lui-même. Par défaut, icônes Xbox quand une manette est
  branchée, icônes PC sinon.
- **Vrais noms des boutons** sur l'écran des commandes (LB, LT, Croix haut...) au lieu de « Button 6 », et icône
  du stick droit à côté de « Zoom » sur la carte.
- **Plus de pauses intempestives** : A et B sont laissés au jeu, qui gère lui-même la manette dans ses menus.

### Confort

- **Passer les cinématiques** : la touche Suppr passe les cinématiques en vidéo.
- **Logos de démarrage accélérés** : l'écran titre arrive au bout de 5 s environ au lieu de 23. Les logos (Konami,
  Double Helix, mentions légales) défilent en accéléré ; le jeu a déjà fini de charger pendant qu'ils s'affichent.
- **Launcher** (`HomecomingAgain.exe`) : tous les réglages, l'installation et la désinstallation, en anglais (par
  défaut) et en français.

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
| `[Graphics]` | `ShadowResolution=8192` | Taille de la carte d'ombres (2048 d'origine, 4096 ou 8192) |
| `[Graphics]` | `FilmGrain=100` | Grain de pellicule en % (0 = aucun) |
| `[Graphics]` | `FullScreenUiClip=1` | Carte entière au-delà du 720p (0 = d'origine) |
| `[Audio]` | `ChannelFix=1` | Son corrigé (0 = son d'origine) |
| `[Movies]` | `SkipKey=46` | Touche pour passer les cinématiques (46 = Suppr, 0 = aucune) |
| `[Movies]` | `SkipLogos=1` | Logos de démarrage accélérés (0 = d'origine) |
| `[Input]` | `ButtonIcons=0` | Icônes de boutons : 0 = auto (Xbox si une manette est branchée), 1 = PC (d'origine), 2 = Xbox, 3 = PlayStation |
| `[Input]` | `MenuPad=0` | Traduire la manette en touches du clavier dans les menus (croix, stick gauche, Start) ; désactivé par défaut, le jeu gère la manette lui-même |
| `[Camera]` | `CameraFix=0` | Correctif de caméra à la manette (d'après le Camera Fix d'Emertels) : le personnage tourne avec le stick droit, réponse verticale proportionnelle, sensibilité de visée séparée. Vitesses `HorizontalSpeed`, `VerticalSpeed`, `AimHorizontalSpeed`, `AimVerticalSpeed` (0,25 à 3,0). Le launcher libère aussi le stick droit dans `Engine\binds_pc_mjs.cfg` |
| `[Camera]` | `Mode=Follow` | Avec `CameraFix=1` : `Follow` = le personnage tourne et la caméra le suit ; `Orbit` (expérimental) = le stick droit tourne la caméra autour du personnage, qui ne bouge pas |
| `[DLSS]` | `ShareFrames`, `Jitter`, `Apply` | DLSS (DLAA) : les trois à 1 pour l'activer, avec DXVK (fait par le launcher) |

## Limites connues

- Le plein écran exclusif plante toujours à l'Alt-Tab (jeu d'origine) : utiliser le mode sans bordure.
- Manettes : seules les manettes XInput (Xbox et compatibles) sont lues par le jeu. Zones mortes, sensibilité de la
  caméra et vibrations restent celles d'origine.
- DLSS : expérimental. Seul le mouvement de la caméra est transmis : de légères traînées sont possibles sur les
  personnages en mouvement rapide.

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
- Correctif de caméra à la manette porté du Camera Fix d'Emerson Teles (Emertels), licence MIT, Copyright (c) 2026
  Emerson Teles. Voir `third_party/camerafix/LICENSE.txt` et `third_party/camerafix/ORIGINE.md`.
- Interfaces d'interopérabilité de DXVK (extrait de `d3d9_interfaces.h`), licence zlib. Copyright (c) 2017 Philip
  Rebohle, (c) 2019 Joshua Ashton, (c) 2019 Robin Kertels. Voir `third_party/dxvk/LICENSE.txt`.
- SDK DLSS de NVIDIA (`nvngx_dlss.dll` et la bibliothèque NGX liée à `HomecomingAgain.Dlss.exe`), licence NVIDIA RTX
  SDKs, voir `LICENSE-NVIDIA-DLSS.txt` dans le paquet. Absent du dépôt : téléchargé à la compilation. NVIDIA et DLSS
  sont des marques de NVIDIA Corporation ; ce projet n'est ni soutenu ni approuvé par NVIDIA.
