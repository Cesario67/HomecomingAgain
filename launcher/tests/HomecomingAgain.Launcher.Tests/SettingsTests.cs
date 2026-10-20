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
        Assert.True(settings.PlayStationPad);
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
            FilmGrain = 0, SkipLogos = false, God = true, UnlimitedAmmo = true, PlayStationPad = false, CameraFix = true,
            CameraOrbit = true,
            FullScreenUiClip = false, ObjectViewPerspective = false, SsaoRadius = 2.25, SsaoFadeStart = 200,
            SsaoFadeEnd = 600, D3D9On12 = true, AnimatedIcons = false, MenuPad = true, OrbitGuardFollow = false,
            CameraHorizontalSpeed = 0.5, CameraVerticalSpeed = 1.25, CameraAimHorizontalSpeed = 2.5,
            CameraAimVerticalSpeed = 3.0, AudioChannelFix = false, AudioChannels = 6, AudioChannelOrder = "0,1,2,3,4,5",
            FrameLimitFps = 90, ReportIntervalSeconds = 30, SkipKey = 27, DumpKeepCount = 5, LogKeepCount = 100,
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
        Assert.True(read.FullScreenUiClip && read.ObjectViewPerspective && read.AnimatedIcons && read.OrbitGuardFollow);
        Assert.True(read.AudioChannelFix);
        Assert.False(read.MenuPad || read.D3D9On12);
        Assert.Equal((1.5, 150, 450), (read.SsaoRadius, read.SsaoFadeStart, read.SsaoFadeEnd));
        Assert.Equal((1.0, 1.0, 1.0, 1.0), (read.CameraHorizontalSpeed, read.CameraVerticalSpeed,
                                           read.CameraAimHorizontalSpeed, read.CameraAimVerticalSpeed));
        Assert.Equal((0, 0, 10, 46), (read.AudioChannels, read.FrameLimitFps, read.ReportIntervalSeconds, read.SkipKey));
        Assert.Equal((10, 30, "2,4,1,3,5,0"), (read.DumpKeepCount, read.LogKeepCount, read.AudioChannelOrder));
    }

    [Fact]
    public void ViewModelsKeepEveryNewOptionBetweenLoadAndApply()
    {
        PatchSettings source = new()
        {
            FullScreenUiClip = false, ObjectViewPerspective = false, SsaoRadius = 2.25, SsaoFadeStart = 200,
            SsaoFadeEnd = 600, D3D9On12 = true, AnimatedIcons = false, MenuPad = true, OrbitGuardFollow = false,
            CameraHorizontalSpeed = 0.5, CameraVerticalSpeed = 1.25, CameraAimHorizontalSpeed = 2.5,
            CameraAimVerticalSpeed = 3.0, AudioChannelFix = false, AudioChannels = 8, AudioChannelOrder = "0,1,2,3,4,5",
            FrameLimitFps = 90, ReportIntervalSeconds = 30, SkipKey = 27, DumpKeepCount = 5, LogKeepCount = 100,
        };
        GraphicsViewModel graphics = new();
        PerformanceViewModel performance = new();
        AdvancedViewModel advanced = new();
        graphics.Load(source);
        performance.Load(source);
        advanced.Load(source);

        PatchSettings result = advanced.ApplyTo(performance.ApplyTo(graphics.ApplyTo(new PatchSettings())));

        Assert.Equal(source, result with { DisplayMode = source.DisplayMode, Width = source.Width, Height = source.Height,
                                           TargetFps = source.TargetFps });
    }

    [Fact]
    public void ACustomFrameLimitFromTheIniIsKeptNotLost()
    {
        PerformanceViewModel performance = new();
        performance.Load(new PatchSettings { FrameLimitFps = 100 });
        Assert.Equal(100, performance.SelectedFrameLimit.Value);
        Assert.Equal(100, performance.ApplyTo(new PatchSettings()).FrameLimitFps);
    }

    [Fact]
    public void ClearingANumericFieldKeepsTheDefault()
    {
        AdvancedViewModel advanced = new() { LogKeepCount = null, SsaoRadius = null, ChannelOrder = "x" };
        PatchSettings result = advanced.ApplyTo(new PatchSettings());
        Assert.Equal((30, 1.5, "2,4,1,3,5,0"), (result.LogKeepCount, result.SsaoRadius, result.AudioChannelOrder));
        Assert.True(advanced.ChannelOrderInvalid);
    }

    [Fact]
    public void NewOptionsAreClampedToWhatTheDllAccepts()
    {
        IniDocument mod = IniDocument.Parse("");
        (new PatchSettings
        {
            CameraHorizontalSpeed = 9, CameraVerticalSpeed = 0.01, SsaoRadius = 500, SsaoFadeStart = 300, SsaoFadeEnd = 100,
            AudioChannels = 4, FrameLimitFps = 5000, ReportIntervalSeconds = 0, SkipKey = 999, DumpKeepCount = 0,
            LogKeepCount = 9999,
        }).WriteMod(mod);
        Assert.Equal(("3.0", "0.25"), (mod.Get("Camera", "HorizontalSpeed"), mod.Get("Camera", "VerticalSpeed")));
        Assert.Equal("100.0", mod.Get("Graphics", "SsaoRadius"));
        Assert.Equal(("300", "301"), (mod.Get("Graphics", "SsaoFadeStart"), mod.Get("Graphics", "SsaoFadeEnd")));
        Assert.Equal(("0", "1000", "1"), (mod.Get("Audio", "Channels"), mod.Get("Frames", "FrameLimitFps"),
                                          mod.Get("Frames", "ReportIntervalSeconds")));
        Assert.Equal(("254", "1", "500"), (mod.Get("Movies", "SkipKey"), mod.Get("Crash", "DumpKeepCount"),
                                           mod.Get("Debug", "LogKeepCount")));
    }

    [Theory]
    [InlineData("2,4,1,3,5,0", true)]
    [InlineData(" 0, 1, 2, 3, 4, 5 ", true)]
    [InlineData("2,4,1,3,5", false)]
    [InlineData("2,2,1,3,5,0", false)]
    [InlineData("2,4,1,3,5,9", false)]
    [InlineData("a,b,c,d,e,f", false)]
    [InlineData("", false)]
    public void ChannelOrderIsValidatedLikeTheDll(string order, bool valid)
    {
        Assert.Equal(valid, PatchSettings.IsValidChannelOrder(order));
        IniDocument mod = IniDocument.Parse("");
        (new PatchSettings { AudioChannelOrder = order }).WriteMod(mod);
        Assert.Equal(valid ? order.Trim() : "2,4,1,3,5,0", mod.Get("Audio", "ChannelOrder"));
    }

    [Fact]
    public void InvalidChannelOrderInTheIniFallsBackToTheDefault()
    {
        IniDocument mod = IniDocument.Parse("[Audio]\nChannelOrder=1,1,1,1,1,1");
        PatchSettings read = PatchSettings.Read(mod, KeyValueDocument.Parse(OriginalGameSettings));
        Assert.Equal("2,4,1,3,5,0", read.AudioChannelOrder);
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
