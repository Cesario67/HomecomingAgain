# Projet

**HomecomingAgain** : mod pour *Silent Hill: Homecoming* (PC, Steam appid 19000, Double Helix 2008) qui vise à le
rendre jouable sur les machines d'aujourd'hui. Objectifs, par priorité :

1. **Plantages et cadence** : corriger les plantages connus (Windows 10/11, résolution, fréquence), régulariser
   la cadence d'images (verrou 30 i/s signalé), compatibilité avec le matériel actuel.
2. **Graphismes** : résolution et ratio (16:9, 21:9), champ de vision, anticrénelage, carte et interface, dans
   la limite de ce que le moteur permet réellement.
3. **Manettes Xbox 360 / Xbox** : lecture et correspondance des boutons correctes.

- Jeu installé dans : `E:\SteamLibrary\steamapps\common\Silent Hill Homecoming` (Steam, build 6.30). Deux
  morceaux : le lanceur `Bin\SilentHill.exe` (33 Ko) et le moteur `Bin\g_SilentHill.sgl` (une DLL de 20 Mo, non
  protégée). Voir `docs/analyse-shh-exe.md`.
- Projet frère, même méthode et mêmes outils : `E:\DeadlyPremonitionFix` (DPStabilityFix). Y puiser le code
  et les leçons (hooks, proxy, manette, plantages), pas l'inverse.
- Recherches web sur le jeu et le correctif communautaire existant : `docs/recherche-web.md`. Tout y est
  **non vérifié** tant que l'exécutable n'a pas été désassemblé.

## Architecture

Reprise de DPStabilityFix, adaptée (espace de noms `hca`, projet `HomecomingAgain`). Modules, une responsabilité chacun :

- `core/` : chemins, config `.ini`, journal, hooking (IAT, vtables COM), infos système.
- `crash/` : filtre d'exceptions non gérées chaîné avec celui du jeu, minidumps, rapport.
- `framepacing/` : mesures de cadence, statistiques de `Sleep`, résolution du minuteur, limiteur.
- `graphics/` : `Direct3DCreate9` (IAT) -> `CreateDevice` -> `Present`/`Reset` (vtables). Journalise les
  paramètres de présentation et mesure la cadence, sans rien modifier pour l'instant.
- `proxy/` : `version.dll`, première DLL importée par le lanceur, transmise à la vraie (17 exports). C'est ce qui
  charge HomecomingAgain. Nos hooks visent l'IAT du **moteur** `g_SilentHill.sgl` (d3d9, XINPUT1_3 par ordinaux 2/3/4,
  DINPUT8), pas celle du lanceur.
- À venir selon l'analyse : `input/` (manette XInput), `patches/` (fonctions internes du moteur, vérifiées par
  signature avant application).
- `DllMain` ne fait que des opérations sûres sous le verrou du chargeur ; ce qui charge des DLL est différé au
  premier `Direct3DCreate9` (`LateInit`).
- Chaque fonctionnalité doit pouvoir être désactivée dans le `.ini` (`dist/HomecomingAgain.ini`).
- Toute nouvelle fonctionnalité doit être couverte par un test (`tests/unit` pour les fonctions pures ; un faux
  jeu `tests/fake_game` à recréer comme dans DPStabilityFix dès que le chargeur est choisi).
- Analyse de `SilentHill.exe` : `docs/analyse-shh-exe.md`. Désassembler dans le scratchpad
  (`dumpbin /disasm:nobytes`), jamais dans le dépôt, et consigner adresses et descriptions en distinguant
  **vérifié** de **hypothèse**.

## État

Version 1.0.1 publiée (31/10/2026), dont l'ultra-large (Hor+, sélecteur de format du launcher). Le mod est installé et testé en jeu à chaque modification : cadence,
sans bordure, son, SMAA, SSAO, ombres 8192, DLSS, manette (menus, icônes, noms des boutons, zoom de la carte, caméra orbitale, manette
PlayStation sans Steam Input), carte entière et objets examinés au-delà du 720p, logos accélérés, triches (mode dieu,
munitions illimitées), launcher Linux expérimental (jeu dans Proton) et rapport de bogue en un clic. Plan et points en attente : `docs/a-faire.md`. Les fonctionnalités et
réglages sont décrits dans les README.

## Commandes

- Compiler : `powershell -ExecutionPolicy Bypass -File tools\build.ps1` (preset `x86-release`).
- Tests unitaires : `build\x86-release\HomecomingAgainUnitTests.exe` (code de sortie = nombre d'échecs).
- Les scripts `.ps1` doivent rester en UTF-8 **avec BOM** (sinon PowerShell 5.1 abîme les accents).
- CI GitHub Actions (`.github/workflows/build.yml`) : compilation et tests unitaires.

## Toolchain

- **Build Tools for Visual Studio 2022** : MSVC, SDK Windows, CMake et Ninja fournis par les Build Tools.
  Éditeur : VS Code.
- Cible **Win32 (x86)** (lanceur et moteur 32 bits, vérifié) : compiler depuis un environnement
  `vcvarsall.bat x86` (ou `amd64_x86`).
- C++20, `/W4`, avertissements traités comme des erreurs.
- Dépendances non libres ou volumineuses : téléchargées par `FetchContent` avec empreinte SHA-256, jamais
  stockées dans le dépôt.
- Toute copie d'un binaire compilé (paquet, faux jeu) se fait en `POST_BUILD` **de la cible qui le produit**.
- Pour prouver qu'un test détecte un bug, le vérifier une fois **sans** le correctif (il doit échouer).

## Conventions C++

- Typer explicitement les signatures de fonctions et les membres ; `auto` seulement quand le type est
  évident à la lecture (itérateurs, `make_unique`, casts).
- Pas de `new`/`delete` nus : RAII partout (`std::unique_ptr`, wrappers de `HANDLE` avec deleter).
- Respecte le principe de responsabilité unique : un fichier/une classe = une responsabilité claire.
  Pas de logique métier dans les fonctions de hook : le hook délègue à un module testable.
- Les hooks doivent être **défensifs** : ne jamais lancer d'exception à travers une frontière de hook,
  toujours appeler la fonction d'origine si notre traitement échoue, journaliser l'erreur.
- Pas d'allocation ni d'E/S bloquante dans les chemins chauds (hook de `Present`, boucle de jeu).
- Nommage : `PascalCase` pour les types, `camelCase` pour les fonctions et variables, `kConstante`
  pour les constantes, `g_` pour les globales (à limiter au strict nécessaire).
- Chaque adresse, offset ou signature issu de la rétro-ingénierie est documenté en commentaire :
  ce que fait le code ciblé, comment il a été identifié, version de `SilentHill.exe` concernée.

## Langue des commentaires

- **Tous les commentaires sont écrits en anglais** : code C++ (DLL) et C# (launcher), XAML, scripts, et commentaires de
  `HomecomingAgain.ini`. Les nouveaux commentaires, et ceux qu'on modifie, sont en anglais.
- Les commentaires français existants sont traduits au fil des modifications (traduire ceux du fichier qu'on touche).
  Une traduction de tout le dépôt d'un coup est un travail à part, à ne faire que sur demande.
- Cela ne change pas : les textes affichés au joueur (launcher en français et en anglais, `Core/Translations.cs`), la
  documentation du dépôt (`CLAUDE.md`, `docs/`, README) et les messages de commit, qui restent comme ils sont
  aujourd'hui. Les règles existantes valent aussi pour les commentaires anglais (pas de tiret cadratin, hypothèse
  distinguée du vérifié).

## Licence

- Projet sous **GPL-3.0-or-later** (`LICENSE`, copyright Cesar Schaal). Le paquet distribué doit contenir
  `LICENSE.txt`.
- Les correctifs communautaires existants (Steam006 2018, **unknownproject** jusqu'à la v3.10 de 2024, voir
  `docs/recherche-web.md`) servent de **liste de ce qui est possible**. Ils sont sans licence et leur auteur
  s'oppose à toute réutilisation : ne jamais copier ni redistribuer leurs binaires ou leur code, ne pas les
  télécharger ni les désassembler pour en tirer des adresses. Nos adresses viennent de notre seule analyse de
  l'exécutable d'origine, et nos tests se font sur un jeu d'origine (sans leur patch, qui bloque les wrappers).
- Ne jamais intégrer de code ou de shader sans licence libre vérifiée. Seule exception, décidée par l'auteur le
  13/10/2026 : le SDK DLSS de NVIDIA (licence NVIDIA RTX SDKs), utilisé uniquement par le programme DLSS (`helper/`),
  sous la permission supplémentaire GPL article 7 décrite dans le README. Jamais stocké dans le dépôt : téléchargé
  à la compilation avec empreintes SHA-256 (`helper/cmake/DlssSdk.cmake`).

## Sécurité des données du joueur et du jeu

- **Ne jamais modifier les fichiers du jeu** (exécutable, DLL, données, `Engine\vars_pc`...) ni les sauvegardes
  sans confirmation explicite de l'utilisateur. La lecture et l'analyse sont autorisées.
- Toute opération sur une sauvegarde doit d'abord en garder une copie.
- Le déploiement de la DLL dans le dossier du jeu se fait sur demande, jamais automatiquement.
- Ne jamais committer de fichiers du jeu (exécutable, données, sauvegardes, minidumps contenant de la
  mémoire du jeu) : seulement notre code et des signatures.

## Workflow avec Claude Code

- **À chaque modification, appliquer la méthode de `docs/methode-de-travail.md`** (lire ce fichier en début de
  session) : rétro-ingénierie, correctifs vérifiés par octets, tests (et preuve qu'ils détectent le défaut),
  installation et tests en jeu, commits, PR et release. En particulier : d'autres sessions peuvent travailler en
  même temps dans ce dossier, donc **committer en nommant ses fichiers un par un, jamais `git add -A`**.
- **Mesurer avant de corriger** : capture, journal de diagnostic temporaire (une ligne par changement, marqué
  `DIAGNOSTIC TEMPORAIRE`, retiré une fois la cause trouvée), relevé d'image F11, désassemblage des shaders
  (`fxc /dumpbin`). Quand une hypothèse est démentie par une mesure, le dire et repartir de la mesure.
  Pour les touches et icônes de la manette (bouton physique, commande, icône, noms affichés), les adresses vérifiées
  et la technique sont dans `docs/methode-de-travail.md`, section « Touches et icônes de la manette ».
- **Mode test autonome** : avec l'accord de l'utilisateur (à demander ; il peut le laisser actif jusqu'à « stop »),
  Claude ferme le jeu (`WM_CLOSE`), installe, relance et charge la partie au clavier, et vérifie par captures
  d'écran (procédure dans `docs/methode-de-travail.md`, section 5). Si l'utilisateur joue, demander avant de fermer.

- L'utilisateur a de solides bases en **C#**, pas en C++ : expliquer les concepts C++ et Win32 nouveaux
  avec des équivalents C# quand c'est pertinent (pointeurs de fonction / délégués, RAII / `IDisposable`,
  `HANDLE` / `SafeHandle`, etc.).
- Ne jamais lancer de build (`msbuild`, `cmake --build`...) ni le jeu de ta propre initiative :
  toujours demander confirmation avant (le jeu : sauf en mode test accordé, voir plus haut).
- Claude ne joue pas : les tests de jeu proprement dits (combats, exploration, manette) sont faits par l'utilisateur,
  qui remonte journaux et minidumps. Préciser à chaque itération exactement quoi tester et quoi rapporter. En mode
  test, Claude peut lancer le jeu, charger la partie, ouvrir des menus et vérifier par captures d'écran.
- Les commandes `git` (y compris commit/push) sont autorisées.
- Toujours travailler sur la branche **`dev`** (commits et push sur `dev`, jamais directement sur `main`).
- Ne pas utiliser de tiret cadratin (« — ») dans les livrables écrits : commentaires de code,
  documentation, messages de commit. Préférer des virgules, deux-points, parenthèses ou des phrases
  séparées. (Sans importance dans les réponses de chat.)
- Ne pas présenter une hypothèse sur le fonctionnement interne du jeu comme un fait : distinguer ce qui
  est vérifié (désassemblage, dump, test) de ce qui est supposé.

## Style de réponse

Adaptatif :
- Sujet simple : direct et concis.
- Sujet complexe, ou contexte que l'utilisateur semble ne pas maîtriser (C++, Win32, rétro-ingénierie) :
  expliquer en détail le raisonnement, et proposer les alternatives avec leurs compromis.
- Quand tu proposes une implémentation, justifie brièvement les choix d'architecture.

## Launcher (`launcher/`)

Repris de DPStabilityFix (mêmes conventions C#, voir le CLAUDE.md de ce projet : MVVM strict, liaisons compilées,
textes dans `Core/Translations.cs` en français et en anglais, fichiers modifiés en place). Spécificités :
- Trois onglets : Graphismes, Performances et stabilité, Installation. Les réglages sont répartis entre
  `Bin\HomecomingAgain.ini` (mod, `IniDocument`) et `Engine\vars_pc.cfg` (jeu, `KeyValueDocument`, copie de secours
  `vars_pc.cfg.hca-backup` à la première modification). Correspondance dans `Core/PatchSettings.cs`.
- Le jeu réécrit `vars_pc.cfg` en quittant : toute écriture est refusée tant qu'il tourne.
- L'installation applique le patch 4 Go à `Bin\SilentHill.exe` (original : `SilentHill.exe.hca-original`, remis à
  la désinstallation, voir `Core/ExePatcher.cs`), puis copie `Bin\version.dll`, `Bin\HomecomingAgain.ini` s'il est absent,
  les licences et le launcher dans `Bin`.
- `tools\build.ps1` compile la DLL puis publie `HomecomingAgain.exe` (autonome, ~21 Mo) qui l'embarque.
- Tests : `dotnet test launcher` (xUnit v3).
