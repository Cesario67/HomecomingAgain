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
