using HomecomingAgain.Launcher.Core;
using HomecomingAgain.Launcher.ViewModels;
using Xunit;

namespace HomecomingAgain.Launcher.Tests;

public sealed class SettingsTests
{
    // Extrait du vars_pc.cfg créé par le jeu à son premier lancement (Steam, build 6.30, relevé le 09/10/2026).
    private const string OriginalGameSettings =
        "AutoDownResTextures=512\r\nfps=false\r\nfpsLimit=30\r\nFullScreen=true\r\nLowQualityShaders=true\r\n" +
        "maxFPSLimit=30\r\nScreenRefresh=60\r\nScreenResHeight=600\r\nScreenResWidth=800\r\n";

    [Fact]
    public void KeyValueDocumentReadsIgnoringCaseAndUpdatesInPlace()
    {
        KeyValueDocument game = KeyValueDocument.Parse(OriginalGameSettings);
        Assert.Equal(800, game.GetInt("screenreswidth", 0));
        Assert.True(game.GetBool("FullScreen", false));

        game.SetInt("SCREENRESWIDTH", 1920);
        game.SetBool("FullScreen", false);
        game.Set("NewKey", "1");

        string[] lines = game.ToString().Split('\n');
        Assert.Contains("ScreenResWidth=1920", lines);  // casse d'origine conservée
        Assert.Contains("FullScreen=false", lines);
        Assert.Equal("NewKey=1", lines[^1]);
        Assert.Equal("AutoDownResTextures=512", lines[0]);  // ordre conservé
    }

    [Fact]
    public void ReadsTheOriginalGameAsExclusiveFullscreenLowQuality()
    {
        PatchSettings settings = PatchSettings.Read(IniDocument.Parse(""), KeyValueDocument.Parse(OriginalGameSettings));
        Assert.Equal(DisplayMode.Fullscreen, settings.DisplayMode);
        Assert.Equal((800, 600), (settings.Width, settings.Height));
        Assert.False(settings.HighQualityShaders);
        Assert.False(settings.FullResolutionTextures);
        // Valeurs par défaut de la DLL quand HomecomingAgain.ini est vide.
        Assert.True(settings.Smaa);
        Assert.Equal(16, settings.Anisotropy);
        Assert.Equal(60, settings.TargetFps);
        Assert.True(settings.Ssao);
        Assert.Equal(8192, settings.ShadowResolution);
        Assert.True(settings.WindowThreadLock && settings.D3DMultithreaded && settings.HangDetection);
        Assert.True(settings.SkipLogos);
        Assert.False(settings.God || settings.UnlimitedAmmo);
        Assert.False(settings.CameraOrbit);
    }

    [Theory]
    [InlineData("Orbit", true)]
    [InlineData("orbit", true)]
    [InlineData("Follow", false)]
    [InlineData("autre", false)]
    public void ReadsCameraModeLikeTheDll(string mode, bool orbit)
    {
        IniDocument mod = IniDocument.Parse($"[Camera]\nMode={mode}");
        PatchSettings settings = PatchSettings.Read(mod, KeyValueDocument.Parse(OriginalGameSettings));
        Assert.Equal(orbit, settings.CameraOrbit);
    }

    [Fact]
    public void WritesCameraModeAsText()
    {
        IniDocument mod = IniDocument.Parse("");
        (new PatchSettings() with { CameraOrbit = true }).WriteMod(mod);
        Assert.Equal("Orbit", mod.Get("Camera", "Mode"));
        (new PatchSettings()).WriteMod(mod);
        Assert.Equal("Follow", mod.Get("Camera", "Mode"));
    }

    [Theory]
    [InlineData(DisplayMode.Borderless, "false", "1")]
    [InlineData(DisplayMode.Windowed, "false", "0")]
    [InlineData(DisplayMode.Fullscreen, "true", "0")]
    public void DisplayModeIsSplitBetweenTheTwoFiles(DisplayMode mode, string fullScreen, string borderless)
    {
        IniDocument mod = IniDocument.Parse("");
        KeyValueDocument game = KeyValueDocument.Parse(OriginalGameSettings);
        PatchSettings settings = new() { DisplayMode = mode, Width = 1920, Height = 1080 };
        settings.WriteMod(mod);
        settings.WriteGame(game);

        Assert.Equal(fullScreen, game.Get("FullScreen"));
        Assert.Equal(borderless, mod.Get("Window", "Borderless"));
        Assert.Equal(mode, PatchSettings.Read(mod, game).DisplayMode);
    }

    [Fact]
    public void RoundTripsEveryOption()
    {
        PatchSettings written = new()
        {
            DisplayMode = DisplayMode.Windowed, Width = 2560, Height = 1440, HighQualityShaders = true,
            FullResolutionTextures = true, Smaa = false, Anisotropy = 4, TargetFps = 144, PreciseTimer = false,
            WindowThreadLock = false, D3DMultithreaded = false, CrashDumps = false, FullMemoryDumps = true,
            HangDetection = false, FrameStats = false, Ssao = false, SsaoStrength = 150, ShadowResolution = 8192,
            FilmGrain = 0, SkipLogos = false, God = true, UnlimitedAmmo = true, CameraFix = true,
            CameraOrbit = true,
            FullScreenUiClip = false, ObjectViewPerspective = false,
            OrbitGuardFollow = false, CameraHorizontalSpeed = 0.5, CameraVerticalSpeed = 1.25,
            CameraAimHorizontalSpeed = 2.5, CameraAimVerticalSpeed = 3.0, AudioChannelFix = false,
        };
        IniDocument mod = IniDocument.Parse("");
        KeyValueDocument game = KeyValueDocument.Parse(OriginalGameSettings);
        written.WriteMod(mod);
        written.WriteGame(game);

        Assert.Equal(written, PatchSettings.Read(mod, game));
        Assert.Equal("0", game.Get("AutoDownResTextures"));
        Assert.Equal("false", game.Get("LowQualityShaders"));
    }

    [Fact]
    public void NewOptionsHaveTheDllDefaultsWhenTheIniIsEmpty()
    {
        PatchSettings read = PatchSettings.Read(IniDocument.Parse(""), KeyValueDocument.Parse(OriginalGameSettings));
        Assert.True(read.FullScreenUiClip && read.ObjectViewPerspective && read.OrbitGuardFollow);
        Assert.True(read.AudioChannelFix);
        Assert.Equal((1.0, 1.0, 1.0, 1.0), (read.CameraHorizontalSpeed, read.CameraVerticalSpeed,
                                           read.CameraAimHorizontalSpeed, read.CameraAimVerticalSpeed));
    }

    [Fact]
    public void ViewModelsKeepEveryNewOptionBetweenLoadAndApply()
    {
        PatchSettings source = new()
        {
            FullScreenUiClip = false, ObjectViewPerspective = false,
            OrbitGuardFollow = false, CameraHorizontalSpeed = 0.5, CameraVerticalSpeed = 1.25,
            CameraAimHorizontalSpeed = 2.5, CameraAimVerticalSpeed = 3.0, AudioChannelFix = false,
        };
        GraphicsViewModel graphics = new();
        PerformanceViewModel performance = new();
        graphics.Load(source);
        performance.Load(source);

        PatchSettings result = performance.ApplyTo(graphics.ApplyTo(new PatchSettings()));

        Assert.Equal(source, result with { DisplayMode = source.DisplayMode, Width = source.Width, Height = source.Height,
                                           TargetFps = source.TargetFps });
    }

    [Fact]
    public void SettingsTheLauncherDoesNotExposeAreLeftAsTheyAre()
    {
        // Réglages du .ini sans case dans le launcher : l'enregistrement ne doit ni les remettre à leur défaut ni les retirer.
        const string text = "[Graphics]\nSsaoRadius=2.25\nSsaoFadeStart=200\nSsaoFadeEnd=600\nD3D9On12=1\n" +
                            "[Audio]\nChannelOrder=0,1,2,3,4,5\nChannels=6\n[Movies]\nSkipKey=27\n" +
                            "[Frames]\nReportIntervalSeconds=30\nFrameLimitFps=90\n" +
                            "[Crash]\nDumpKeepCount=5\n[Debug]\nLogKeepCount=100\nFrameCapture=1\nSkipMovies=1\n" +
                            "[Input]\nAnimatedIcons=0\nPlayStationPad=0";
        IniDocument mod = IniDocument.Parse(text);
        new PatchSettings().WriteMod(mod);
        Assert.Equal(("2.25", "200", "600", "1"), (mod.Get("Graphics", "SsaoRadius"), mod.Get("Graphics", "SsaoFadeStart"),
                                                    mod.Get("Graphics", "SsaoFadeEnd"), mod.Get("Graphics", "D3D9On12")));
        Assert.Equal(("0,1,2,3,4,5", "27", "30", "5"), (mod.Get("Audio", "ChannelOrder"), mod.Get("Movies", "SkipKey"),
                                                         mod.Get("Frames", "ReportIntervalSeconds"),
                                                         mod.Get("Crash", "DumpKeepCount")));
        Assert.Equal(("100", "1", "1"), (mod.Get("Debug", "LogKeepCount"), mod.Get("Debug", "FrameCapture"),
                                         mod.Get("Debug", "SkipMovies")));
        Assert.Equal(("6", "90"), (mod.Get("Audio", "Channels"), mod.Get("Frames", "FrameLimitFps")));
        Assert.Equal(("0", "0"), (mod.Get("Input", "AnimatedIcons"), mod.Get("Input", "PlayStationPad")));
    }

    [Theory]
    [InlineData(false, false, false)]
    [InlineData(false, true, false)]  // correctif décoché alors que l'orbite reste cochée : options grisées
    [InlineData(true, false, false)]
    [InlineData(true, true, true)]
    public void OrbitOptionsNeedBothTheCameraFixAndTheOrbit(bool cameraFix, bool orbit, bool enabled)
    {
        GraphicsViewModel graphics = new() { CameraFix = cameraFix, CameraOrbit = orbit };
        Assert.Equal(enabled, graphics.OrbitOptionsEnabled);
        List<string> changed = [];
        graphics.PropertyChanged += (_, e) => changed.Add(e.PropertyName ?? "");
        graphics.CameraFix = !cameraFix;
        Assert.Contains(nameof(GraphicsViewModel.OrbitOptionsEnabled), changed);
    }

    [Fact]
    public void NewOptionsAreClampedToWhatTheDllAccepts()
    {
        IniDocument mod = IniDocument.Parse("");
        (new PatchSettings
        {
            CameraHorizontalSpeed = 9, CameraVerticalSpeed = 0.01,
        }).WriteMod(mod);
        Assert.Equal(("3.0", "0.25"), (mod.Get("Camera", "HorizontalSpeed"), mod.Get("Camera", "VerticalSpeed")));
    }

    [Theory]
    [InlineData("1.5", 1.5)]
    [InlineData("0.25", 0.25)]
    [InlineData("abc", 7.0)]
    [InlineData("", 7.0)]
    [InlineData("1,5", 7.0)]
    public void DecimalNumbersAreReadWithADotWhateverTheWindowsLanguage(string text, double expected)
    {
        IniDocument mod = IniDocument.Parse($"[Camera]\nHorizontalSpeed={text}");
        Assert.Equal(expected, mod.GetDouble("Camera", "HorizontalSpeed", 7.0));
        IniDocument written = IniDocument.Parse("");
        written.SetDouble("Camera", "HorizontalSpeed", 1.5);
        Assert.Equal("1.5", written.Get("Camera", "HorizontalSpeed"));
    }

    [Theory]
    [InlineData(3000, "2048")]
    [InlineData(4096, "4096")]
    [InlineData(8192, "8192")]
    public void ShadowResolutionIsLimitedToSupportedSizes(int requested, string written)
    {
        IniDocument mod = IniDocument.Parse("");
        (new PatchSettings() with { ShadowResolution = requested }).WriteMod(mod);
        Assert.Equal(written, mod.Get("Graphics", "ShadowResolution"));
    }

    [Fact]
    public void DefaultsKeepOnlyTheDisplay()
    {
        PatchSettings custom = new() { DisplayMode = DisplayMode.Windowed, Width = 1280, Height = 720, Smaa = false, TargetFps = 0 };
        PatchSettings defaults = DefaultSettings.For(custom);
        Assert.Equal((DisplayMode.Windowed, 1280, 720), (defaults.DisplayMode, defaults.Width, defaults.Height));
        Assert.True(defaults.Smaa);
        Assert.Equal(60, defaults.TargetFps);
    }

    [Fact]
    public void ButtonIconsAreWrittenAndClamped()
    {
        IniDocument mod = IniDocument.Parse("");
        Assert.Equal(0, PatchSettings.Read(mod, KeyValueDocument.Parse("")).ButtonIcons);  // auto par défaut
        new PatchSettings { ButtonIcons = 3 }.WriteMod(mod);
        Assert.Equal(3, PatchSettings.Read(mod, KeyValueDocument.Parse("")).ButtonIcons);
        new PatchSettings { ButtonIcons = 9 }.WriteMod(mod);
        Assert.Equal(3, mod.GetInt("Input", "ButtonIcons", -1));
    }

    [Theory]
    [InlineData(0, "A B X Y")]
    [InlineData(1, "1 2 3 4")]
    [InlineData(2, "A B X Y")]
    [InlineData(3, "✕ ○ □ △")]
    public void ButtonIconPreviewFollowsTheSetting(int buttonIcons, string symbols)
    {
        Assert.Equal(symbols, string.Join(' ', ButtonGlyph.For(buttonIcons).Select(glyph => glyph.Symbol)));
    }
}
