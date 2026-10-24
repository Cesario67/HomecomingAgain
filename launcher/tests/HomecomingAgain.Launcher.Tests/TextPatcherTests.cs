using System.Text;
using HomecomingAgain.Launcher.Core;
using Xunit;

namespace HomecomingAgain.Launcher.Tests;

public sealed class TextPatcherTests : IDisposable
{
    private readonly string _root = Path.Combine(Path.GetTempPath(), "hca-tests", Guid.NewGuid().ToString("N"));
    private readonly GamePaths _paths;

    // Windows-1252 bytes in the original ("é" = 0xE9), CRLF line endings, an anchor line like the real files.
    private const string Original = "[M00_Lock1]C'est verrouillé.\r\n[M00_HandWritten]C'est un message écrit.\r\n\r\n### M01 ###\r\n";

    public TextPatcherTests()
    {
        _paths = new GamePaths(_root);
        Directory.CreateDirectory(_paths.StringsDirectory);
        foreach (string language in TextPatcher.Languages)
        {
            File.WriteAllBytes(TextPatcher.FilePath(_paths, language), Encoding.Latin1.GetBytes(Original));
        }
    }

    public void Dispose()
    {
        if (Directory.Exists(_root))
        {
            Directory.Delete(_root, recursive: true);
        }
    }

    private static string[] Lines(string language) =>
    [
        "[M00_QuickInventory_01]<COMMAND_KICK> Examiner   <INVENTORY_WHEEL> Inventaire",
        "[M00_Lock1]ne doit pas remplacer l'existant",
        "[M00_Accent]Pierres tombales éè " + language,
    ];

    private string Read(string language) => Encoding.Latin1.GetString(File.ReadAllBytes(TextPatcher.FilePath(_paths, language)));

    [Fact]
    public void AddsOnlyMissingKeysAfterTheAnchorWithTheFileLineEnding()
    {
        (string content, int added) = TextPatcher.Insert(Original, Lines("FRA"));
        Assert.Equal(2, added);
        Assert.Equal("[M00_Lock1]C'est verrouillé.\r\n[M00_HandWritten]C'est un message écrit.\r\n" +
                     "[M00_QuickInventory_01]<COMMAND_KICK> Examiner   <INVENTORY_WHEEL> Inventaire\r\n" +
                     "[M00_Accent]Pierres tombales éè FRA\r\n\r\n### M01 ###\r\n", content);
    }

    [Fact]
    public void KeepsLfLineEndingsAndIgnoresAMissingAnchor()
    {
        (string lf, int added) = TextPatcher.Insert("[M00_HandWritten]x\n[A]a\n", ["[B]b"]);
        Assert.Equal(1, added);
        Assert.Equal("[M00_HandWritten]x\n[B]b\n[A]a\n", lf);
        Assert.Equal(("[A]a\n", 0), TextPatcher.Insert("[A]a\n", ["[B]b"]));
    }

    [Fact]
    public void AppliesTwiceWithoutChangeAndWritesWindows1252Bytes()
    {
        Assert.Equal(8, TextPatcher.Apply(_paths, Lines));
        byte[] first = File.ReadAllBytes(TextPatcher.FilePath(_paths, "GER"));
        Assert.Contains((byte)0xE8, first);
        Assert.DoesNotContain((byte)0xC3, first);
        Assert.Equal(0, TextPatcher.Apply(_paths, Lines));
        Assert.Equal(first, File.ReadAllBytes(TextPatcher.FilePath(_paths, "GER")));
        Assert.Contains("[M00_Lock1]C'est verrouillé.", Read("GER"));
    }

    [Fact]
    public void RestoreGivesBackTheOriginalFilesAndRemovesTheCopies()
    {
        TextPatcher.Apply(_paths, Lines);
        Assert.All(TextPatcher.Languages, language => Assert.True(File.Exists(TextPatcher.BackupPath(_paths, language))));
        Assert.Equal(4, TextPatcher.Restore(_paths));
        Assert.All(TextPatcher.Languages, language =>
        {
            Assert.Equal(Original, Read(language));
            Assert.False(File.Exists(TextPatcher.BackupPath(_paths, language)));
        });
        Assert.Equal(0, TextPatcher.Restore(_paths));
    }

    [Fact]
    public void TheFirstBackupIsNeverOverwritten()
    {
        TextPatcher.Apply(_paths, language => ["[M00_One]1"]);
        TextPatcher.Apply(_paths, language => ["[M00_Two]2"]);
        TextPatcher.Restore(_paths);
        Assert.Equal(Original, Read("FRA"));
    }

    [Fact]
    public void EmbeddedTranslationsExistForEveryLanguageAndKeepTheEnglishTags()
    {
        foreach (string language in TextPatcher.Languages)
        {
            string[] lines = TextPatcher.EmbeddedLines(language);
            Assert.NotEmpty(lines);
            Assert.All(lines, line => Assert.Matches(@"^\[[A-Za-z0-9_]+\].+", line));
            Assert.Equal(lines.Length, lines.Select(line => line[..(line.IndexOf(']') + 1)]).Distinct().Count());
            Assert.All(lines, line => Assert.Equal(CountOf(line, "<COLOR,"), CountOf(line, "<CLEARCOLOR>")));
        }
    }

    [Fact]
    public void SaveMissingTextsAddsThenRemovesAndNeedsNoGameRunning()
    {
        // The embedded lines target real keys that the test files lack: enabling adds them, disabling restores.
        ModInstaller.SaveMissingTexts(_paths, enabled: true);
        Assert.NotEqual(Original, Read("FRA"));
        ModInstaller.SaveMissingTexts(_paths, enabled: true);
        string once = Read("FRA");
        ModInstaller.SaveMissingTexts(_paths, enabled: true);
        Assert.Equal(once, Read("FRA"));
        ModInstaller.SaveMissingTexts(_paths, enabled: false);
        Assert.Equal(Original, Read("FRA"));
    }

    private static int CountOf(string text, string part) => (text.Length - text.Replace(part, "", StringComparison.Ordinal).Length) / part.Length;
}
