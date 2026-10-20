namespace HomecomingAgain.Launcher.Core;

public enum DisplayMode
{
    Borderless,
    Fullscreen,
    Windowed,
}

/// <summary>
/// Tous les réglages présentés par le launcher, avec les valeurs par défaut de la DLL. Ils sont répartis entre
/// deux fichiers : <c>Bin\HomecomingAgain.ini</c> (le mod) et <c>Engine\vars_pc.cfg</c> (le jeu : affichage et qualité).
/// </summary>
public sealed record PatchSettings
{
    // --- Affichage (vars_pc.cfg, plus [Window] Borderless pour le mode sans bordure)
    public DisplayMode DisplayMode { get; init; } = DisplayMode.Borderless;
    public int Width { get; init; } = 1920;
    public int Height { get; init; } = 1080;

    // --- Qualité (vars_pc.cfg). Le jeu se crée avec LowQualityShaders=true et AutoDownResTextures=512.
    public bool HighQualityShaders { get; init; }
    public bool FullResolutionTextures { get; init; }

    // --- Graphismes du mod ([Graphics])
    public bool Smaa { get; init; } = true;
    public int Anisotropy { get; init; } = 16;
    public bool Ssao { get; init; } = true;
    public int SsaoStrength { get; init; } = 100;
    public int ShadowResolution { get; init; } = 8192;
    public int FilmGrain { get; init; } = 100;
    // Carte et interface étendues au-delà de 1280x720 ; perspective de la vue d'inspection des objets rétablie.
    public bool FullScreenUiClip { get; init; } = true;
    public bool ObjectViewPerspective { get; init; } = true;
    // Onglet Avancé : rayon, début et fin du fondu de la SSAO (unités du jeu), D3D9On12 (expérimental).
    public double SsaoRadius { get; init; } = 1.5;
    public int SsaoFadeStart { get; init; } = 150;
    public int SsaoFadeEnd { get; init; } = 450;
    public bool D3D9On12 { get; init; }

    // --- DLSS ([DLSS]) : ShareFrames, Jitter et Apply ensemble. Désactivé par défaut (NVIDIA RTX et DXVK nécessaires).
    public bool Dlss { get; init; }

    // --- Manette ([Input]) : icônes de boutons, 0 = auto (Xbox 360 si une manette est branchée), 1 = clavier,
    // 2 = Xbox 360, 3 = PS3.
    public int ButtonIcons { get; init; }
    // Invite à marteler (QTE) aux couleurs de la manette ([Input] AnimatedIcons) ; croix et stick gauche = flèches dans
    // les menus ([Input] MenuPad).
    public bool AnimatedIcons { get; init; } = true;
    public bool MenuPad { get; init; }

    // --- Caméra à la manette ([Camera] CameraFix, plus les commandes du stick droit, voir ControllerBinds).
    public bool CameraFix { get; init; }
    // Manette PlayStation sans Steam Input ([Input] PlayStationPad).
    public bool PlayStationPad { get; init; } = true;
    // Caméra orbitale ([Camera] Mode=Orbit, sinon Follow), effective seulement avec CameraFix.
    public bool CameraOrbit { get; init; }
    // Orbite suspendue tant que la gâchette gauche (L2) est tenue ([Camera] OrbitGuardFollow).
    public bool OrbitGuardFollow { get; init; } = true;
    // Vitesses de la caméra ([Camera]) : de 0,25 à 3, 1 = calibrage d'origine.
    public double CameraHorizontalSpeed { get; init; } = 1.0;
    public double CameraVerticalSpeed { get; init; } = 1.0;
    public double CameraAimHorizontalSpeed { get; init; } = 1.0;
    public double CameraAimVerticalSpeed { get; init; } = 1.0;

    // --- Son ([Audio]) : correction des sons d'un seul côté, canaux de sortie (0 = ceux de Windows, 6 = 5.1, 8 = 7.1)
    // et ordre des canaux du moteur (onglet Avancé).
    public bool AudioChannelFix { get; init; } = true;
    public int AudioChannels { get; init; }
    public string AudioChannelOrder { get; init; } = "2,4,1,3,5,0";

    // --- Cadence ([Frames])
    public int TargetFps { get; init; } = 60;
    public bool PreciseTimer { get; init; } = true;
    // Limiteur d'images (0 = aucun).
    public int FrameLimitFps { get; init; }
    public int ReportIntervalSeconds { get; init; } = 10;

    // --- Démarrage ([Movies]) : logos accélérés, touche pour passer les vidéos (code Windows, 0 = aucune).
    public bool SkipLogos { get; init; } = true;
    public int SkipKey { get; init; } = 46;

    // --- Triches ([Cheats])
    public bool God { get; init; }
    public bool UnlimitedAmmo { get; init; }

    // --- Compatibilité ([Compat])
    public bool WindowThreadLock { get; init; } = true;
    public bool D3DMultithreaded { get; init; } = true;

    // --- Diagnostic ([Crash], [Frames])
    public bool CrashDumps { get; init; } = true;
    public bool FullMemoryDumps { get; init; }
    public bool HangDetection { get; init; } = true;
    public bool FrameStats { get; init; } = true;
    public int DumpKeepCount { get; init; } = 10;
    public int LogKeepCount { get; init; } = 30;

    /// <summary>Valeur par défaut d'AutoDownResTextures, telle que le jeu l'écrit à son premier lancement.</summary>
    public const int DefaultTextureLimit = 512;

    public static PatchSettings Read(IniDocument mod, KeyValueDocument game)
    {
        PatchSettings defaults = new();
        bool fullScreen = game.GetBool("FullScreen", true);
        bool borderless = mod.GetBool("Window", "Borderless", true);
        return new PatchSettings
        {
            DisplayMode = fullScreen ? DisplayMode.Fullscreen : borderless ? DisplayMode.Borderless : DisplayMode.Windowed,
            Width = game.GetInt("ScreenResWidth", defaults.Width),
            Height = game.GetInt("ScreenResHeight", defaults.Height),
            HighQualityShaders = !game.GetBool("LowQualityShaders", true),
            FullResolutionTextures = game.GetInt("AutoDownResTextures", DefaultTextureLimit) == 0,
            Smaa = mod.GetBool("Graphics", "SMAA", defaults.Smaa),
            Anisotropy = mod.GetInt("Graphics", "Anisotropy", defaults.Anisotropy),
            Ssao = mod.GetBool("Graphics", "SSAO", defaults.Ssao),
            SsaoStrength = mod.GetInt("Graphics", "SsaoStrength", defaults.SsaoStrength),
            ShadowResolution = mod.GetInt("Graphics", "ShadowResolution", defaults.ShadowResolution),
            FilmGrain = mod.GetInt("Graphics", "FilmGrain", defaults.FilmGrain),
            FullScreenUiClip = mod.GetBool("Graphics", "FullScreenUiClip", defaults.FullScreenUiClip),
            ObjectViewPerspective = mod.GetBool("Graphics", "ObjectViewPerspective", defaults.ObjectViewPerspective),
            SsaoRadius = mod.GetDouble("Graphics", "SsaoRadius", defaults.SsaoRadius),
            SsaoFadeStart = mod.GetInt("Graphics", "SsaoFadeStart", defaults.SsaoFadeStart),
            SsaoFadeEnd = mod.GetInt("Graphics", "SsaoFadeEnd", defaults.SsaoFadeEnd),
            D3D9On12 = mod.GetBool("Graphics", "D3D9On12", defaults.D3D9On12),
            Dlss = mod.GetBool("DLSS", "ShareFrames", false) && mod.GetBool("DLSS", "Apply", false),
            ButtonIcons = Math.Clamp(mod.GetInt("Input", "ButtonIcons", 0), 0, 3),
            AnimatedIcons = mod.GetBool("Input", "AnimatedIcons", defaults.AnimatedIcons),
            MenuPad = mod.GetBool("Input", "MenuPad", defaults.MenuPad),
            CameraFix = mod.GetBool("Camera", "CameraFix", defaults.CameraFix),
            OrbitGuardFollow = mod.GetBool("Camera", "OrbitGuardFollow", defaults.OrbitGuardFollow),
            CameraHorizontalSpeed = ClampSpeed(mod.GetDouble("Camera", "HorizontalSpeed", defaults.CameraHorizontalSpeed)),
            CameraVerticalSpeed = ClampSpeed(mod.GetDouble("Camera", "VerticalSpeed", defaults.CameraVerticalSpeed)),
            CameraAimHorizontalSpeed = ClampSpeed(mod.GetDouble("Camera", "AimHorizontalSpeed", defaults.CameraAimHorizontalSpeed)),
            CameraAimVerticalSpeed = ClampSpeed(mod.GetDouble("Camera", "AimVerticalSpeed", defaults.CameraAimVerticalSpeed)),
            AudioChannelFix = mod.GetBool("Audio", "ChannelFix", defaults.AudioChannelFix),
            AudioChannels = mod.GetInt("Audio", "Channels", defaults.AudioChannels),
            AudioChannelOrder = mod.Get("Audio", "ChannelOrder") is { } order && IsValidChannelOrder(order)
                ? order.Trim() : defaults.AudioChannelOrder,
            PlayStationPad = mod.GetBool("Input", "PlayStationPad", defaults.PlayStationPad),
            // Mêmes règles que la DLL (patches/CameraTuning.cpp) : toute autre valeur = défaut.
            CameraOrbit = string.Equals(mod.Get("Camera", "Mode"), "Orbit", StringComparison.OrdinalIgnoreCase),
            TargetFps = mod.GetInt("Frames", "TargetFps", defaults.TargetFps),
            PreciseTimer = mod.GetInt("Frames", "TimerResolutionMs", 1) > 0,
            FrameLimitFps = mod.GetInt("Frames", "FrameLimitFps", defaults.FrameLimitFps),
            ReportIntervalSeconds = mod.GetInt("Frames", "ReportIntervalSeconds", defaults.ReportIntervalSeconds),
            SkipLogos = mod.GetBool("Movies", "SkipLogos", defaults.SkipLogos),
            SkipKey = mod.GetInt("Movies", "SkipKey", defaults.SkipKey),
            God = mod.GetBool("Cheats", "God", defaults.God),
            UnlimitedAmmo = mod.GetBool("Cheats", "UnlimitedAmmo", defaults.UnlimitedAmmo),
            WindowThreadLock = mod.GetBool("Compat", "WindowThreadLock", defaults.WindowThreadLock),
            D3DMultithreaded = mod.GetBool("Compat", "D3DMultithreaded", defaults.D3DMultithreaded),
            CrashDumps = mod.GetBool("Crash", "CrashDumps", defaults.CrashDumps),
            FullMemoryDumps = mod.GetBool("Crash", "FullMemoryDumps", defaults.FullMemoryDumps),
            HangDetection = mod.GetInt("Crash", "HangSeconds", 3) > 0,
            FrameStats = mod.GetBool("Frames", "FrameStats", defaults.FrameStats),
            DumpKeepCount = mod.GetInt("Crash", "DumpKeepCount", defaults.DumpKeepCount),
            LogKeepCount = mod.GetInt("Debug", "LogKeepCount", defaults.LogKeepCount),
        };
    }

    /// <summary>Vitesses de caméra acceptées par la DLL (patches/CameraTuning.cpp) : de 0,25 à 3.</summary>
    public static double ClampSpeed(double speed) => Math.Clamp(speed, 0.25, 3.0);

    /// <summary>
    /// Ordre des canaux du moteur, comme l'exige la DLL (audio/AudioProbe.cpp) : six numéros de 0 à 5, tous différents,
    /// séparés par des virgules. Toute autre valeur est ignorée par la DLL, qui garde l'ordre par défaut.
    /// </summary>
    public static bool IsValidChannelOrder(string text)
    {
        string[] parts = text.Split(',', StringSplitOptions.TrimEntries);
        if (parts.Length != 6)
        {
            return false;
        }
        HashSet<int> seen = [];
        foreach (string part in parts)
        {
            if (!int.TryParse(part, out int channel) || channel is < 0 or > 5 || !seen.Add(channel))
            {
                return false;
            }
        }
        return true;
    }

    public void WriteMod(IniDocument mod)
    {
        mod.SetBool("Window", "Borderless", DisplayMode == DisplayMode.Borderless);
        mod.SetBool("Graphics", "SMAA", Smaa);
        mod.SetInt("Graphics", "Anisotropy", Math.Clamp(Anisotropy, 0, 16));
        mod.SetBool("Graphics", "SSAO", Ssao);
        mod.SetInt("Graphics", "SsaoStrength", Math.Clamp(SsaoStrength, 10, 400));
        mod.SetInt("Graphics", "ShadowResolution", ShadowResolution is 4096 or 8192 ? ShadowResolution : 2048);
        mod.SetInt("Graphics", "FilmGrain", Math.Clamp(FilmGrain, 0, 100));
        mod.SetBool("Graphics", "FullScreenUiClip", FullScreenUiClip);
        mod.SetBool("Graphics", "ObjectViewPerspective", ObjectViewPerspective);
        mod.SetDouble("Graphics", "SsaoRadius", Math.Clamp(SsaoRadius, 0.05, 100.0));
        int fadeStart = Math.Clamp(SsaoFadeStart, 0, 100000);
        mod.SetInt("Graphics", "SsaoFadeStart", fadeStart);
        mod.SetInt("Graphics", "SsaoFadeEnd", Math.Clamp(SsaoFadeEnd, fadeStart + 1, 100000));
        mod.SetBool("Graphics", "D3D9On12", D3D9On12);
        mod.SetBool("DLSS", "ShareFrames", Dlss);
        mod.SetInt("DLSS", "Jitter", Dlss ? 1 : 0);
        mod.SetBool("DLSS", "Apply", Dlss);
        mod.SetInt("Input", "ButtonIcons", Math.Clamp(ButtonIcons, 0, 3));
        mod.SetBool("Input", "AnimatedIcons", AnimatedIcons);
        mod.SetBool("Input", "MenuPad", MenuPad);
        mod.SetBool("Camera", "CameraFix", CameraFix);
        mod.SetBool("Input", "PlayStationPad", PlayStationPad);
        mod.Set("Camera", "Mode", CameraOrbit ? "Orbit" : "Follow");
        mod.SetBool("Camera", "OrbitGuardFollow", OrbitGuardFollow);
        mod.SetDouble("Camera", "HorizontalSpeed", ClampSpeed(CameraHorizontalSpeed));
        mod.SetDouble("Camera", "VerticalSpeed", ClampSpeed(CameraVerticalSpeed));
        mod.SetDouble("Camera", "AimHorizontalSpeed", ClampSpeed(CameraAimHorizontalSpeed));
        mod.SetDouble("Camera", "AimVerticalSpeed", ClampSpeed(CameraAimVerticalSpeed));
        mod.SetBool("Audio", "ChannelFix", AudioChannelFix);
        mod.SetInt("Audio", "Channels", AudioChannels is 6 or 8 ? AudioChannels : 0);
        mod.Set("Audio", "ChannelOrder", IsValidChannelOrder(AudioChannelOrder) ? AudioChannelOrder.Trim() : new PatchSettings().AudioChannelOrder);
        mod.SetInt("Frames", "TargetFps", Math.Clamp(TargetFps, 0, 1000));
        mod.SetInt("Frames", "TimerResolutionMs", PreciseTimer ? 1 : 0);
        mod.SetInt("Frames", "FrameLimitFps", Math.Clamp(FrameLimitFps, 0, 1000));
        mod.SetInt("Frames", "ReportIntervalSeconds", Math.Clamp(ReportIntervalSeconds, 1, 3600));
        mod.SetBool("Movies", "SkipLogos", SkipLogos);
        mod.SetInt("Movies", "SkipKey", Math.Clamp(SkipKey, 0, 254));
        mod.SetBool("Cheats", "God", God);
        mod.SetBool("Cheats", "UnlimitedAmmo", UnlimitedAmmo);
        mod.SetBool("Compat", "WindowThreadLock", WindowThreadLock);
        mod.SetBool("Compat", "D3DMultithreaded", D3DMultithreaded);
        mod.SetBool("Crash", "CrashDumps", CrashDumps);
        mod.SetBool("Crash", "FullMemoryDumps", FullMemoryDumps);
        mod.SetInt("Crash", "HangSeconds", HangDetection ? 3 : 0);
        mod.SetBool("Frames", "FrameStats", FrameStats);
        mod.SetInt("Crash", "DumpKeepCount", Math.Clamp(DumpKeepCount, 1, 100));
        mod.SetInt("Debug", "LogKeepCount", Math.Clamp(LogKeepCount, 1, 500));
    }

    /// <summary>
    /// Réglages du jeu. Le plein écran exclusif ne passe que par FullScreen : le mode sans bordure est un
    /// fenêtré que la DLL étend à tout l'écran.
    /// </summary>
    public void WriteGame(KeyValueDocument game)
    {
        game.SetBool("FullScreen", DisplayMode == DisplayMode.Fullscreen);
        game.SetInt("ScreenResWidth", Width);
        game.SetInt("ScreenResHeight", Height);
        game.SetBool("LowQualityShaders", !HighQualityShaders);
        game.SetInt("AutoDownResTextures", FullResolutionTextures ? 0 : DefaultTextureLimit);
    }
}
