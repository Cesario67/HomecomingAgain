# Méthode de travail (sessions Claude Code)

Ce que chaque session applique à chaque modification, en plus de `CLAUDE.md` (qui reste la référence). Le but :
que deux sessions travaillent de la même façon, et sans se gêner.

## 1. Avant de commencer

- Lire `CLAUDE.md`, `docs/a-faire.md`, et `git status` : **d'autres sessions peuvent travailler en même temps dans le
  même dossier, sur la même branche `dev`**. Tout fichier modifié qu'on n'a pas touché soi-même appartient à une
  autre session : ne jamais le committer, le reformater, le revenir en arrière ni le supprimer.
- Travailler sur `dev` (ou une branche de fonctionnalité créée depuis `dev`, puis PR vers `dev`). Jamais sur `main`.

## 2. Rétro-ingénierie du moteur (`g_SilentHill.sgl`, Steam build 6.30, base 0x10000000)

- Désassemblage complet dans le dossier de travail de la session (scratchpad), jamais dans le dépôt :
  `dumpbin /disasm g_SilentHill.sgl > engine.asm` (environ 250 Mo). Chercher avec `grep -n "^  10XXXXXX:"`.
- Petits scripts PowerShell utiles (à recréer dans le scratchpad) : chaînes et références à une chaîne
  (offset fichier = VA - 0x10000000 pour les sections 1 à 3), appels directs `E8`/sauts `E9` vers une adresse.
- Les relais du moteur (`jmp` vers la vraie fonction, vers 0x10001000 à 0x1006xxxx) sont le meilleur point
  d'interception : un seul endroit à rediriger pour tous les appelants.
- Consigner chaque adresse dans un commentaire de code : ce que fait le code ciblé, comment il a été identifié,
  la date, la version. Écrire **vérifié** (désassemblage, journal, test en jeu) ou **hypothèse**, jamais une
  hypothèse présentée comme un fait.

### Démarche : mesurer avant de corriger

Chaque défaut se traite dans cet ordre : observer, mesurer, trouver le code, corriger, vérifier. Ne pas empiler les
suppositions : quand une piste ne marche pas, revenir à la mesure (exemples réels de cette branche ci-dessous).

1. **Reproduire et capturer** : captures d'écran du jeu (une par seconde pour une séquence, assemblées en planche),
   heures précises, journal du mod.
2. **Mesurer avec un diagnostic temporaire** plutôt que deviner :
   - journal ciblé, une ligne par changement (jamais à chaque image), marqué `DIAGNOSTIC TEMPORAIRE` dans le code
     et retiré une fois la cause trouvée (ex. : fichiers ouverts et octets lus au démarrage, pauses `SleepEx` par
     appelant, noms d'icônes demandés, entrées manette et clavier) ;
   - relevé d'image F11 (`[Debug] FrameCapture=1`, `graphics/FrameCapture.cpp`) : tous les appels Direct3D d'une
     image, dont `SetViewport` et `SetScissorRect`, et les shaders (`.bin`) ;
   - désassemblage des shaders relevés : `fxc /dumpbin fichier.bin` (Windows SDK) donne les registres
     (`mWorldViewProj` en c0..c3, etc.).
3. **Remonter au code** : chaîne -> références -> fonction ; relais `jmp` ; tables de données (`.data`, pointeurs
   vers des noms) ; tables d'aiguillage (`jmp [table+reg*4]`, lire les octets bruts) ; appelants (`grep "call  X"`).
   Vérifier qu'une table ou une fonction n'a pas d'autre utilisateur avant de la modifier.
4. **Corriger au plus près** de la cause, avec les octets vérifiés (section 3), et un réglage pour désactiver.
5. **Vérifier en jeu**, puis le noter dans le commit (« vérifié en jeu ») ou dire clairement ce qui ne l'est pas.

Leçons de cette branche :
- logos de démarrage : accélérer l'horloge du jeu n'a rien fait ; la mesure (fichiers lus, pauses par appelant) a
  montré une boucle à pas fixe, d'où le bon correctif (`compat/StartupLogos`) ;
- carte coupée : étendre la zone d'affichage agrandissait la carte ; il fallait aussi compenser la projection
  (`graphics/UiClip`) ;
- icônes : renvoyer un autre nom ne suffit pas quand le jeu cherche le nom dans une liste préparée au chargement ;
  remplacer l'image ou utiliser un nom déjà présent dans la liste (journal des noms) ;
- pauses intempestives : la première hypothèse (traduction manette vers clavier) était démentie par le journal ;
  le dire, et mesurer davantage (`input/InputLog`).

## 3. Écrire un correctif du moteur

Modèle : `src/input/ButtonIcons.cpp`, `src/patches/FrameRateCap.cpp`.

- Vérifier que le moteur est chargé à 0x10000000, sinon ne rien faire (et le journaliser).
- **Comparer les octets d'origine attendus avant toute écriture** (`memcmp`) : sur une autre version du jeu, le
  correctif ne s'applique pas, il ne casse rien. Écrire avec `hooking::WriteCode` (ou `WriteJump`).
- Remplaçantes de fonctions thiscall : `__fastcall` avec un paramètre `edx` inutilisé ; même nombre d'arguments
  pour que la pile soit rendue pareil.
- Hooks défensifs : aucune exception ne traverse le hook (`try/catch`, lectures de structures du moteur sous
  `__try/__except`), toujours appeler l'original si notre traitement échoue.
- Pas d'allocation ni d'écriture de journal dans les chemins appelés à chaque image (Present, boucle de jeu), sauf
  « une fois » ou lors d'un changement.
- Une ligne de journal à l'installation : appliqué, ou non appliqué et pourquoi.
- Chaque fonctionnalité a son réglage dans le `.ini` : `src/core/Config.h/.cpp`, `dist/HomecomingAgain.ini`
  (commenté), et si utile le launcher (`PatchSettings.cs`, `GraphicsViewModel.cs`, `MainWindow.axaml`,
  `Translations.cs` en français et en anglais, test dans `launcher/tests`).
- La logique testable va dans une fonction pure (fichier à part si besoin, ex. `src/crash/HangClock.cpp`), ajoutée
  à la cible `HomecomingAgainUnitTests` de `CMakeLists.txt`, avec un test dans `tests/unit/main.cpp`.

## 4. Compiler et tester

- Compiler : `powershell -ExecutionPolicy Bypass -File tools\build.ps1` (DLL, programme DLSS, launcher publié).
- Tests du mod : `build\x86-release\HomecomingAgainUnitTests.exe` (doit finir par « Tous les tests unitaires passent »).
- Tests du launcher : `dotnet build launcher\tests\HomecomingAgain.Launcher.Tests -c Release`, puis lancer
  directement `HomecomingAgain.Launcher.Tests.exe` dans son dossier `bin\Release\...` (`dotnet test` ne marche pas
  avec .NET 10 et Microsoft.Testing.Platform).
- **Prouver qu'un nouveau test détecte le défaut** : réintroduire le défaut un instant, compiler, voir le test
  échouer (« ECHEC »), remettre le code, recompiler, vérifier que tout passe et que `git diff` du fichier est propre.
- Launcher : vérifier le rendu à l'écran. Le lancer depuis `build\x86-release\package\HomecomingAgain.exe`, faire
  défiler par `WM_MOUSEWHEEL`, capturer par `PrintWindow` (drapeau 2) : `CopyFromScreen` capture la fenêtre de
  devant si le launcher n'a pas le focus.

## 5. Installer dans le jeu et faire tester

- **Jamais de compilation, de lancement ni de fermeture du jeu sans l'accord de l'utilisateur.** Ne jamais tuer le
  jeu : lui demander de le fermer.
- Installer seulement si le jeu est fermé (`Get-Process SilentHill`), puis vérifier la copie par empreinte :
  ```powershell
  $bin='E:\SteamLibrary\steamapps\common\Silent Hill Homecoming\Bin'
  Copy-Item build\x86-release\version.dll "$bin\version.dll" -Force
  (Get-FileHash build\x86-release\version.dll).Hash -eq (Get-FileHash "$bin\version.dll").Hash
  ```
- Lancer le jeu (sur demande) : `Start-Process "steam://rungameid/19000"`.
- **Mode test autonome** (seulement si l'utilisateur l'a accordé ; il peut le laisser actif jusqu'à « stop ») :
  - fermer le jeu par `WM_CLOSE` à sa fenêtre (`PostMessage`, fermeture propre en 1 s ; Alt+F4 simulé ne marche pas) ;
  - touches : `keybd_event` avec le code de touche Windows **et** le code clavier, appui de **80 ms** (300 ms fait
    descendre de deux crans ; `SendInput` en code clavier seul est ignoré), jeu au premier plan avant chaque touche
    (`SetForegroundWindow` encadré d'un appui Alt) ;
  - charger la partie : lancer, attendre 12 s (logos accélérés), Entrée, flèche bas, Entrée, 4 s, Entrée, 30 s ; le
    jeu arrive parfois sur Pause : Entrée sur « Reprendre » ;
  - vérifier chaque étape par une capture d'écran (`CopyFromScreen`, jeu au premier plan), et arrêter les appuis dès
    que l'écran visé est atteint (des Entrée en trop traversent les menus) ;
  - ne jamais valider une ligne de l'écran des commandes ni un réglage : regarder seulement.
- Si l'utilisateur est en train de jouer, demander avant de fermer le jeu, même en mode test.
- Donner à l'utilisateur des étapes de test **numérotées et précises** (où aller, quoi faire, quoi regarder), et ce
  qu'il doit répondre (« c'est fait »).
- Lire le journal : `Bin\HomecomingAgain\logs\HomecomingAgain_*.log` (le plus récent). Lignes utiles : `ERROR`,
  `WARN`, `BLOCAGE`, `Images :` (cadence toutes les 10 s), `Saccades`, `Mémoire`. Les fichiers de diagnostic sont
  dans `Bin\HomecomingAgain\crashdumps`.
- Rappel : le jeu se met en pause quand il n'est pas au premier plan (aucune image, sans être bloqué).
- Diagnostic temporaire (journal de valeurs du moteur) : le désactiver ou le retirer avant la PR.
- Ne jamais modifier les fichiers du jeu ni les sauvegardes sans accord explicite ; toujours une copie avant.

## 6. Committer

- **Nommer les fichiers un par un** : `git add chemin1 chemin2 ...`. Jamais `git add -A` ni `git commit -a` (une
  autre session peut avoir des fichiers modifiés dans le même dossier ; c'est déjà arrivé). Pour un fichier modifié
  par deux sessions, ne committer que ses propres lignes, ou s'entendre avec l'autre session.
- Vérifier avant de pousser : `git show --stat HEAD` ne doit lister que ses fichiers.
- Message en français, sans accents dans la première ligne (habitude du dépôt), sans tiret cadratin. Première
  ligne : `Domaine : ce qui change` (ex. `Manette : ...`, `Detecteur de blocage : ...`). Corps : pourquoi, et ce qui
  a été vérifié. Dernière ligne :
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`
- Pousser `dev` (ou la branche de fonctionnalité). Jamais de `push --force` sans accord ; si nécessaire, toujours
  `--force-with-lease=<branche>:<sha attendu>`.

## 7. Pull request et release

- PR vers `dev` (fonctionnalité) ou de `dev` vers `main` (version) : `gh pr create --repo Cesario67/HomecomingAgain`,
  description en français (résumé, comment, tests), terminée par
  `🤖 Generated with [Claude Code](https://claude.com/claude-code)`.
- `main` est protégée (vérification `build` obligatoire, branche à jour, relecture, même pour l'administrateur) :
  ne fusionner que sur demande explicite de l'utilisateur, une fois la vérification passée. Ne pas relancer la CI
  inutilement (chaque push en lance deux).
- Release : numéro dans `CMakeLists.txt` (`project(... VERSION x.y.z)`) et le `.csproj` du launcher (`<Version>`),
  recompiler, vérifier que `main` est identique au code compilé (`git diff --stat <commit compilé> origin/main` vide),
  puis `gh release create vX.Y.Z` avec `HomecomingAgain.exe` (contient tout) et un zip exe + licences.

## 8. Fichiers et encodage

- Les `.ps1` en UTF-8 **avec BOM** ; le reste en UTF-8 sans BOM (ne pas changer l'encodage d'un fichier existant).
- Le `.ini` du mod est lu par `GetPrivateProfileString` : pas de BOM.
- Écrire depuis PowerShell 5.1 avec `[IO.File]::WriteAllText(chemin, texte, (New-Object Text.UTF8Encoding $false))`
  (`Set-Content` écrit en ANSI).

## 9. Licences

- Rien de copié sans licence libre vérifiée (exception : SDK DLSS, voir `CLAUDE.md`). Jamais de code, binaire ni
  adresse tirés de Steam006 ou unknownproject. Le code repris d'un projet sous licence libre garde ses crédits et sa
  licence (dossier `third_party/<projet>/` avec le texte de la licence).
