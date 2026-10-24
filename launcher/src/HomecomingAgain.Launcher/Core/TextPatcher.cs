using System.Reflection;
using System.Text;

namespace HomecomingAgain.Launcher.Core;

/// <summary>
/// Adds the text lines that the original game lacks in its translated string tables
/// (<c>Engine\gameinfo\strings\strings_FRA/GER/ITA/SPA.str</c>): the prompts of the "Inventory" tutorial, some
/// investigation prompts, menu labels (Italian) and a few notes. Without them the game shows nothing at all for
/// those texts (checked in French for the prompts; the other languages are assumed to behave the same).
/// The lines are embedded in the launcher (<c>Resources/Texts/*.txt</c>, UTF-8, one <c>[KEY]text</c> per line).
/// A key already present in the game file is never touched. The first change of a file makes a copy of the
/// original (<c>.hca-backup</c>, never overwritten) that <see cref="Restore"/> puts back.
/// </summary>
public static class TextPatcher
{
    public static readonly string[] Languages = ["FRA", "GER", "ITA", "SPA"];

    private const string Anchor = "[M00_HandWritten]";
    private const string ResourcePrefix = "texts/";

    // The game files are Windows-1252; Latin-1 maps every byte to the same code point, so the existing bytes round-trip
    // unchanged and the characters we add (all below U+0100) are written as their single Windows-1252 byte.
    private static readonly Encoding Latin1 = Encoding.Latin1;

    public static string FilePath(GamePaths paths, string language) =>
        Path.Combine(paths.StringsDirectory, $"strings_{language}.str");

    public static string BackupPath(GamePaths paths, string language) => FilePath(paths, language) + ".hca-backup";

    /// <summary>Embedded lines of a language (UTF-8 text of the resource); empty if the launcher has none.</summary>
    public static string[] EmbeddedLines(string language)
    {
        Assembly assembly = typeof(TextPatcher).Assembly;
        using Stream? stream = assembly.GetManifestResourceStream(ResourcePrefix + language + ".txt");
        if (stream is null)
        {
            return [];
        }
        using StreamReader reader = new(stream, Encoding.UTF8);
        return reader.ReadToEnd().Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
    }

    /// <summary>Number of lines of <paramref name="lines"/> whose key is absent from <paramref name="content"/>.</summary>
    public static int CountMissing(string content, IEnumerable<string> lines) =>
        lines.Count(line => KeyOf(line) is { } key && !HasKey(content, key));

    /// <summary>
    /// Inserts the lines whose key is absent from <paramref name="content"/> after the <c>[M00_HandWritten]</c> line,
    /// with that line's own line ending. Returns the new content and the number of lines added (0: content unchanged,
    /// also when the anchor line is missing).
    /// </summary>
    public static (string Content, int Added) Insert(string content, IEnumerable<string> lines)
    {
        int anchor = content.IndexOf(Anchor, StringComparison.Ordinal);
        int lineEnd = anchor < 0 ? -1 : content.IndexOf('\n', anchor);
        if (lineEnd < 0)
        {
            return (content, 0);
        }
        string eol = content[lineEnd - 1] == '\r' ? "\r\n" : "\n";
        StringBuilder added = new();
        int count = 0;
        foreach (string line in lines)
        {
            if (KeyOf(line) is not { } key || HasKey(content, key) || line.Any(character => character > 0xFF))
            {
                continue;
            }
            added.Append(line).Append(eol);
            count++;
        }
        return count == 0 ? (content, 0) : (content.Insert(lineEnd + 1, added.ToString()), count);
    }

    /// <summary>Adds the missing lines to every translated table that exists. Returns the number of lines added.</summary>
    public static int Apply(GamePaths paths) => Apply(paths, EmbeddedLines);

    public static int Apply(GamePaths paths, Func<string, string[]> linesOf)
    {
        int total = 0;
        foreach (string language in Languages)
        {
            string path = FilePath(paths, language);
            if (!File.Exists(path))
            {
                continue;
            }
            string content = Latin1.GetString(File.ReadAllBytes(path));
            (string patched, int added) = Insert(content, linesOf(language));
            if (added == 0)
            {
                continue;
            }
            if (!File.Exists(BackupPath(paths, language)))
            {
                File.Copy(path, BackupPath(paths, language));
            }
            File.WriteAllBytes(path, Latin1.GetBytes(patched));
            total += added;
        }
        return total;
    }

    /// <summary>Puts the original tables back (copies made by <see cref="Apply"/>). Returns the number restored.</summary>
    public static int Restore(GamePaths paths)
    {
        int restored = 0;
        foreach (string language in Languages)
        {
            string backup = BackupPath(paths, language);
            if (File.Exists(backup))
            {
                File.Copy(backup, FilePath(paths, language), overwrite: true);
                File.Delete(backup);
                restored++;
            }
        }
        return restored;
    }

    private static string? KeyOf(string line)
    {
        if (line.Length < 3 || line[0] != '[')
        {
            return null;
        }
        int end = line.IndexOf(']');
        return end > 1 ? line[..(end + 1)] : null;
    }

    // A key counts as present when a line of the table starts with it.
    private static bool HasKey(string content, string key) =>
        content.StartsWith(key, StringComparison.Ordinal) ||
        content.Contains("\n" + key, StringComparison.Ordinal);
}
