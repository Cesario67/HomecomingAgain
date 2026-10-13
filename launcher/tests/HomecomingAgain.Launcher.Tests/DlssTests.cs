using HomecomingAgain.Launcher.Core;
using Xunit;

namespace HomecomingAgain.Launcher.Tests;

// Les messages du compte rendu sont traduits : même collection que les tests de langue.
[Collection("Language")]
public sealed class DlssTests : IDisposable
{
    private readonly string _root = Path.Combine(Path.GetTempPath(), "hca-tests", Guid.NewGuid().ToString("N"));
    private readonly string _package;
    private readonly GamePaths _paths;

    public DlssTests()
    {
        _package = Path.Combine(_root, "paquet");
        Directory.CreateDirectory(_package);
        Directory.CreateDirectory(Path.Combine(_root, "Bin"));
        _paths = new GamePaths(_root);
        File.WriteAllText(Path.Combine(_package, DlssInstaller.DxvkDll), "dxvk");
        File.WriteAllText(Path.Combine(_package, DlssInstaller.HelperExe), "programme DLSS");
        File.WriteAllText(Path.Combine(_package, DlssInstaller.NvidiaDll), "nvngx");
        File.WriteAllText(Path.Combine(_package, "LICENSE-DXVK.txt"), "zlib");
        File.WriteAllText(Path.Combine(_package, "LICENSE-NVIDIA-DLSS.txt"), "NVIDIA");
    }

    public void Dispose()
    {
        if (Directory.Exists(_root))
        {
            Directory.Delete(_root, recursive: true);
        }
    }

    private string Bin(string name) => Path.Combine(_paths.BinDirectory, name);

    [Fact]
    public void SettingWritesTheThreeDlssKeys()
    {
        IniDocument mod = IniDocument.Parse("");
        new PatchSettings { Dlss = true }.WriteMod(mod);
        Assert.True(PatchSettings.Read(mod, KeyValueDocument.Parse("")).Dlss);
        Assert.Equal(1, mod.GetInt("DLSS", "Jitter", 0));
        new PatchSettings { Dlss = false }.WriteMod(mod);
        Assert.False(PatchSettings.Read(mod, KeyValueDocument.Parse("")).Dlss);
        Assert.Equal(0, mod.GetInt("DLSS", "Jitter", 1));
        Assert.False(mod.GetBool("DLSS", "Apply", true));
    }

    [Fact]
    public void EnablingInstallsAndDisablingRemovesOnlyDxvk()
    {
        Assert.True(DlssInstaller.IsAvailable(_package));
        DlssInstaller.Apply(true, _package, _paths);
        Assert.True(DlssInstaller.IsOurDxvk(_package, _paths));
        Assert.True(File.Exists(Bin(DlssInstaller.HelperExe)));
        Assert.True(File.Exists(Bin("HomecomingAgain-LICENSE-NVIDIA-DLSS.txt")));
        Assert.Empty(DlssInstaller.Apply(true, _package, _paths));  // déjà à jour : rien à signaler

        DlssInstaller.Apply(false, _package, _paths);
        Assert.False(File.Exists(Bin(DlssInstaller.DxvkDll)));
        Assert.True(File.Exists(Bin(DlssInstaller.HelperExe)));  // inutile sans DXVK, laissé en place
    }

    [Fact]
    public void ForeignD3d9IsSetAsideAndRestored()
    {
        File.WriteAllText(Bin(DlssInstaller.DxvkDll), "d3d9 d'un autre mod");
        DlssInstaller.Install(_package, _paths);
        Assert.True(DlssInstaller.IsOurDxvk(_package, _paths));
        Assert.Equal("d3d9 d'un autre mod", File.ReadAllText(Bin(DlssInstaller.DxvkDll + ".avant-hca")));

        DlssInstaller.Remove(_package, _paths);
        Assert.Equal("d3d9 d'un autre mod", File.ReadAllText(Bin(DlssInstaller.DxvkDll)));
        Assert.False(File.Exists(Bin(DlssInstaller.DxvkDll + ".avant-hca")));
    }

    [Fact]
    public void DisablingNeverDeletesAForeignD3d9()
    {
        File.WriteAllText(Bin(DlssInstaller.DxvkDll), "d3d9 d'un autre mod");
        Assert.Empty(DlssInstaller.Remove(_package, _paths));
        Assert.True(File.Exists(Bin(DlssInstaller.DxvkDll)));
    }

    [Fact]
    public void UninstallRemovesOurFilesButNotAForeignNvngx()
    {
        DlssInstaller.Install(_package, _paths);
        File.WriteAllText(Bin(DlssInstaller.NvidiaDll), "nvngx d'une autre version");
        DlssInstaller.RemoveAll(_package, _paths);
        Assert.False(File.Exists(Bin(DlssInstaller.DxvkDll)));
        Assert.False(File.Exists(Bin(DlssInstaller.HelperExe)));
        Assert.True(File.Exists(Bin(DlssInstaller.NvidiaDll)));
    }

    [Fact]
    public void IncompletePackageIsRefused()
    {
        File.Delete(Path.Combine(_package, DlssInstaller.NvidiaDll));
        Assert.False(DlssInstaller.IsAvailable(_package));
        Assert.Throws<FileNotFoundException>(() => DlssInstaller.Install(_package, _paths));
    }
}
