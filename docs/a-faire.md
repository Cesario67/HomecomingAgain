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
- Branchement à chaud, plusieurs manettes, invites de boutons Xbox correctes (les icônes existent dans les
  ressources du jeu, à l'écran selon le périphérique actif).
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
