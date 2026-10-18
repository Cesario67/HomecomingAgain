using System.Runtime.Versioning;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace HomecomingAgain.Launcher.Core;

/// <summary>
/// Retrouve le dossier du jeu : celui du launcher (ou son parent s'il est dans Bin), puis le dernier dossier choisi
/// (mémorisé dans %APPDATA%\HomecomingAgain\game-directory.txt, ~/.config sous Linux), puis les bibliothèques Steam.
/// </summary>
public static partial class GameLocator
{
    public const string SteamAppId = "19000";
    private const string GameFolderName = "Silent Hill Homecoming";

    // Fichier texte d'une ligne (le dossier du jeu) : pas de JSON, dont la sérialisation par réflexion est
    // incompatible avec l'élagage du .exe publié.
    private static string SettingsFile => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "HomecomingAgain", "game-directory.txt");

    public static string? FindGameDirectory(string launcherDirectory)
    {
        if (GamePaths.ResolveRoot(launcherDirectory) is { } fromLauncher)
        {
            return fromLauncher;
        }
        if (LoadRememberedDirectory() is { } remembered && new GamePaths(remembered).ContainsGame)
        {
            return remembered;
        }
        foreach (string library in SteamLibraries())
        {
            string candidate = Path.Combine(library, "steamapps", "common", GameFolderName);
            if (new GamePaths(candidate).ContainsGame)
            {
                return candidate;
            }
        }
        return null;
    }

    public static void RememberDirectory(string gameDirectory)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(SettingsFile)!);
        File.WriteAllText(SettingsFile, gameDirectory);
    }

    private static string? LoadRememberedDirectory() =>
        File.Exists(SettingsFile) ? File.ReadAllText(SettingsFile).Trim() : null;

    // libraryfolders.vdf contient une ligne "path" "D:\\SteamLibrary" par bibliothèque ("/home/..." sous Linux).
    [GeneratedRegex("\"path\"\\s+\"([^\"]+)\"")]
    private static partial Regex LibraryPath();

    /// <summary>Dossiers des bibliothèques listées dans le contenu d'un libraryfolders.vdf.</summary>
    public static IEnumerable<string> ParseLibraryFolders(string vdf) =>
        LibraryPath().Matches(vdf).Select(match => match.Groups[1].Value.Replace(@"\\", @"\"));

    private static IEnumerable<string> SteamLibraries()
    {
        IEnumerable<string> roots = OperatingSystem.IsWindows() ? WindowsSteamRoots() : LinuxSteamRoots();
        HashSet<string> seen = new(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
        foreach (string root in roots)
        {
            if (!Directory.Exists(root))
            {
                continue;
            }
            if (seen.Add(Path.TrimEndingDirectorySeparator(root)))
            {
                yield return root;
            }
            string folders = Path.Combine(root, "steamapps", "libraryfolders.vdf");
            if (!File.Exists(folders))
            {
                continue;
            }
            foreach (string library in ParseLibraryFolders(File.ReadAllText(folders)))
            {
                if (seen.Add(Path.TrimEndingDirectorySeparator(library)))
                {
                    yield return library;
                }
            }
        }
    }

    [SupportedOSPlatform("windows")]
    private static IEnumerable<string> WindowsSteamRoots()
    {
        if (Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", null) is string steam)
        {
            yield return steam.Replace('/', Path.DirectorySeparatorChar);
        }
    }

    // Emplacements habituels de Steam sous Linux : installation native (et son lien ~/.steam/steam), Flatpak.
    private static IEnumerable<string> LinuxSteamRoots()
    {
        string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        yield return Path.Combine(home, ".local", "share", "Steam");
        yield return Path.Combine(home, ".steam", "steam");
        yield return Path.Combine(home, ".var", "app", "com.valvesoftware.Steam", ".local", "share", "Steam");
    }
}
