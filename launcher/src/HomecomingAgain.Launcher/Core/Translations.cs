namespace HomecomingAgain.Launcher.Core;

/// <summary>
/// Table des textes du launcher : clé, puis (français, anglais). Les arguments de mise en forme sont
/// <c>{0}</c>, <c>{1}</c>... dans les deux langues. Un test vérifie que chaque clé employée dans les vues et
/// le code existe, a les deux langues et les mêmes arguments.
/// </summary>
public static class Translations
{
    public static readonly IReadOnlyDictionary<string, (string Fr, string En)> Table =
        new Dictionary<string, (string Fr, string En)>
        {
            // --- Fenêtre principale
            ["WindowTitle"] = ("Homecoming Again : Silent Hill Homecoming", "Homecoming Again: Silent Hill Homecoming"),
            ["GameFolder"] = ("Dossier du jeu :", "Game folder:"),
            ["Change"] = ("Changer...", "Change..."),
            ["ResetDefaults"] = ("Paramètres par défaut", "Restore defaults"),
            ["Save"] = ("Enregistrer", "Save"),
            ["Play"] = ("▶  Jouer", "▶  Play"),
            ["TabGraphics"] = ("Graphismes", "Graphics"),
            ["TabPerformance"] = ("Performances et stabilité", "Performance and stability"),
            ["TabInstall"] = ("Installation", "Installation"),
            ["Yes"] = ("Oui", "Yes"),
            ["No"] = ("Non", "No"),
            ["FolderPickerTitle"] = ("Dossier de Silent Hill Homecoming (celui qui contient Bin et Engine)",
                                     "Silent Hill Homecoming folder (the one that contains Bin and Engine)"),

            // --- Messages de la barre d'état
            ["GameNotFound"] = ("Jeu introuvable : choisissez le dossier de Silent Hill Homecoming.",
                                "Game not found: choose the Silent Hill Homecoming folder."),
            ["ExeNotFoundIn"] = ("Bin\\SilentHill.exe introuvable dans {0}.", "Bin\\SilentHill.exe not found in {0}."),
            ["LaunchedBySteam"] = ("Lancé par Steam : « Jouer » démarre le jeu.",
                                   "Started by Steam: “Play” starts the game."),
            ["ErrorPrefix"] = ("Erreur : {0}", "Error: {0}"),
            ["SettingsSaved"] = ("Réglages enregistrés : ils s'appliquent au prochain lancement du jeu.",
                                 "Settings saved: they apply the next time the game starts."),
            ["SettingsSavedModOnly"] = ("Réglages du mod enregistrés. Lancez le jeu une première fois pour qu'il crée ses " +
                                        "propres réglages (affichage, qualité), puis enregistrez de nouveau.",
                                        "Mod settings saved. Start the game once so that it creates its own settings " +
                                        "(display, quality), then save again."),
            ["ResetConfirm"] = ("Revenir aux paramètres par défaut ?\n\nToutes les options reprennent leur valeur d'origine. " +
                                "Le mode d'affichage et la résolution sont conservés.",
                                "Restore the default settings?\n\nAll options go back to their original value. The display " +
                                "mode and resolution are kept."),
            ["ResetDone"] = ("Paramètres par défaut rétablis.", "Default settings restored."),
            ["GameAlreadyRunning"] = ("Le jeu est déjà lancé.", "The game is already running."),
            ["GameLaunched"] = ("Jeu lancé.", "Game started."),
            ["GameRunningClose"] = ("Le jeu est lancé : fermez-le d'abord (il réécrit ses réglages en quittant).",
                                    "The game is running: close it first (it rewrites its settings when it quits)."),
            ["SteamCopied"] = ("Option de lancement copiée.", "Launch option copied."),

            // --- Graphismes
            ["SecDisplay"] = ("Affichage", "Display"),
            ["Mode"] = ("Mode", "Mode"),
            ["Resolution"] = ("Résolution", "Resolution"),
            ["ModeBorderless"] = ("Plein écran sans bordure (recommandé)", "Borderless fullscreen (recommended)"),
            ["ModeFullscreen"] = ("Plein écran exclusif", "Exclusive fullscreen"),
            ["ModeWindowed"] = ("Fenêtré", "Windowed"),
            ["DisplayHint"] = ("Le jeu d'origine démarre en 800 × 600. En sans bordure, l'image est étirée à la taille de " +
                               "l'écran si la résolution choisie est plus petite.",
                               "The original game starts at 800 × 600. In borderless mode, the image is stretched to the " +
                               "screen size if the chosen resolution is smaller."),
            ["FullscreenWarning"] = ("Plein écran exclusif : un Alt-Tab peut faire planter le jeu (défaut du jeu d'origine, " +
                                     "pas encore corrigé). Le mode sans bordure n'a pas ce problème.",
                                     "Exclusive fullscreen: Alt-Tab can crash the game (an original game bug, not fixed " +
                                     "yet). Borderless mode does not have this problem."),
            ["SecImage"] = ("Qualité d'image", "Image quality"),
            ["Smaa"] = ("Anticrénelage SMAA", "SMAA anti-aliasing"),
            ["SmaaHint"] = ("Lisse les contours en escalier. Appliqué à l'image finale : les textes de l'interface sont " +
                            "légèrement adoucis aussi.",
                            "Smooths jagged edges. Applied to the final image: interface text is slightly softened too."),
            ["Dlss"] = ("DLSS (DLAA, expérimental)", "DLSS (DLAA, experimental)"),
            ["DlssHint"] = ("Anticrénelage par l'IA de NVIDIA, plus stable et plus net que le SMAA, qu'il remplace quand il est " +
                            "actif. Carte NVIDIA RTX uniquement ; installe aussi DXVK (Direct3D 9 sur Vulkan). Sans carte " +
                            "compatible, le jeu garde son image normale.",
                            "NVIDIA AI anti-aliasing, more stable and sharper than SMAA, which it replaces while active. " +
                            "NVIDIA RTX cards only; also installs DXVK (Direct3D 9 on Vulkan). Without a compatible card, " +
                            "the game keeps its normal image."),
            ["DlssCredit"] = ("NVIDIA DLSS : licence NVIDIA RTX SDKs. DXVK : licence zlib. Ce projet n'est ni soutenu ni " +
                              "approuvé par NVIDIA.",
                              "NVIDIA DLSS: NVIDIA RTX SDKs License. DXVK: zlib license. This project is not sponsored or " +
                              "endorsed by NVIDIA."),
            ["DlssPackageMissing"] = ("Fichiers du DLSS absents de ce launcher.", "DLSS files are missing from this launcher."),
            ["DlssInstalled"] = ("DLSS et DXVK installés.", "DLSS and DXVK installed."),
            ["DlssRemoved"] = ("DXVK retiré : le jeu revient à Direct3D 9.", "DXVK removed: the game is back on Direct3D 9."),
            ["DlssForeignKept"] = ("Une d3d9.dll d'un autre mod a été mise de côté (d3d9.dll.avant-hca).",
                                   "A d3d9.dll from another mod was set aside (d3d9.dll.avant-hca)."),
            ["DlssForeignRestored"] = ("La d3d9.dll de l'autre mod a été remise.", "The other mod's d3d9.dll was restored."),
            ["Anisotropy"] = ("Filtrage anisotrope", "Anisotropic filtering"),
            ["AnisoOff"] = ("Désactivé (jeu d'origine)", "Off (original game)"),
            ["AnisotropyHint"] = ("Textures nettes au sol et vues de biais. Impact négligeable sur une carte graphique récente.",
                                  "Sharp textures on the ground and at an angle. Negligible cost on a recent graphics card."),
            ["SecLighting"] = ("Éclairage", "Lighting"),
            ["Ssao"] = ("Occlusion ambiante (SSAO)", "Ambient occlusion (SSAO)"),
            ["SsaoStrength"] = ("Intensité du SSAO", "SSAO strength"),
            ["SsaoLight"] = ("Légère", "Light"),
            ["SsaoNormal"] = ("Normale", "Normal"),
            ["SsaoStrong"] = ("Marquée", "Strong"),
            ["SsaoHint"] = ("Assombrit légèrement les coins et le contact entre les objets. Appliqué avant l'interface, qui " +
                            "reste intacte.",
                            "Slightly darkens corners and contact between objects. Applied before the interface, which " +
                            "stays untouched."),
            ["ShadowResolution"] = ("Résolution des ombres", "Shadow resolution"),
            ["ShadowOriginal"] = ("2048 × 2048 (jeu d'origine)", "2048 × 2048 (original game)"),
            ["ShadowResolutionHint"] = ("Ombres plus nettes. Au-delà de 4096, des défauts apparaissent sur les vêtements (constaté en 8192).",
                                        "Sharper shadows. Above 4096, artifacts appear on clothing (seen at 8192)."),            ["FilmGrain"] = ("Grain de pellicule", "Film grain"),
            ["GrainOff"] = ("Désactivé", "Off"),
            ["GrainHalf"] = ("Réduit de moitié", "Halved"),
            ["GrainOriginal"] = ("Jeu d'origine", "Original game"),
            ["FilmGrainHint"] = ("Grain ajouté par le jeu à toute l'image. La teinte et le contraste ne changent pas.",
                                 "Grain the game adds over the whole picture. Tint and contrast are unchanged."),            ["SecGameQuality"] = ("Qualité du jeu", "Game quality"),
            ["HighQualityShaders"] = ("Shaders en haute qualité", "High quality shaders"),
            ["FullResolutionTextures"] = ("Textures en pleine résolution", "Full resolution textures"),
            ["GameQualityHint"] = ("Réglages du jeu lui-même, en basse qualité par défaut (shaders simplifiés, textures " +
                                   "limitées à 512). Leur effet exact n'a pas encore été vérifié. Une copie de secours de " +
                                   "vars_pc.cfg est faite à la première modification.",
                                   "Settings of the game itself, low quality by default (simplified shaders, textures limited " +
                                   "to 512). Their exact effect has not been verified yet. A backup of vars_pc.cfg is made " +
                                   "on the first change."),
            ["SmaaCredit"] = ("Uses SMAA. Copyright (C) 2011 by Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando " +
                              "Navarro and Diego Gutierrez.",
                              "Uses SMAA. Copyright (C) 2011 by Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando " +
                              "Navarro and Diego Gutierrez."),

            // --- Performances et stabilité
            ["SecFrameRate"] = ("Cadence", "Frame rate"),
            ["TargetFps"] = ("Images par seconde", "Frames per second"),
            ["Fps30Original"] = ("30 (jeu d'origine)", "30 (original game)"),
            ["TargetFpsHint"] = ("Le jeu d'origine est bloqué à 30 i/s par une valeur codée en dur, que le mod remplace. " +
                                 "Le réglage fpsLimit du jeu n'a aucun effet sur ce plafond.",
                                 "The original game is locked to 30 fps by a hard-coded value, which the mod replaces. The " +
                                 "game's fpsLimit setting has no effect on this cap."),
            ["HighFpsWarning"] = ("Au-delà de 60 i/s, le comportement du jeu (animations, physique, QTE) n'a pas été vérifié.",
                                  "Above 60 fps, the game's behaviour (animations, physics, QTEs) has not been verified."),
            ["PreciseTimer"] = ("Minuteur Windows précis (1 ms)", "Precise Windows timer (1 ms)"),
            ["PreciseTimerHint"] = ("Attentes plus régulières entre les images.", "More regular waits between frames."),
            ["SecCompatibility"] = ("Compatibilité", "Compatibility"),
            ["WindowThreadLock"] = ("Fenêtre réactive pendant les chargements (overlay Steam)",
                                    "Responsive window during loading (Steam overlay)"),
            ["D3DMultithreaded"] = ("Direct3D protégé contre les accès simultanés", "Direct3D protected against concurrent access"),
            ["CompatibilityHint"] = ("Corrigent les blocages au chargement en fenêtré ou sans bordure, surtout avec l'overlay " +
                                     "Steam. À laisser activés.",
                                     "Fix freezes during loading in windowed or borderless mode, especially with the Steam " +
                                     "overlay. Keep them enabled."),
            ["SecDiagnostics"] = ("Diagnostic", "Diagnostics"),
            ["CrashDumps"] = ("Fichier de diagnostic en cas de plantage", "Diagnostic file on crash"),
            ["FullDumps"] = ("Diagnostic complet (fichier jusqu'à 2 Go)", "Full diagnostics (file up to 2 GB)"),
            ["HangDetection"] = ("Diagnostic en cas de blocage (3 s sans image)", "Diagnostics on freeze (3 s without a frame)"),
            ["FrameStats"] = ("Mesures de cadence dans le journal", "Frame rate measurements in the log"),
            ["DiagnosticsHint"] = ("Journaux et diagnostics sont dans Bin\\HomecomingAgain : joignez-les à un rapport de bug.",
                                   "Logs and diagnostics are in Bin\\HomecomingAgain: attach them to a bug report."),

            // --- Installation
            ["SecStatus"] = ("État", "Status"),
            ["ModInstalledState"] = ("Homecoming Again {0} installé.", "Homecoming Again {0} installed."),
            ["ModForeignDll"] = ("Une autre version.dll est présente dans Bin (autre mod ?) : elle sera mise de côté à " +
                                 "l'installation.",
                                 "Another version.dll is present in Bin (another mod?): it will be set aside on install."),
            ["ModNotInstalled"] = ("Homecoming Again n'est pas installé.", "Homecoming Again is not installed."),
            ["GameSettingsFound"] = ("Réglages du jeu trouvés (Engine\\vars_pc.cfg).", "Game settings found (Engine\\vars_pc.cfg)."),
            ["GameSettingsMissing"] = ("Réglages du jeu absents : lancez le jeu une fois pour qu'il les crée.",
                                       "Game settings missing: start the game once so that it creates them."),
            ["SecInstallation"] = ("Installation", "Installation"),
            ["InstallButton"] = ("Installer / mettre à jour", "Install / update"),
            ["InstallButtonVersion"] = ("Installer / mettre à jour (version {0})", "Install / update (version {0})"),
            ["Uninstall"] = ("Désinstaller", "Uninstall"),
            ["InstallHint"] = ("Le mod tient dans Bin\\version.dll et Bin\\HomecomingAgain.ini. Seul l'en-tête de SilentHill.exe est " +
                               "modifié (patch 4 Go), après copie de l'original, remis à la désinstallation. Vos réglages " +
                               "existants sont conservés.",
                               "The mod lives in Bin\\version.dll and Bin\\HomecomingAgain.ini. Only the SilentHill.exe header is " +
                               "modified (4 GB patch), after copying the original, which is restored on uninstall. Your " +
                               "existing settings are kept."),
            ["UninstallConfirm"] = ("Désinstaller Homecoming Again ?\n\nversion.dll est retirée ; les réglages et les journaux sont " +
                                    "conservés.",
                                    "Uninstall Homecoming Again?\n\nversion.dll is removed; settings and logs are kept."),
            ["SecSteam"] = ("Lancement par Steam", "Starting from Steam"),
            ["SteamHint"] = ("Pour ouvrir ce launcher depuis Steam : Propriétés du jeu, Options de lancement, puis collez " +
                             "cette ligne.",
                             "To open this launcher from Steam: game Properties, Launch options, then paste this line."),
            ["Copy"] = ("Copier", "Copy"),
            ["SecDiagnosticTools"] = ("Diagnostic", "Diagnostics"),
            ["OpenLatestLog"] = ("Ouvrir le dernier journal", "Open the latest log"),
            ["OpenModFolder"] = ("Ouvrir le dossier du mod", "Open the mod folder"),
            ["CrashReportHint"] = ("En cas de problème, envoyez le dernier journal et, s'il y en a, le fichier de " +
                                   "crashdumps correspondant.",
                                   "If something goes wrong, send the latest log and, if any, the matching crashdumps file."),
            ["NoLogYet"] = ("Pas encore de journal : lancez le jeu une fois.", "No log yet: start the game once."),
            ["LogOpened"] = ("Journal ouvert : {0}", "Log opened: {0}"),

            // --- Installation (compte rendu)
            ["InstallInvalidGame"] = ("Ce dossier ne contient pas Bin\\SilentHill.exe.", "This folder does not contain Bin\\SilentHill.exe."),
            ["InstallDllMissing"] = ("version.dll du mod introuvable dans le paquet.", "The mod's version.dll is missing from the package."),
            ["InstallDllKept"] = ("Une autre version.dll a été mise de côté (version.dll.avant-hca).",
                                  "Another version.dll was set aside (version.dll.avant-hca)."),
            ["InstallModInstalled"] = ("Homecoming Again {0} installé.", "Homecoming Again {0} installed."),
            ["InstallFileKept"] = ("{0} existant conservé.", "Existing {0} kept."),
            ["InstallFileInstalled"] = ("{0} installé.", "{0} installed."),
            ["InstallLauncherCopied"] = ("Launcher copié dans Bin.", "Launcher copied to Bin."),
            ["UninstallDllRemoved"] = ("version.dll retirée.", "version.dll removed."),
            ["UninstallDllRestored"] = ("version.dll d'origine remise en place.", "Original version.dll restored."),
            ["UninstallKept"] = ("Réglages et journaux conservés.", "Settings and logs kept."),
            ["UninstallExeRestored"] = ("SilentHill.exe d'origine restauré.", "Original SilentHill.exe restored."),
            ["InstallUnknownVersion"] = ("Version du jeu différente de celle analysée : certains correctifs peuvent " +
                                         "être ignorés (voir le journal).",
                                         "Game version differs from the analysed one: some fixes may be skipped (see the log)."),
            ["InstallLaaApplied"] = ("Patch 4 Go appliqué (original conservé : SilentHill.exe.hca-original).",
                                     "4 GB patch applied (original kept: SilentHill.exe.hca-original)."),
            ["InstallLaaAlready"] = ("Patch 4 Go déjà présent.", "4 GB patch already present."),

            // --- Patch 4 Go (en-tête de SilentHill.exe)
            ["ExeUnreadable"] = ("SilentHill.exe illisible.", "SilentHill.exe is unreadable."),
            ["ExeKnownVersion"] = ("Jeu : version Steam analysée (build 6.30).", "Game: analysed Steam version (build 6.30)."),
            ["ExeUnknownVersion"] = ("Jeu : version inconnue (horodatage {0}).", "Game: unknown version (timestamp {0})."),
            ["LaaApplied"] = ("Mémoire : 4 Go disponibles (patch appliqué).", "Memory: 4 GB available (patch applied)."),
            ["LaaMissing"] = ("Mémoire : limitée à 2 Go (patch 4 Go appliqué à l'installation).",
                              "Memory: limited to 2 GB (4 GB patch applied on install)."),
            ["ExeInvalidHeader"] = ("En-tête de SilentHill.exe invalide.", "Invalid SilentHill.exe header."),
            ["PatchVerifyFailed"] = ("Le patch 4 Go n'a pas pu être vérifié après écriture.",
                                     "The 4 GB patch could not be verified after writing."),
            ["ExeBackupMissing"] = ("Aucune copie d'origine de SilentHill.exe (SilentHill.exe.hca-original).",
                                    "No original copy of SilentHill.exe (SilentHill.exe.hca-original)."),

            // --- Ligne de commande
            ["CliUsage"] = ("Usage : HomecomingAgain.exe install \"<dossier du jeu>\"", "Usage: HomecomingAgain.exe install \"<game folder>\""),
            ["CliNotGame"] = ("{0} n'est pas un dossier de Silent Hill Homecoming.", "{0} is not a Silent Hill Homecoming folder."),
            ["CliFailure"] = ("Échec : {0}", "Failed: {0}"),
        };
}
