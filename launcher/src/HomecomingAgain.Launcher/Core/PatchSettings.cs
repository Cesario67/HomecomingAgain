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

    // --- DLSS ([DLSS]) : ShareFrames, Jitter et Apply ensemble. Désactivé par défaut (NVIDIA RTX et DXVK nécessaires).
    public bool Dlss { get; init; }

    // --- Manette ([Input]) : icônes de boutons, 0 = auto (Xbox 360 si une manette est branchée), 1 = clavier,
    // 2 = Xbox 360, 3 = PS3.
    public int ButtonIcons { get; init; }

    // --- Caméra à la manette ([Camera] CameraFix, plus les commandes du stick droit, voir ControllerBinds).
    public bool CameraFix { get; init; }

    // --- Cadence ([Frames])
    public int TargetFps { get; init; } = 60;
    public bool PreciseTimer { get; init; } = true;

    // --- Compatibilité ([Compat])
    public bool WindowThreadLock { get; init; } = true;
    public bool D3DMultithreaded { get; init; } = true;

    // --- Diagnostic ([Crash], [Frames])
    public bool CrashDumps { get; init; } = true;
    public bool FullMemoryDumps { get; init; }
    public bool HangDetection { get; init; } = true;
    public bool FrameStats { get; init; } = true;

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
            Dlss = mod.GetBool("DLSS", "ShareFrames", false) && mod.GetBool("DLSS", "Apply", false),
            ButtonIcons = Math.Clamp(mod.GetInt("Input", "ButtonIcons", 0), 0, 3),
            CameraFix = mod.GetBool("Camera", "CameraFix", defaults.CameraFix),
            TargetFps = mod.GetInt("Frames", "TargetFps", defaults.TargetFps),
            PreciseTimer = mod.GetInt("Frames", "TimerResolutionMs", 1) > 0,
            WindowThreadLock = mod.GetBool("Compat", "WindowThreadLock", defaults.WindowThreadLock),
            D3DMultithreaded = mod.GetBool("Compat", "D3DMultithreaded", defaults.D3DMultithreaded),
            CrashDumps = mod.GetBool("Crash", "CrashDumps", defaults.CrashDumps),
            FullMemoryDumps = mod.GetBool("Crash", "FullMemoryDumps", defaults.FullMemoryDumps),
            HangDetection = mod.GetInt("Crash", "HangSeconds", 3) > 0,
            FrameStats = mod.GetBool("Frames", "FrameStats", defaults.FrameStats),
        };
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
        mod.SetBool("DLSS", "ShareFrames", Dlss);
        mod.SetInt("DLSS", "Jitter", Dlss ? 1 : 0);
        mod.SetBool("DLSS", "Apply", Dlss);
        mod.SetInt("Input", "ButtonIcons", Math.Clamp(ButtonIcons, 0, 3));
        mod.SetBool("Camera", "CameraFix", CameraFix);
        mod.SetInt("Frames", "TargetFps", Math.Clamp(TargetFps, 0, 1000));
        mod.SetInt("Frames", "TimerResolutionMs", PreciseTimer ? 1 : 0);
        mod.SetBool("Compat", "WindowThreadLock", WindowThreadLock);
        mod.SetBool("Compat", "D3DMultithreaded", D3DMultithreaded);
        mod.SetBool("Crash", "CrashDumps", CrashDumps);
        mod.SetBool("Crash", "FullMemoryDumps", FullMemoryDumps);
        mod.SetInt("Crash", "HangSeconds", HangDetection ? 3 : 0);
        mod.SetBool("Frames", "FrameStats", FrameStats);
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
