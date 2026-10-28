# À faire

Points en attente, avec ce que l'on sait déjà. À tenir à jour : retirer un point une fois traité.

## Prochaine étape immédiate : premier lancement de vérification

Le code actuel ne modifie rien dans le jeu : il journalise. Il sert à confirmer deux hypothèses de
`docs/analyse-shh-exe.md` avant d'aller plus loin.

1. FAIT (09/10/2026, sur confirmation) : compilation propre, `version.dll` (17 exports) et `HomecomingAgain.ini` copiés dans
   `Bin\` à côté de `SilentHill.exe`. Retour en arrière : supprimer ces deux fichiers (et le dossier `Bin\HomecomingAgain`).
2. Lancer le jeu, jouer deux minutes, quitter, puis lire `Bin\HomecomingAgain\logs\HomecomingAgain_*.log` et vérifier :
   - la ligne de démarrage de HomecomingAgain (le proxy `version.dll` est bien chargé) ;
   - l'absence de l'avertissement « g_SilentHill.sgl pas encore chargé » (le moteur est mappé à ce moment-là) ;
   - `Direct3DCreate9`, `CreateDevice` (résolution, MSAA, fenêtré) et le rapport de cadence (30 i/s ?).
3. Si le moteur n'est pas encore mappé dans `DllMain` : différer l'installation des hooks (premier appel d'une
   fonction de `version.dll`, ou hook de `LoadLibraryA` dans le lanceur).

## Plan pour faire mieux que l'existant

Constat (voir `docs/recherche-web.md`) : l'essentiel existe déjà dans un patch fermé. HomecomingAgain doit être **ouvert,
non destructif, et corriger ce qu'il laisse de côté**. Chaque ligne = un objectif mesurable.

### 1. Plantages et cadence (objectif 1)
- Alt-Tab / périphérique perdu : traiter proprement `Reset` (libérer puis recréer les ressources) au lieu de fermer
  le jeu. Dépend de l'analyse des ressources `D3DPOOL_DEFAULT` du moteur.
- Plantage au-delà de 720p et lors de l'entrée dans certaines zones à haute résolution (témoignages) : retrouver la
  cause par minidump, ne pas deviner.
- Mémoire : drapeau 4 Go sans modifier le fichier d'origine (copie `SilentHill_4GB.exe` ou lanceur), sur
  confirmation. Message d'erreur clair si `d3dx9_38`, `xinput1_3` ou le CRT VC80 manquent.
- Verrou 30 i/s : le trouver, puis limiteur propre (QueryPerformanceCounter + minuteur haute résolution, déjà dans
  `framepacing/`). Vérifier que la logique (timeSetEvent), les animations, la physique et les dialogues suivent.
- Windows 11, GPU récents : tester et consigner.
- Steam Deck / Proton (19/10/2026) : **vérifié** par un joueur (CachyOS, CachyOS Proton) que le mod ne se charge pas
  sans l'option de lancement `WINEDLLOVERRIDES="version=n,b" %command%` (Wine prend sa `version.dll` intégrée).
  Documenté dans les README ; le launcher détecte Wine (`wine_get_version` dans ntdll) et affiche l'option. Reste à
  confirmer qu'avec l'option tout fonctionne (journal dans `Bin\HomecomingAgain\logs`, SMAA, cadence, DXVK et DLSS).
  - Manette intégrée du Steam Deck qui ne marche que sur l'écran « appuyez sur une touche » (rapport d'un joueur).
    **Hypothèse non vérifiée** : la variable `inputstream` de `Engine\vars_pc.cfg` choisit le jeu de commandes de
    `binds_pc_mjs.cfg` (0 = clavier et souris, `setbind 0` ; 1 = manette, `setbind 1`). Elle valait 0 au premier
    lancement et 1 plus tard sur le PC de l'auteur ; l'écran « appuyez sur une touche » réagirait à toute entrée
    alors que le reste suivrait `inputstream`. `targetinputstream` existe aussi dans `g_SilentHill.sgl`. À vérifier :
    valeur de `inputstream` dans le `vars_pc.cfg` du joueur, puis désassembler qui lit et écrit ces deux variables.

### 2. Graphismes (objectif 2)
- Résolution et ratio : constantes 720p, interface, carte, objets, photos ; 16:9, 21:9, 32:9.
- Champ de vision réglable (Hor+ en ultra-large).
  - Fait (27/10/2026) : écran noir hors 16:9 corrigé (`graphics/ClipCopy`, `ClipFinalCopy=1`). Vérifié en jeu en
    1920x810. Cause **vérifiée** : la dernière copie du moteur (`StretchRect` de la cible de scène vers le tampon
    arrière) vise le rectangle (0,-135,1920,945) qui dépasse le tampon 1920x810 ; Direct3D 9 répond
    `D3DERR_INVALIDCALL`. Le mod rogne la destination et la source de la même quantité.
  - Mesures **vérifiées** (relevés F11 du 27/10/2026, menu principal, 1920x810 et 1920x1080) : le moteur dessine
    tout (monde 3D et interface) dans des cibles 16:9 de largeur égale à la résolution (1920x1080 ; 3840x2160 pour un
    tampon 3840x1080), viewport plein cible, puis centre cette image sur le tampon arrière en rognant haut et bas.
    Le champ de vision horizontal reste donc celui du 16:9 : un écran ultra-large perd de la hauteur au lieu de
    gagner de la largeur. La projection du monde (mWorldViewProj, c0..c3) a un rapport échelle y / échelle x de 16/9
    (1,6406 et 2,9163 relevés), identique à 1920x810 et à 1920x1080 : le ratio est fixé à 16:9 par le moteur.
  - Fait (28/10/2026) : prototype Hor+ (`graphics/HorPlus`, `[Graphics] HorPlus`, désactivé par défaut), **vérifié en
    jeu en 1920x810** (menu, save point, scène de la table de la nouvelle partie : plus d'ombres cassées). Le moteur
    envoie des constantes pour du 16:9 qu'il faut corriger avec la projection (relevés F11, **vérifié** par
    élimination avec des réglages de debug, depuis retirés) : projection du monde c0 x4 (ligne x multipliée par
    (16/9)/(L/H)), projection seule c222 x4, inverse des échelles c117 ([1/sx, 1/sy], pixel shader), conversion
    position pixel vers écran c116 ([2/L, -2/H, -L/2, -H/2], pixel shader), décalage d'un demi-pixel c194 (vertex
    shader) et échelle écran c111 ([1/L, 1/H], pixel shader de la lumière volumétrique `ps_1C62CA10`, qui lit la
    profondeur R32F par `vPos` x c111 et reconstruit la position monde). Sans la correction de c111, des taches noires
    à bords en escalier apparaissaient sur les personnages (les cibles pleine résolution redimensionnées sans elle).
    Vérifié en jeu en 1920x810 (28/10/2026) : cinématique des insectes, menus, pause (le voile du menu pause, un quad
    d'interface à texture 32x32, n'est plus comprimé au centre : `BoundTextureIsOverlay`, hypothèse valable pour ce
    seul menu).
    **Bug signalé par l'auteur (28/10/2026), non traité** : des éléments (cercles de sauvegarde) disparaissent quand
    ils sortent du champ de la caméra d'origine. **Hypothèse non vérifiée** : le moteur élimine les objets avec un
    frustum calculé sur le ratio 16:9 ; Hor+ élargit l'image sans élargir ce frustum, donc les objets du bord sont
    éliminés. Il faut trouver dans `g_SilentHill.sgl` où le frustum est construit (champ de vision et ratio) et
    l'élargir comme la projection.
    **Corrigé le 28/10/2026 (vérifié en jeu en 1920x810)** : un point d'arrêt matériel d'écriture sur la matrice de
    projection CPU (outil du scratchpad, `DebugActiveProcess` + registres de débogage Wow64) a donné la fonction du
    moteur qui la construit, `0x10B29100` : `m00 = [caméra+0x94] / tan(...)`, `m11 = 1 / tan(...)`. Le champ
    `[caméra+0x94]` est l'inverse du ratio (0,5625), copié par le constructeur de la caméra (`0x10B9C530`) depuis la
    constante unique `0x1109615C`. Remplacer cette constante par H/L du tampon arrière (`[Graphics] HorPlusEngine=1`,
    défaut) donne au moteur son propre Hor+ : projection, frustum de culling et matrices du GPU cohérents. Le cercle de
    sauvegarde reste entier même tout au bord de l'image. La correction par constantes (c0 du monde) devient sans objet
    pour ces caméras (leur ratio n'est plus 16:9) ; les autres corrections (c116, c111, c194, voile de pause, cibles)
    restent. L'essai du même jour sans mesure de la matrice avait conclu à tort à un échec (jugé sur les cibles).
    Pistes explorées le 28/10/2026 (résultats **vérifiés** sauf mention) : (1) le moteur a un champ `AspectRatio` dans
    un objet caméra (chaîne `AspectRatio` en 0x11080548, sérialiseur en 0x109A5890) et une option `TOption_AspectRatio` ;
    (2) la mémoire du jeu contient, pour la caméra de rendu, une structure avec 0,5625, la taille 1920 / 1080 en flottants
    (offset +0x58 / +0x5C avant la matrice) puis la projection CPU (1,64067 et 2,91675), les matrices vue et vue-projection
    et d'autres (relevé mémoire, adresses variables d'une exécution à l'autre) ; (3) remplacer la constante 0,5625 du
    moteur (0x1109615C, une seule référence, un constructeur) par H/L ne change pas les cibles : le moteur crée
    toujours du 1920x1080, donc cette constante ne fixe pas le ratio (essai retiré). Prochaine étape envisagée :
    un point d'arrêt matériel sur l'écriture de la matrice de projection CPU pour trouver la fonction qui la calcule et
    celle qui construit le frustum de culling (les appels passent par la surcouche Steam : l'adresse de retour ne donne
    pas l'appelant du moteur).
    Reste à tester : carte, objets examinés, DLSS, 2560x1080 / 3440x1440 / 3840x1080 sur un vrai écran ultra-large,
    et d'autres constantes à 1080 que le moteur pourrait envoyer en blocs de plusieurs registres.
  - Reste (**hypothèse de conception, non vérifiée**) : Hor+ demande trois changements ensemble : cibles de dessin à la
    taille du tampon arrière (au lieu de largeur x 9/16), échelle x de la projection du monde multipliée par
    (16/9) / (L/H), et interface limitée à la zone 16:9 centrale. Les modules SMAA, SSAO, vecteurs de mouvement et DLSS
    lisent les tailles des cibles : à revoir avec. La construction de la projection dans le moteur n'est pas encore
    trouvée (aucune constante 1,7777778 en flottant ; 0,5625 référencé une fois, en 0x10B9C55B, sans rapport établi).
- Anticrénelage : MSAA via les paramètres de présentation, puis post-traitement (SMAA) à la manière de DPfix
  (DPfix est GPL : réutilisable avec ses crédits, voir `E:\DeadlyPremonitionFix\third_party\dpfix`).
- Qualité : suppression optionnelle du grain, filtre de tonalité, gamma, ombres (résolution), distance d'affichage.
- Limites du moteur à constater avant de promettre quoi que ce soit.

### 3. Manette (objectif 3)
- Intercepter `XInputGetState` (ordinal 2) et `XInputSetState` (ordinal 3) du moteur : zones mortes, courbe et
  sensibilité de la caméra réglables (point faible signalé par des joueurs), échange des gâchettes, vibration.
- Fait (14/10/2026) : icônes de boutons Xbox 360 ou PS3 partout (menus, inventaire, commandes, invites en jeu),
  menu principal à la manette. Reste : branchement à chaud, plusieurs manettes.
- Fait (14/10/2026) : correctif de caméra au stick droit porté du Camera Fix d'Emertels (MIT), en mémoire
  (`patches/CameraFix`), désactivé par défaut, activé par le launcher avec les touches du stick droit. Validé en jeu
  à la manette par l'utilisateur. Reste : vérifier que la souris ne passe pas par ces sites.
- Fait (18/10/2026) : caméra orbitale expérimentale (`[Camera] Mode=Orbit`, désactivée par défaut) : le stick droit
  tourne la caméra autour du personnage ; en marche, il se tourne vers la caméra. Validé en jeu à la manette.
  Option `OrbitMove=Stick` (vers la direction du stick) retirée le 20/10/2026 : elle ne fonctionnait pas en jeu.
- Fait (20/10/2026) : en orbite, le personnage ne se tournait plus vers la caméra après la scène des insectes
  (contrôles « tank »). Cause (mesurée par un journal temporaire, retiré) : la lecture de l'entrée de chaque acteur
  effaçait le cap voulu, y compris celle des insectes lue après celle du joueur dans la même image. Seule l'entrée
  du personnage suivi y touche désormais. Validé en jeu par l'auteur.
- Fait (20/10/2026) : `OrbitGuardFollow=1` (défaut) : gâchette gauche (L2) tenue, l'orbite est suspendue (caméra
  derrière le personnage, stick droit qui le tourne). Non validée seule en jeu.
- Fait (21/10/2026) : les réglages utiles de `HomecomingAgain.ini` ont une case, une liste ou un curseur dans le
  launcher (Graphismes : carte, objets ; Manette : icônes des boutons, correctif de caméra, caméra orbitale, L2, vitesses de la caméra ;
  Performances : son). Volontairement hors du launcher, à modifier à la main : `FrameLimitFps`, `Channels`, `AnimatedIcons` et `PlayStationPad` (toujours à 1) (le jeu gère déjà sa manette dans ses
  menus), `SsaoRadius`, `SsaoFadeStart`, `SsaoFadeEnd`, `D3D9On12`, `ChannelOrder`, `SkipKey`, `ReportIntervalSeconds`,
  `DumpKeepCount`, `LogKeepCount` (réglage fin, expérimental ou diagnostic) et `SsaoDebug`, `FrameCapture`,
  `FrameCaptureKey`, `BackBufferDump`, `SkipMovies`. Le launcher ne touche pas à ces lignes en enregistrant (test). Un
  onglet Avancé a été essayé puis retiré à la demande de l'auteur. Rendu des nouveaux écrans à vérifier à l'œil.
- Fait (20/10/2026) : une piste de la cinématique des insectes n'était entendue qu'à gauche. **Vérifié** par le
  journal : le canal 0 (avant gauche) du flux 5.1 du moteur est vide en jeu courant (0,0000) et porte 0,08 à 0,11
  pendant la scène. Il est maintenant envoyé à égalité des deux côtés (`DownmixSurroundToStereo`). Validé à
  l'écoute par l'auteur.
- À voir : léger décalage entre cette piste (ou la voix d'Alex) et l'image dans la même scène. **Hypothèse non
  vérifiée** : la scène est calculée par le moteur et avance image par image, donc plus vite à `TargetFps=60` qu'à
  30 (tester `TargetFps=0`). Piste : repasser à 30 i/s pendant les scènes du moteur, repère à trouver (nom de la
  caméra active ? état du moteur ?).
- Idée (option à ajouter) : suspendre l'orbite pendant la garde au couteau (L2, `COMMAND_STALK`), caméra remise
  derrière le personnage et stick droit qui le tourne. Aujourd'hui l'orbite reste active pendant la garde ; seule la
  visée aux armes à feu (caméra « Precision ») rend la main au jeu. Repérer la gâchette par la manette (XInput).
- À savoir : chaque emplacement de sauvegarde (`shv_save.bin`) contient ses propres touches, qui passent devant
  `binds_pc_mjs.cfg` (voir `docs/analyse-shh-exe.md`). Une sauvegarde aux touches changées peut donc aussi annuler
  les touches libérées par le correctif de caméra. Piste : option pour toujours utiliser `binds_pc_mjs.cfg`.
- Manettes non XInput (PlayStation) : conversion comme `ControllerMapping` de DPStabilityFix.
- Clavier / souris : touches reconfigurables via `binds_pc_mjs.cfg` (le fichier d'origine est déjà un format texte).

### 4. Au-delà (idées à valider avec l'utilisateur)
- Lanceur/installeur à la DPStabilityFix : installation et désinstallation propres (`version.dll`, réglages),
  vérification des dépendances, ouverture des journaux.
- Sauvegardes : sauvegarde de secours avant écriture, dossier de sauvegardes lisible.
- Outils de diagnostic exposés au joueur : capture de journal et de minidump en un clic pour les rapports de bogue.
- Fidélité : conserver l'expérience d'origine par défaut (difficulté, objectifs, conseils intacts), contrairement à
  l'existant qui modifie l'équilibrage.

## Fait

- Dépôt initialisé (09/10/2026) : modules `core`, `crash`, `framepacing` repris de DPStabilityFix et adaptés,
  `graphics/D3D9Hooks` simplifié (journalisation des paramètres, mesure de cadence). Premier build propre.
- Analyse d'identité du lanceur et du moteur (voir `docs/analyse-shh-exe.md`) : proxy `version.dll` écrit,
  hooks portés sur le moteur, import par ordinal ajouté. **Pas encore recompilé après ce changement.**
