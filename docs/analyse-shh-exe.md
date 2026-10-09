# Analyse de SilentHill.exe et du moteur

Règles (voir CLAUDE.md) : désassembler dans le scratchpad, jamais dans le dépôt ; consigner adresses et
descriptions en distinguant **vérifié** (désassemblage, dump, test) de **hypothèse**. Aucune information tirée du
patch d'unknownproject ou de Steam006.

Méthode de cette première passe (09/10/2026) : `dumpbin /headers /imports /exports` et recherche de chaînes dans
les fichiers d'origine, en lecture seule. Version : Silent Hill PC "Build v6.30" (Code Changelist #640742),
fichier `Engine\versionPC.txt` ; README du jeu : v1.0, novembre 2008. Installation Steam, appid 19000.

## Architecture (vérifié)

Le jeu est en deux morceaux :

| Fichier | Rôle | Faits vérifiés |
|---|---|---|
| `Bin\SilentHill.exe` (33 280 o) | **Lanceur** | PE 32 bits, horodatage 26/02/2009, sous-système GUI, **sans LARGE_ADDRESS_AWARE** (caractéristiques 0x103), sans ASLR ni DEP, point d'entrée 0x4025D1, base 0x400000 |
| `Bin\g_SilentHill.sgl` (20 959 232 o) | **Moteur** : une DLL (nom d'extension `.sgl`) | PE 32 bits, horodatage 03/11/2008, base **0x10000000** avec table de relocations, taille d'image 0x191B000, `.text` 0xF79000 (16 Mo de code, non compressé), sans ASLR |
| `Bin\binkw32.dll` | Vidéo Bink | importée par le moteur |

Le moteur n'est ni compressé ni protégé : ses exports gardent leurs noms C++ décorés (25 exports, dont
`TGO::GetPlayer`, `TGO::GetTotalElapsedGameTime`, `TGO::Rand`, `TAIBrain`, `ICameraControl`, `BindScript`,
`Execute`), ce qui facilite beaucoup l'analyse. Pas de SteamStub constaté (code lisible).

Le lanceur importe, dans cet ordre : **VERSION.dll** (VerQueryValueA, GetFileVersionInfoA,
GetFileVersionInfoSizeA), KERNEL32, USER32, ADVAPI32, SHELL32, **g_SilentHill.sgl** (5 variables : `g_ReloadVidResH`,
`g_ReloadRefresh`, `g_ReloadLowQualityRendering`, `g_RelaunchGame`, `g_ReloadVidRes`) et MSVCR80. Il appelle
`Execute` du moteur (hypothèse : c'est le point d'entrée du jeu, non vérifié), crée un mutex et lit le registre.
Les variables `g_Reload*` suggèrent un redémarrage du moteur quand on change de résolution (hypothèse).

Imports du moteur (vérifié) : KERNEL32 (Sleep, QueryPerformanceCounter, GetTickCount, CreateThread,
SetThreadPriority, CreateFileA, SetUnhandledExceptionFilter), USER32 (CreateWindowExA, DestroyWindow, RegisterClassA,
ClipCursor, SetCursorPos), **d3d9.dll** (`Direct3DCreate9`), **d3dx9_38.dll** (textures, capture), **DINPUT8.dll**
(`DirectInput8Create`), **XINPUT1_3.dll par ordinaux 2, 3 et 4**, WINMM (`timeSetEvent`, `timeKillEvent`), WSOCK32
(réseau), binkw32 (18 fonctions), MSVCR80 (CRT Visual C++ 2005).

Ordinaux de `XINPUT1_3.dll` (vérifié sur la DLL du système) : 2 = `XInputGetState`, 3 = `XInputSetState`,
4 = `XInputGetCapabilities`. Le moteur lit donc les manettes via XInput et le clavier/souris via DirectInput 8.

## Conséquences pour le mod

1. **Chargeur** : proxy de `version.dll`, première DLL importée par le lanceur, absente de KnownDLLs (hypothèse à
   confirmer au premier lancement). Même principe que `X3DAudio1_7.dll` pour DP. Nos hooks visent l'IAT du
   **moteur** (`g_SilentHill.sgl`), pas celle du lanceur : `d3d9`, `XINPUT1_3` et `DINPUT8` n'y sont importés que
   par lui. Hypothèse à confirmer : à l'exécution de notre `DllMain`, le moteur est déjà mappé et ses imports
   résolus, ce qui permet de patcher son IAT.
2. **Manette** : le moteur sait déjà lire XInput, donc le problème n'est pas l'absence de support mais ce qu'il en
   fait (correspondance des boutons dans `binds_pc_mjs.cfg`, sensibilité de la caméra signalée trop forte). On
   intercepte `XInputGetState` (ordinal 2) pour filtrer zones mortes et sensibilité, sans toucher au moteur.
3. **4 Go** : le lanceur n'est pas LARGE_ADDRESS_AWARE, donc le processus est limité à 2 Go d'espace d'adressage.
   Seul un changement de l'en-tête du fichier ou une copie modifiée peut y remédier : jamais sans confirmation
   de l'utilisateur (voir CLAUDE.md), probablement via une copie `SilentHill_4GB.exe` ou un lanceur dédié.
4. **Dépendances d'exécution** : MSVCR80 (VC++ 2005), d3dx9_38 et xinput1_3 viennent du dossier `vcredist` et
   `DirectX` du jeu, installés par `installscript.vdf` de Steam. Sur cette machine, `d3dx9_38.dll` et
   `xinput1_3.dll` sont présents dans SysWOW64 et le runtime VC80 dans WinSxS. Une machine sans eux échouerait au
   démarrage : à signaler clairement (message d'erreur) plutôt que par un plantage muet.
5. **Cadence** : le moteur a des réglages `fpsLimit` et `maxFPSLimit` (chaînes du moteur), `VSync`, `vsyncInterval`,
   `antiAlias`, `ScreenResWidth`, `ScreenResHeight`, `preferredFullscreenRes` dans `vars_pc.cfg` (fichier créé au
   premier lancement). Le verrou 30 i/s est donc soit une borne sur ces valeurs, soit une constante : à trouver
   en désassemblant les références à ces chaînes.

## À analyser ensuite (par ordre de priorité)

1. Références aux chaînes `fpsLimit` / `maxFPSLimit` / `ScreenResWidth` : borne de cadence, clamp de résolution.
2. `Direct3DCreate9` : paramètres de présentation (le journal de SHHFix donnera la réalité au premier lancement).
3. Utilisation de `timeSetEvent` : horloge de la logique du jeu, à comparer à la cadence d'affichage.
4. Boucle de lecture XInput : conversion en commandes, zones mortes, sensibilité de la caméra.
5. Gestion du périphérique perdu (Alt-Tab) : `Reset` et ressources.
6. Constantes de résolution 720p (interface, carte, objets).

## Premier lancement de vérification (09/10/2026, journal SHHFix 0.1.0, Windows 11 build 26300)

Tout ce qui suit est **vérifié par le journal** (le mod ne faisait que journaliser, aucun réglage modifié).

- **Le proxy `version.dll` est chargé** et le jeu démarre normalement, sans plantage (aucun minidump).
- **Hypothèse confirmée** : à l'exécution de notre `DllMain`, `g_SilentHill.sgl` est déjà mappé (pas d'avertissement)
  et les hooks de son IAT (`Direct3DCreate9`, `Sleep`, `SetUnhandledExceptionFilter`) ont fonctionné.
- `Direct3DCreate9` du moteur passe d'abord par `apphelp.dll` (shim de compatibilité de Windows) : sans conséquence,
  mais à garder en tête si un correctif doit lire l'IAT.
- **Création du périphérique par défaut** : 800x600, format 22 (X8R8G8B8), 1 tampon, MSAA 0, swap 3 (COPY),
  **plein écran**, 120 Hz, intervalle de présentation 0 (immédiat), comportement 0x40 (HARDWARE_VERTEXPROCESSING).
- **Cadence verrouillée à 30 i/s** : 29,8 i/s mesurés, images de 33,5 ms (p99 ~35 ms). Le fichier `Engine\vars_pc.cfg`
  créé au premier lancement contient `fpsLimit=30` et `maxFPSLimit=30` (les deux réglages sont donc bien la source
  du plafond, ce qui reste à confirmer en les changeant), ainsi que `FullScreen=true`, `LowQualityShaders=true`,
  `AutoDownResTextures=512`, `mousespeed=1.0` et `controlscheme=0`.
- **Deux threads présentent des images** et alternent (chargement / jeu) ; le thread de rendu garde une précision x87
  de 53 bits, le thread principal de 24 bits (simple précision) après `CreateDevice` sans `D3DCREATE_FPU_PRESERVE`.
  Même situation que Deadly Premonition (caméra saccadée en simple précision) : piste à tester avec le drapeau.
- **Mémoire** : espace d'adressage limité à 2047 Mo (pas LARGE_ADDRESS_AWARE), 810 à 840 Mo utilisés après 6 minutes
  dans le menu et les premiers niveaux : marge confortable pour l'instant, la limite 4 Go pèsera surtout avec des
  textures plus grandes ou des résolutions élevées.
- **Saccades** : quelques pointes de 80 à 200 ms aux changements de thread de rendu (chargements), aucune en jeu
  continu.
- **Alt-Tab en plein écran** : `Present` renvoie `0x88760868` (`D3DERR_DEVICELOST`), puis `Reset` (800x600 plein écran)
  réussit et `Present` refonctionne. **Sur cette machine, le jeu d'origine survit donc à l'Alt-Tab** (interruption de
  14 s, le temps de l'avoir quitté), contrairement à ce que dit la réputation du jeu ; les plantages signalés sont
  peut-être liés au contexte (résolution élevée, mode fenêtré, alt-tab en menu, autre GPU). À re-tester dans ces cas.
