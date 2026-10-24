using System.Buffers.Binary;
using HomecomingAgain.Launcher.Core;
using Xunit;

namespace HomecomingAgain.Launcher.Tests;

// Les messages du compte rendu sont traduits : même collection que les tests de langue.
[Collection("Language")]
public sealed class InstallTests : IDisposable
{
    private readonly string _root = Path.Combine(Path.GetTempPath(), "hca-tests", Guid.NewGuid().ToString("N"));

    public InstallTests()
    {
        Directory.CreateDirectory(Path.Combine(_root, "Bin"));
        Directory.CreateDirectory(Path.Combine(_root, "Engine"));
        File.WriteAllBytes(Path.Combine(_root, "Bin", GamePaths.GameExeName), MinimalExe(ExeInfo.KnownGameTimestamp, false));
    }

    public void Dispose()
    {
        if (Directory.Exists(_root))
        {
            Directory.Delete(_root, recursive: true);
        }
    }

    [Fact]
    public void MigratesTheIniKeepingUserValuesAndAddingNewKeys()
    {
        GamePaths paths = new(_root);
        string package = Path.Combine(_root, "pkg.ini");
        File.WriteAllText(package, "; header\n[Video]\nWidth=1280\nSMAA=1\nNewKey=5\n[Extra]\nFoo=1\n");
        File.WriteAllText(paths.ModIni, "[Video]\nWidth=3440\nObsolete=9\n");

        ModInstaller.MigrateSettings(package, paths);

        IniDocument merged = IniDocument.Load(paths.ModIni);
        Assert.Equal("3440", merged.Get("Video", "Width"));
        Assert.Equal("1", merged.Get("Video", "SMAA"));
        Assert.Equal("5", merged.Get("Video", "NewKey"));
        Assert.Equal("1", merged.Get("Extra", "Foo"));
        Assert.Null(merged.Get("Video", "Obsolete"));
        Assert.Contains("Obsolete=9", File.ReadAllText(paths.ModIniOld));
    }

    [Fact]
    public void LeavesTheIniAloneWhenNothingChanged()
    {
        GamePaths paths = new(_root);
        string package = Path.Combine(_root, "pkg.ini");
        File.WriteAllText(package, "[Video]\nWidth=1280\n");
        ModInstaller.MigrateSettings(package, paths);
        ModInstaller.MigrateSettings(package, paths);
        Assert.False(File.Exists(paths.ModIniOld));
    }

    [Fact]
    public void FindsTheGameFromItsRootOrItsBinFolder()
    {
        Assert.Equal(_root, GamePaths.ResolveRoot(_root));
        Assert.Equal(_root, GamePaths.ResolveRoot(Path.Combine(_root, "Bin")));
        Assert.Null(GamePaths.ResolveRoot(Path.Combine(_root, "Engine")));
    }

    [Fact]
    public void RejectsPackageWithoutModDll()
    {
        string package = Path.Combine(_root, "paquet-vide");
        Directory.CreateDirectory(package);
        Assert.False(ModInstaller.IsPackageDirectory(package));
        Assert.Throws<FileNotFoundException>(() => ModInstaller.Install(package, new GamePaths(_root), null));
    }

    [Fact]
    public void RejectsAFolderWithoutTheGame()
    {
        string empty = Path.Combine(_root, "Engine");
        Assert.Throws<InvalidDataException>(() => ModInstaller.Install(empty, new GamePaths(empty), null));
    }

    [Fact]
    public void ForeignVersionDllIsReportedAndRestoredOnUninstall()
    {
        GamePaths paths = new(_root);
        File.WriteAllText(paths.ModDll, "version.dll d'un autre mod");
        Assert.True(ModInstaller.ReadStatus(paths).HasForeignVersionDll);
        Assert.False(ModInstaller.ReadStatus(paths).IsModInstalled);

        // Situation après une installation : l'autre DLL a été mise de côté.
        File.Move(paths.ModDll, paths.ModDllBackup);
        ModInstaller.Uninstall(paths);
        Assert.Equal("version.dll d'un autre mod", File.ReadAllText(paths.ModDll));
        Assert.False(File.Exists(paths.ModDllBackup));
    }

    [Fact]
    public void GameSettingsBackupIsMadeOnceAndNeverOverwritten()
    {
        GamePaths paths = new(_root);
        File.WriteAllText(paths.GameSettings, "FullScreen=true\r\nScreenResWidth=800\r\n");

        KeyValueDocument settings = KeyValueDocument.Load(paths.GameSettings);
        settings.SetBool("FullScreen", false);
        ModInstaller.SaveGameSettings(paths, settings);
        settings.SetInt("ScreenResWidth", 1920);
        ModInstaller.SaveGameSettings(paths, settings);

        Assert.Equal("FullScreen=true\r\nScreenResWidth=800\r\n", File.ReadAllText(paths.GameSettingsBackup));
        Assert.Equal("FullScreen=false\r\nScreenResWidth=1920\r\n", File.ReadAllText(paths.GameSettings));
    }

    [Fact]
    public void ExtractsFilesThenSkipsWhenUpToDate()
    {
        string target = Path.Combine(_root, "extrait");
        int opened = 0;
        (string, Func<Stream>)[] files =
        [
            ("version.dll", () => { opened++; return new MemoryStream([1, 2, 3]); }),
            ("HomecomingAgain.ini", () => { opened++; return new MemoryStream([4]); }),
        ];
        EmbeddedPackage.Extract(files, target, "v1");
        EmbeddedPackage.Extract(files, target, "v1");
        Assert.Equal(2, opened);
        Assert.Equal([1, 2, 3], File.ReadAllBytes(Path.Combine(target, "version.dll")));
        EmbeddedPackage.Extract(files, target, "v2");
        Assert.Equal(4, opened);
    }

    [Fact]
    public void LargeAddressAwarePatchKeepsTheOriginalAndIsRestored()
    {
        GamePaths paths = new(_root);
        byte[] original = File.ReadAllBytes(paths.GameExe);
        Assert.False(ExePatcher.ReadInfo(paths.GameExe).LargeAddressAware);
        Assert.True(ExePatcher.ReadInfo(paths.GameExe).IsKnownVersion);

        Assert.True(ExePatcher.EnableLargeAddressAware(paths.GameExe));
        Assert.False(ExePatcher.EnableLargeAddressAware(paths.GameExe));  // déjà appliqué : rien n'est réécrit
        Assert.True(ExePatcher.ReadInfo(paths.GameExe).LargeAddressAware);
        Assert.Equal(original, File.ReadAllBytes(paths.OriginalExeBackup));

        ModInstaller.Uninstall(paths);
        Assert.Equal(original, File.ReadAllBytes(paths.GameExe));
    }

    [Fact]
    public void LinuxLauncherCopyNoLongerTakesTheModFolderName()
    {
        GamePaths paths = new(_root);
        string launcher = Path.Combine(_root, "telechargements", "HomecomingAgain");
        Directory.CreateDirectory(Path.GetDirectoryName(launcher)!);
        File.WriteAllText(launcher, "launcher Linux");
        // Copie laissée par la version 0.9.4 : un fichier à la place du dossier du mod.
        File.WriteAllText(paths.ModDataDirectory, "ancienne copie");

        ModInstaller.CopyLauncher(paths, launcher, windows: false);

        Assert.Equal("launcher Linux", File.ReadAllText(Path.Combine(_root, "Bin", "HomecomingAgain-launcher")));
        Assert.False(File.Exists(paths.ModDataDirectory));
    }

    [Fact]
    public void OldLinuxCopyRunningFromBinIsCopiedBeforeBeingRemoved()
    {
        GamePaths paths = new(_root);
        File.WriteAllText(paths.ModDataDirectory, "launcher Linux lancé par Steam");

        ModInstaller.CopyLauncher(paths, paths.ModDataDirectory, windows: false);

        Assert.Equal("launcher Linux lancé par Steam", File.ReadAllText(paths.LauncherCopy(windows: false)));
        Assert.False(File.Exists(paths.ModDataDirectory));
    }

    [Fact]
    public void ModFolderIsNeverRemoved()
    {
        GamePaths paths = new(_root);
        Directory.CreateDirectory(paths.LogsDirectory);

        ModInstaller.CopyLauncher(paths, null, windows: false);

        Assert.True(Directory.Exists(paths.LogsDirectory));
    }

    [Fact]
    public void RejectsAnInvalidGameExecutable()
    {
        GamePaths paths = new(_root);
        File.WriteAllText(paths.GameExe, "pas un exécutable");
        Assert.Throws<InvalidDataException>(() => ModInstaller.Install(_root, paths, null));
    }

    /// <summary>En-tête DOS + PE suffisant pour ExePatcher (machine i386, horodatage, caractéristiques).</summary>
    private static byte[] MinimalExe(uint timestamp, bool largeAddressAware)
    {
        byte[] bytes = new byte[512];
        bytes[0] = (byte)'M';
        bytes[1] = (byte)'Z';
        const int peOffset = 0x80;
        BinaryPrimitives.WriteInt32LittleEndian(bytes.AsSpan(0x3C), peOffset);
        "PE\0\0"u8.CopyTo(bytes.AsSpan(peOffset));
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(peOffset + 4), 0x014C);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(peOffset + 8), timestamp);
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(peOffset + 22), (ushort)(0x0102 | (largeAddressAware ? 0x20 : 0)));
        return bytes;
    }
}
