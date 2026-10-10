namespace SHHFix.Launcher.Core;

/// <summary>
/// Tous les chemins utiles d'une installation du jeu, calculés depuis son dossier racine (celui qui contient
/// <c>Bin</c> et <c>Engine</c>). Le mod s'installe dans <c>Bin</c>, à côté du lanceur SilentHill.exe.
/// </summary>
public sealed record GamePaths(string GameDirectory)
{
    public const string GameExeName = "SilentHill.exe";
    public const string ModDllName = "version.dll";

    public string BinDirectory => Path.Combine(GameDirectory, "Bin");
    public string GameExe => Path.Combine(BinDirectory, GameExeName);
    public string OriginalExeBackup => GameExe + ".shhf-original";
    public string ModDll => Path.Combine(BinDirectory, ModDllName);
    public string ModDllBackup => ModDll + ".avant-shhf";
    public string ModIni => Path.Combine(BinDirectory, "SHHFix.ini");

    /// <summary>Réglages du jeu (résolution, plein écran, qualité), créés par le jeu à son premier lancement.</summary>
    public string GameSettings => Path.Combine(GameDirectory, "Engine", "vars_pc.cfg");
    public string GameSettingsBackup => GameSettings + ".shhf-backup";

    public string ModDataDirectory => Path.Combine(BinDirectory, "SHHFix");
    public string LogsDirectory => Path.Combine(ModDataDirectory, "logs");
    public string CrashDumpsDirectory => Path.Combine(ModDataDirectory, "crashdumps");

    public bool ContainsGame => File.Exists(GameExe);

    /// <summary>
    /// Dossier racine du jeu à partir d'un dossier choisi par l'utilisateur : la racine elle-même, ou son
    /// sous-dossier Bin (là où se trouve SilentHill.exe). Null si aucun des deux ne contient le jeu.
    /// </summary>
    public static string? ResolveRoot(string directory)
    {
        if (new GamePaths(directory).ContainsGame)
        {
            return directory;
        }
        string? parent = Path.GetDirectoryName(Path.TrimEndingDirectorySeparator(directory));
        return parent is not null && string.Equals(Path.GetFileName(Path.TrimEndingDirectorySeparator(directory)), "Bin",
                                                    StringComparison.OrdinalIgnoreCase) &&
               new GamePaths(parent).ContainsGame
            ? parent
            : null;
    }
}
