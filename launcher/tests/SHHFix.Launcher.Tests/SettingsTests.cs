using SHHFix.Launcher.Core;
using Xunit;

namespace SHHFix.Launcher.Tests;

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
        // Valeurs par défaut de la DLL quand SHHFix.ini est vide.
        Assert.True(settings.Smaa);
        Assert.Equal(16, settings.Anisotropy);
        Assert.Equal(60, settings.TargetFps);
        Assert.True(settings.WindowThreadLock && settings.D3DMultithreaded && settings.HangDetection);
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
            HangDetection = false, FrameStats = false,
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
    public void DefaultsKeepOnlyTheDisplay()
    {
        PatchSettings custom = new() { DisplayMode = DisplayMode.Windowed, Width = 1280, Height = 720, Smaa = false, TargetFps = 0 };
        PatchSettings defaults = DefaultSettings.For(custom);
        Assert.Equal((DisplayMode.Windowed, 1280, 720), (defaults.DisplayMode, defaults.Width, defaults.Height));
        Assert.True(defaults.Smaa);
        Assert.Equal(60, defaults.TargetFps);
    }
}
