using HomecomingAgain.Launcher.Core;
using Xunit;

namespace HomecomingAgain.Launcher.Tests;

/// <summary>Ce qui sert à faire tourner le launcher sous Linux (jeu dans Proton), testable depuis Windows.</summary>
public sealed class LinuxTests
{
    [Fact]
    public void ReadsWindowsAndLinuxLibraryFolders()
    {
        const string vdf = """
            "libraryfolders"
            {
                "0" { "path" "C:\\Program Files (x86)\\Steam" }
                "1" { "path"		"/home/joueur/.local/share/Steam" }
            }
            """;

        Assert.Equal([@"C:\Program Files (x86)\Steam", "/home/joueur/.local/share/Steam"], GameLocator.ParseLibraryFolders(vdf));
    }

    [Theory]
    [InlineData("Z:\\home\\joueur\\.local\\share\\Steam\\steamapps\\common\\Silent Hill Homecoming\\Bin\\SilentHill.exe\0")]
    [InlineData("C:\\Jeux\\Bin\\silenthill.exe\0-arg\0")]
    [InlineData("/jeux/Bin/SilentHill.exe")]
    public void RecognisesTheGameCommandLine(string commandLine)
    {
        Assert.True(GameProcess.CommandLineRunsGame(commandLine));
    }

    [Theory]
    [InlineData("/usr/bin/bash\0")]
    [InlineData("/home/joueur/.steam/steam/ubuntu12_32/reaper\0SteamLaunch\0--\0proton\0waitforexitandrun\0Z:\\Bin\\SilentHill.exe\0")]
    [InlineData("C:\\Jeux\\Bin\\SilentHill.exe.hca-original\0")]
    [InlineData("")]
    public void IgnoresOtherCommandLines(string commandLine)
    {
        Assert.False(GameProcess.CommandLineRunsGame(commandLine));
    }

    [Theory]
    [InlineData(null, "version=n,b")]
    [InlineData("", "version=n,b")]
    [InlineData("d3d9=n,b", "d3d9=n,b;version=n,b")]
    [InlineData("dxgi,version=n", "dxgi,version=n")]
    public void AddsTheVersionDllOverrideOnce(string? existing, string expected)
    {
        Assert.Equal(expected, GameProcess.WithVersionOverride(existing));
    }

    [Fact]
    public void SteamLaunchOptionForcesOurDllUnderLinux()
    {
        GamePaths paths = new(Path.Combine("jeu"));

        Assert.Equal($"WINEDLLOVERRIDES=\"version=n,b\" \"{Path.Combine("jeu", "Bin", "HomecomingAgain")}\" %command%",
                     GameProcess.SteamLaunchOption(paths, windows: false));
        Assert.Equal($"\"{Path.Combine("jeu", "Bin", "HomecomingAgain.exe")}\" %command%",
                     GameProcess.SteamLaunchOption(paths, windows: true));
    }
}
