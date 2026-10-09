# Projet

**SHHFix** : mod pour *Silent Hill: Homecoming* (PC, Steam appid 19000, Double Helix 2008) qui vise à le
rendre jouable sur les machines d'aujourd'hui. Objectifs, par priorité :

1. **Plantages et cadence** : corriger les plantages connus (Windows 10/11, résolution, fréquence), régulariser
   la cadence d'images (verrou 30 i/s signalé), compatibilité avec le matériel actuel.
2. **Graphismes** : résolution et ratio (16:9, 21:9), champ de vision, anticrénelage, carte et interface, dans
   la limite de ce que le moteur permet réellement.
3. **Manettes Xbox 360 / Xbox** : lecture et correspondance des boutons correctes.

- Jeu installé dans : `E:\SteamLibrary\steamapps\common\Silent Hill Homecoming` (installation en cours au
  09/10/2026). Exécutable attendu : `Bin\SilentHill.exe` (d'après des témoignages, à confirmer).
- Projet frère, même méthode et mêmes outils : `E:\DeadlyPremonitionFix` (DPStabilityFix). Y puiser le code
  et les leçons (hooks, proxy, manette, plantages), pas l'inverse.
- Recherches web sur le jeu et le correctif communautaire existant : `docs/recherche-web.md`. Tout y est
  **non vérifié** tant que l'exécutable n'a pas été désassemblé.

## Architecture

Reprise de DPStabilityFix, adaptée (espace de noms `shhf`, projet `SHHFix`). Modules, une responsabilité chacun :

- `core/` : chemins, config `.ini`, journal, hooking (IAT, vtables COM), infos système.
- `crash/` : filtre d'exceptions non gérées chaîné avec celui du jeu, minidumps, rapport.
- `framepacing/` : mesures de cadence, statistiques de `Sleep`, résolution du minuteur, limiteur.
- `graphics/` : `Direct3DCreate9` (IAT) -> `CreateDevice` -> `Present`/`Reset` (vtables). Journalise les
  paramètres de présentation et mesure la cadence, sans rien modifier pour l'instant.
- À venir selon l'analyse : chargeur (DLL proxy), `input/` (manette), `patches/` (fonctions internes,
  vérifiées par signature avant application).
- `DllMain` ne fait que des opérations sûres sous le verrou du chargeur ; ce qui charge des DLL est différé au
  premier `Direct3DCreate9` (`LateInit`).
- Chaque fonctionnalité doit pouvoir être désactivée dans le `.ini` (`dist/SHHFix.ini`).
- Toute nouvelle fonctionnalité doit être couverte par un test (`tests/unit` pour les fonctions pures ; un faux
  jeu `tests/fake_game` à recréer comme dans DPStabilityFix dès que le chargeur est choisi).
- Analyse de `SilentHill.exe` : `docs/analyse-shh-exe.md`. Désassembler dans le scratchpad
  (`dumpbin /disasm:nobytes`), jamais dans le dépôt, et consigner adresses et descriptions en distinguant
  **vérifié** de **hypothèse**.

## État

Premier jet : structure, modules génériques repris, documentation. **Jamais compilé ni lancé** (attente du feu
vert de l'utilisateur et de l'installation du jeu). Le chargeur du mod dans le jeu n'est pas choisi : la DLL se
compile sous le nom `SHHFix.dll` sans se charger toute seule. Points en attente : `docs/a-faire.md`.

## Commandes

- Compiler : `powershell -ExecutionPolicy Bypass -File tools\build.ps1` (preset `x86-release`).
- Tests unitaires : `build\x86-release\SHHFixUnitTests.exe` (code de sortie = nombre d'échecs).
- Les scripts `.ps1` doivent rester en UTF-8 **avec BOM** (sinon PowerShell 5.1 abîme les accents).
- CI GitHub Actions (`.github/workflows/build.yml`) : compilation et tests unitaires.

## Toolchain

- **Build Tools for Visual Studio 2022** : MSVC, SDK Windows, CMake et Ninja fournis par les Build Tools.
  Éditeur : VS Code.
- Cible **Win32 (x86)** (le jeu est supposé 32 bits, à confirmer) : compiler depuis un environnement
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

## Licence

- Projet sous **GPL-3.0-or-later** (`LICENSE`, copyright Cesar Schaal). Le paquet distribué doit contenir
  `LICENSE.txt`.
- Le correctif communautaire existant (Steam006, 2018) sert de **liste de ce qui est possible** : ne jamais
  copier ni redistribuer son binaire ou son code.
- Ne jamais intégrer de code ou de shader sans licence libre vérifiée.

## Sécurité des données du joueur et du jeu

- **Ne jamais modifier les fichiers du jeu** (exécutable, DLL, données, `Engine\vars_pc`...) ni les sauvegardes
  sans confirmation explicite de l'utilisateur. La lecture et l'analyse sont autorisées.
- Toute opération sur une sauvegarde doit d'abord en garder une copie.
- Le déploiement de la DLL dans le dossier du jeu se fait sur demande, jamais automatiquement.
- Ne jamais committer de fichiers du jeu (exécutable, données, sauvegardes, minidumps contenant de la
  mémoire du jeu) : seulement notre code et des signatures.

## Workflow avec Claude Code

- L'utilisateur a de solides bases en **C#**, pas en C++ : expliquer les concepts C++ et Win32 nouveaux
  avec des équivalents C# quand c'est pertinent (pointeurs de fonction / délégués, RAII / `IDisposable`,
  `HANDLE` / `SafeHandle`, etc.).
- Ne jamais lancer de build (`msbuild`, `cmake --build`...) ni le jeu de ta propre initiative :
  toujours demander confirmation avant.
- Claude ne peut pas jouer : les tests en jeu sont faits par l'utilisateur, qui remonte journaux et
  minidumps. Préciser à chaque itération exactement quoi tester et quoi rapporter.
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
