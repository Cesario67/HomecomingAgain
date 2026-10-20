using System.Diagnostics;
using System.Text;
using HomecomingAgain.Launcher.Core;
using Xunit;

namespace HomecomingAgain.Launcher.Tests;

/// <summary>
/// Lecture de la version de version.dll sans FileVersionInfo, qui sous Linux ne lit pas les DLL natives : le launcher
/// Linux 0.9.4 ne reconnaissait pas son propre paquet et grisait le bouton Installer.
/// </summary>
public sealed class PeVersionInfoTests
{
    [Fact]
    public void ReadsTheStringsOfAVersionResource()
    {
        byte[] version = VersionInfo(("FileDescription", "Homecoming Again, correctifs"), ("FileVersion", "0.9.4"));

        IReadOnlyDictionary<string, string> strings = PeVersionInfo.ParseStrings(version);

        Assert.Equal("Homecoming Again, correctifs", strings["FileDescription"]);
        Assert.Equal("0.9.4", strings["FileVersion"]);
    }

    [Fact]
    public void IgnoresFilesThatAreNotVersionResourcesOrPortableExecutables()
    {
        Assert.Empty(PeVersionInfo.ParseStrings(Encoding.Unicode.GetBytes("pas une ressource de version")));

        string file = Path.Combine(Path.GetTempPath(), $"hca-pe-{Guid.NewGuid():N}.dll");
        File.WriteAllText(file, "version.dll d'un autre mod");
        try
        {
            Assert.Empty(PeVersionInfo.ReadStrings(file));
            Assert.Null(ModInstaller.ModVersionOf(file));
        }
        finally
        {
            File.Delete(file);
        }
    }

    // Sous Windows, FileVersionInfo sert de référence : mêmes chaînes pour une DLL 64 bits et une DLL 32 bits
    // (comme notre version.dll). Une DLL sans traduction (.mui), dont Windows lirait sinon la version traduite.
    [Theory]
    [InlineData("System32")]
    [InlineData("SysWOW64")]
    public void ReadsTheSameStringsAsWindows(string folder)
    {
        string dll = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), folder, "ucrtbase.dll");
        if (!OperatingSystem.IsWindows() || !File.Exists(dll))
        {
            return;
        }
        FileVersionInfo expected = FileVersionInfo.GetVersionInfo(dll);

        IReadOnlyDictionary<string, string> strings = PeVersionInfo.ReadStrings(dll);

        Assert.Equal(expected.FileDescription, strings["FileDescription"]);
        Assert.Equal(expected.FileVersion, strings["FileVersion"]);
        Assert.Equal(expected.CompanyName, strings["CompanyName"]);
    }

    // VS_VERSIONINFO : racine (valeur VS_FIXEDFILEINFO de 52 octets), StringFileInfo, une table, les chaînes.
    private static byte[] VersionInfo(params (string Key, string Value)[] strings)
    {
        byte[][] entries = strings.Select(entry => Block(entry.Key, Encoding.Unicode.GetBytes(entry.Value + "\0"), 1,
                                                         (entry.Value.Length + 1))).ToArray();
        byte[] table = Block("040904b0", [], 1, 0, entries);
        byte[] stringFileInfo = Block("StringFileInfo", [], 1, 0, table);
        return Block("VS_VERSION_INFO", new byte[52], 0, 52, stringFileInfo);
    }

    private static byte[] Block(string key, byte[] value, int type, int valueLength, params byte[][] children)
    {
        List<byte> block = [0, 0, (byte)valueLength, (byte)(valueLength >> 8), (byte)type, 0];
        block.AddRange(Encoding.Unicode.GetBytes(key + "\0"));
        Pad(block);
        block.AddRange(value);
        foreach (byte[] child in children)
        {
            Pad(block);
            block.AddRange(child);
        }
        block[0] = (byte)block.Count;
        block[1] = (byte)(block.Count >> 8);
        return [.. block];
    }

    private static void Pad(List<byte> block)
    {
        while (block.Count % 4 != 0)
        {
            block.Add(0);
        }
    }
}
