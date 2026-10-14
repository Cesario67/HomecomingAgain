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
2. `Direct3DCreate9` : paramètres de présentation (le journal de HomecomingAgain donnera la réalité au premier lancement).
3. Utilisation de `timeSetEvent` : horloge de la logique du jeu, à comparer à la cadence d'affichage.
4. Boucle de lecture XInput : conversion en commandes, zones mortes, sensibilité de la caméra.
5. Gestion du périphérique perdu (Alt-Tab) : `Reset` et ressources.
6. Constantes de résolution 720p (interface, carte, objets).

## Premier lancement de vérification (09/10/2026, journal HomecomingAgain 0.1.0, Windows 11 build 26300)

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

## Deuxième lancement (10/10/2026) : plafond 60 sans effet et plantage à l'Alt-Tab reproduit

Avec `fpsLimit=60` et `maxFPSLimit=60` dans `vars_pc.cfg` (copie de secours `vars_pc.cfg.hca-backup`) :
- **Cadence toujours à 29,2 i/s** : ces réglages ne suffisent pas, le plafond est aussi une constante ou une borne dans
  le code du moteur (**vérifié** par la mesure ; sa nature exacte reste à trouver).
- **Alt-Tab en plein écran : plantage reproduit** (contrairement à la première session). `Present` renvoie
  `D3DERR_DEVICELOST` (0x88760868), puis le premier `Reset` échoue aussitôt avec `D3DERR_INVALIDCALL` (0x8876086C,
  6 ms) et le jeu plante 6 ms plus tard : `EXCEPTION_ACCESS_VIOLATION` en lecture à l'adresse 0, EIP =
  `g_SilentHill.sgl+0xB8DBF2` (0x10B8DBF2), thread principal. Minidump écrit dans `Bin\HomecomingAgain\crashdumps`.
- **Hypothèse** (non vérifiée) : `Reset` échoue parce que le moteur n'a pas libéré toutes ses ressources
  `D3DPOOL_DEFAULT` (cela dépend de ce qu'il tenait à cet instant, d'où le succès de la première fois), puis le
  moteur déréférence un pointeur nul. Un contournement possible : sur `INVALIDCALL`, ne pas rendre l'échec au moteur
  tant que la cause n'est pas traitée (voir l'ordre `Present` / `Reset` dans DPStabilityFix). À analyser avec le
  minidump et le désassemblage autour de `0x10B8DBF2` et de l'appelant `0x10B898E7`.

## Cadence : le verrou 30 i/s (vérifié le 10/10/2026)

- `fpsLimit` (+0xE4) et `maxFPSLimit` (+0xE8) de l'objet de réglages `[[0x111D2460]+0x38]+0x08` sont bien lus et
  valent 60 en mémoire avec le fichier modifié (relevé HomecomingAgain), et `0x101266CF` ramène `fpsLimit` à `maxFPSLimit` à
  chaque image. **Ils ne commandent pas le limiteur.**
- Le relevé des attentes du rendu montre 93 % du temps dans `SleepEx` (enveloppe `0x1080BAC0`, appel en
  `0x1080BAF6`) et `QueryPerformanceCounter` (enveloppe `0x10892C70`).
- Limiteur : fonction `0x10A4CC90` (via la passerelle `0x100643D0`). Elle convertit en ticks la durée lue par
  `fld dword ptr ds:[0x1108E804]` (instruction `0x10A4CC99`, octets `D9 05 04 E8 08 11`), soit **33,32333 ms**,
  constante de `.rdata` référencée uniquement ici ; puis `SleepEx` et attente active jusqu'à cette échéance depuis
  l'image précédente (horodatage en `0x116C7A00`).
- Correctif `patches/FrameRateCap` : l'opérande est redirigé vers une variable du mod (`1000 / TargetFps`).
  Mesuré : **59,5 i/s** au menu en 1920x1080 (fenêtré et sans bordure). L'écran de chargement (thread séparé)
  reste à 29 i/s : limiteur distinct, sans importance.
- Mise à jour du jeu : `0x10126707` et suivants montrent que la logique utilise une durée d'image variable, moyennée
  sur 5 images et bornée à 0,08 s (`0x10F94818`). À vérifier en jeu : vitesse des animations, de la physique, des
  dialogues et des QTE à 60 i/s.

## Fenêtré 1080p et blocage (10/10/2026)

- Blocage du premier essai sans bordure : « Application Hang » de Windows 15 s après le lancement, pendant le
  chargement. Pendant les ~20 premières secondes, le thread principal charge sans traiter les messages et un autre
  thread présente l'écran de chargement : en fenêtré, une entrée utilisateur pendant ce temps fait déclarer le jeu
  « ne répond pas ». Hypothèse cohérente avec le journal, corrigée par `DisableProcessWindowsGhosting`.
- Deux sessions suivantes (avec et sans bordure, 1920x1080) : aucun blocage, jeu réactif, 60 i/s.
- En fenêtré, pas de périphérique perdu à l'Alt-Tab : le plantage `0x10B8DBF2` (plein écran) ne devrait plus se
  produire. À confirmer par un test en jeu.

## Blocage au lancement en fenêtré avec l'overlay Steam (10/10/2026, 09:30)

Le jeu ne répond plus pendant le chargement (pas de plantage : processus vivant, « Responding = False »). Diagnostic
du détecteur de blocage (minidump `SHH_20261010-093053-908.dmp`) :
- **thread principal** : attend dans `EnterCriticalSection` (appel en `0x10891F07`, enveloppe de verrou du moteur),
  depuis `0x10B8DB37` ;
- **thread de l'écran de chargement** : dans `Present`, passé par `gameoverlayrenderer.dll` (overlay Steam) puis le
  pilote (`nvd3dum.dll`), en attente.
Hypothèse (non prouvée) : l'overlay, appelé depuis le thread de chargement, attend le thread de la fenêtre (messages),
qui attend lui-même le verrou du moteur tenu par le thread de chargement : interblocage. N'existe qu'en fenêtré
(le plein écran exclusif ne passe pas par ce chemin) et pas à chaque lancement.
Constat : **3 lancements sur 3 sans overlay** (exécutable lancé directement) sans blocage ; 1 blocage sur 3 lancements
via Steam avec overlay (plus le blocage de 08:49, même phase de chargement).
Contournement : désactiver l'overlay Steam pour ce jeu. Correctif dans le mod à étudier (Present du thread de
chargement quand l'overlay est présent).

## Correctifs des blocages au chargement avec l'overlay Steam (10/10/2026)

Deux interblocages distincts, tous deux pendant le chargement initial en fenêtré, overlay Steam chargé :
1. **Thread de la fenêtre bloqué sur un verrou du moteur** (`EnterCriticalSection` en `0x10891F07`) pendant que le
   thread de chargement, dans `Present` (overlay), attend ce même thread. Correctif `compat/WindowThreadLock` : pour
   le seul thread de la fenêtre, attente du verrou en traitant les messages envoyés. Le journal montre qu'à
   **chaque** lancement, le thread de la fenêtre attend ce verrou et traite un message envoyé pendant l'attente.
2. **Les deux threads bloqués dans le pilote NVIDIA** (`nvd3dum.dll`), chacun dans un appel Direct3D (principal
   depuis `0x10B8A0D3`). Cause : périphérique créé sans `D3DCREATE_MULTITHREADED` (comportement `0x40`) mais utilisé
   par deux threads. Correctif : le drapeau est ajouté à `CreateDevice` (option `[Compat] D3DMultithreaded`).

Mesure avec les deux correctifs : **8 lancements sur 8 via Steam, overlay chargé, sans blocage**, 59,5 i/s en
1920x1080 sans bordure (avant : 1 blocage sur 3 avec le seul correctif 1, 2 sur 5 sans correctif).

## Patch 4 Go (10/10/2026)

Appliqué par le launcher (`HomecomingAgain.exe install`) avec l'accord de l'utilisateur : bit `IMAGE_FILE_LARGE_ADDRESS_AWARE`
(0x20) de `SilentHill.exe`, original conservé dans `SilentHill.exe.hca-original`. Vérifié en jeu : espace
d'adressage **4095 Mo** (2047 avant), 59,6 i/s, aucun blocage. Prérequis aux essais de rendu en haute résolution.

## Caméra à la manette : sites du Camera Fix d'Emertels (14/10/2026)

Le Camera Fix 1.0 (Emerson Teles, licence MIT, voir `third_party/camerafix/ORIGINE.md`) modifie le moteur d'une
installation Unknown Project 3.20, dont le `g_SilentHill.sgl` diffère du nôtre (SHA-256 `2A7C3003...` contre
`A0EBEEC3...` pour Steam build 6.30). Ses sites ont été comparés à notre moteur, sans rien télécharger de ses binaires :
ses octets ajoutés (`navigation_speed_signatures.h`) ont été posés sur une copie du moteur Steam dans le scratchpad,
puis désassemblés avec `dumpbin`.

**Vérifié** (désassemblage du moteur Steam) :

| Adresse | Instruction d'origine | Rôle |
| --- | --- | --- |
| `0x108DB3D6` | `movss [esp+28h],xmm0` | cap de la caméra de navigation, rangé aussi en `[esi+11Ch]` juste après |
| `0x108DAFFB` | `mulsd xmm1,[0x10F91290]` | tangage à la manette : Y² (`[ebx+24h]` au carré) x 60 degrés/s (double 60.0) |
| `0x108DB012` | `movss xmm1,[0x111D6368]` | tangage de l'autre branche : Y x constante (1400 dans le fichier) |
| `0x103851E0` | `movss xmm1,[esi+0AD8h]` | entrée du personnage |
| `0x10711F24` | `movss xmm0,[0x10F8A2E4]` | juste après le rangement de la sortie horizontale de visée en `[esp+20h]` |
| `0x1071EA28` | `movss [esp+1Ch],xmm0` | sortie verticale de visée ; les indicateurs du `cmp` de `0x1071EA21` servent ensuite |
| `0x10145F50` | `push ebp` ... | fonction cdecl (personnage, degrés), dernier appel en `0x1005D7FB` |
| `0x10D20A56` | `jmp [0x117EED0C]` | relais d'import de la CRT (le Camera Fix y appelle `_CIatan2` ; remplacé par `std::atan2`) |
| `0x10386000-0x103864FF` | `int 3` | zone libre où le Camera Fix met son code (inutilisée par HomecomingAgain) |

Dans la fonction `0x108DABF0` (mise à jour de la caméra de navigation), `edi` reçoit en `0x108DAC1E` le résultat de
`0x10066022` et n'est plus modifié jusqu'à `0x108DB3D6` ; `ebx` vaut `[ebp+0Ch]` à partir de `0x108DAF6D`.

**Hypothèses** reprises du Camera Fix, validées par ses tests en jeu sur son moteur, pas par nous : `edi` = transformation
du personnage suivi (avant en `+0x20`/`+0x28`), `esi+0x120` = vitesse du suivi automatique, `[esp+24h]` = durée de
l'image, `[ebp+0Ch]+0x20` = commande X de la caméra, `+0xAE8`/`+0xAEC` du personnage = entrée de visée, chaîne
`[0x1158982C]+8` vers le nom de la caméra active (« Precision » en visée). Le choix de branche du tangage
(`[ecx+178h]` et bit 0 de `[eax+220h]`) n'est pas identifié : on ne sait pas si la souris passe par l'une d'elles, d'où
la condition « stick droit incliné » de `patches/CameraFix.cpp`.
