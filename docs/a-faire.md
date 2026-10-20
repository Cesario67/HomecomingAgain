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
- Windows 11, GPU récents, Steam Deck / Proton : tester et consigner.

### 2. Graphismes (objectif 2)
- Résolution et ratio : constantes 720p, interface, carte, objets, photos ; 16:9, 21:9, 32:9.
- Champ de vision réglable (Hor+ en ultra-large).
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
- Fait (21/10/2026) : tous les réglages utiles de `HomecomingAgain.ini` ont une case, une liste ou un curseur dans le
  launcher (Graphismes : carte, objets, icônes animées, manette dans les menus, L2, vitesses de la caméra ;
  Performances : limiteur d'images, son), et un onglet Avancé reçoit le reste (SSAO fin, ordre des canaux, touche de
  saut des vidéos, nombre de dumps et de journaux, D3D9On12). Restent hors du launcher, volontairement : `SsaoDebug`,
  `FrameCapture`, `FrameCaptureKey`, `BackBufferDump`, `SkipMovies` (outils de diagnostic et de test). Rendu des
  nouveaux écrans à vérifier à l'œil (les liaisons sont vérifiées à la compilation, pas la mise en page).
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
