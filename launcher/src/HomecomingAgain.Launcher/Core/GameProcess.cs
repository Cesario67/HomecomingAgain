using System.Diagnostics;

namespace HomecomingAgain.Launcher.Core;

/// <summary>
/// Détection et lancement du jeu, sous Windows ou sous Linux (le jeu tourne alors dans Proton, qui est Wine).
/// </summary>
public static class GameProcess
{
    /// <summary>
    /// Sous Wine, notre version.dll n'est chargée que si on préfère la DLL du jeu (« n », native) à celle de Wine
    /// (« b », builtin). Steam transmet cette variable d'environnement au jeu.
    /// </summary>
    public const string DllOverrideVariable = "WINEDLLOVERRIDES";
    public const string VersionDllOverride = "version=n,b";

    /// <summary>Vrai si SilentHill.exe est en cours d'exécution depuis ce dossier.</summary>
    public static bool IsRunning(GamePaths paths) => OperatingSystem.IsWindows() ? IsRunningOnWindows(paths) : IsRunningUnderWine();

    private static bool IsRunningOnWindows(GamePaths paths)
    {
        foreach (Process process in Process.GetProcessesByName(Path.GetFileNameWithoutExtension(GamePaths.GameExeName)))
        {
            using (process)
            {
                try
                {
                    if (string.Equals(process.MainModule?.FileName, paths.GameExe, StringComparison.OrdinalIgnoreCase))
                    {
                        return true;
                    }
                }
                catch (Exception exception) when (exception is InvalidOperationException or System.ComponentModel.Win32Exception)
                {
                    // Processus d'un autre utilisateur ou terminé entre-temps : on considère qu'il peut s'agir du jeu.
                    return true;
                }
            }
        }
        return false;
    }

    // Sous Wine, le processus Linux est wine64-preloader (ou équivalent) : seule sa ligne de commande nomme le jeu,
    // avec un chemin Windows (Z:\home\... ou C:\...) qu'on ne compare donc pas au dossier Linux.
    private static bool IsRunningUnderWine()
    {
        if (!Directory.Exists("/proc"))
        {
            return false;
        }
        foreach (string directory in Directory.EnumerateDirectories("/proc"))
        {
            if (!int.TryParse(Path.GetFileName(directory), out _))
            {
                continue;
            }
            try
            {
                if (CommandLineRunsGame(File.ReadAllText(Path.Combine(directory, "cmdline"))))
                {
                    return true;
                }
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                // Processus terminé entre-temps, ou d'un autre utilisateur : ce n'est pas notre jeu.
            }
        }
        return false;
    }

    /// <summary>
    /// Vrai si une ligne de commande Linux (/proc/&lt;pid&gt;/cmdline : arguments séparés par des zéros) exécute
    /// SilentHill.exe. Ne reconnaît pas celle de Steam ou de Proton qui ne fait que le citer parmi d'autres
    /// arguments : le jeu doit être le premier argument.
    /// </summary>
    public static bool CommandLineRunsGame(string commandLine)
    {
        string first = commandLine.Split('\0', 2)[0];
        int separator = first.LastIndexOfAny(['\\', '/']);
        return string.Equals(first[(separator + 1)..], GamePaths.GameExeName, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>Ajoute <see cref="VersionDllOverride"/> à une valeur de WINEDLLOVERRIDES existante (ou absente).</summary>
    public static string WithVersionOverride(string? existing)
    {
        if (string.IsNullOrWhiteSpace(existing))
        {
            return VersionDllOverride;
        }
        bool alreadySet = existing.Split(';').Any(entry =>
            entry.Split('=')[0].Split(',').Any(name => string.Equals(name.Trim(), "version", StringComparison.OrdinalIgnoreCase)));
        return alreadySet ? existing : $"{existing};{VersionDllOverride}";
    }

    /// <summary>
    /// Ligne à coller dans les options de lancement Steam pour que Steam ouvre ce launcher. Sous Linux, elle force
    /// aussi le chargement de notre version.dll, au cas où le joueur lance le jeu sans passer par le launcher.
    /// </summary>
    public static string SteamLaunchOption(GamePaths paths, bool windows) => windows
        ? $"\"{Path.Combine(paths.BinDirectory, "HomecomingAgain.exe")}\" %command%"
        : $"{DllOverrideVariable}=\"{VersionDllOverride}\" \"{Path.Combine(paths.BinDirectory, "HomecomingAgain")}\" %command%";

    /// <summary>
    /// Lance le jeu. Si le launcher a été démarré par Steam (option de lancement « ... %command% »), on
    /// exécute la commande d'origine de Steam ; sinon SilentHill.exe directement, depuis Bin. Sous Linux, sans
    /// commande de Steam, on demande à Steam de lancer le jeu (lui seul sait le démarrer dans Proton).
    /// </summary>
    public static void Launch(GamePaths paths, IReadOnlyList<string> steamCommand)
    {
        ProcessStartInfo start;
        if (steamCommand.Count > 0)
        {
            start = new ProcessStartInfo(steamCommand[0]) { UseShellExecute = false };
            foreach (string argument in steamCommand.Skip(1))
            {
                start.ArgumentList.Add(argument);
            }
            if (OperatingSystem.IsWindows())
            {
                start.WorkingDirectory = Path.GetDirectoryName(steamCommand[0]) ?? paths.GameDirectory;
            }
            else
            {
                // Le premier argument est un outil de Steam (reaper, environnement d'exécution) : on garde le dossier
                // courant que Steam nous a donné, et on force notre DLL.
                start.Environment[DllOverrideVariable] = WithVersionOverride(Environment.GetEnvironmentVariable(DllOverrideVariable));
            }
        }
        else if (OperatingSystem.IsWindows())
        {
            start = new ProcessStartInfo(paths.GameExe)
            {
                UseShellExecute = false,
                WorkingDirectory = paths.BinDirectory,
            };
        }
        else
        {
            start = new ProcessStartInfo($"steam://rungameid/{GameLocator.SteamAppId}") { UseShellExecute = true };
        }
        Process.Start(start)?.Dispose();
    }

    /// <summary>Ouvre un fichier ou un dossier avec l'application associée (Explorateur, éditeur de texte...).</summary>
    public static void Open(string path)
    {
        Process.Start(new ProcessStartInfo(path) { UseShellExecute = true })?.Dispose();
    }

    /// <summary>
    /// Montre un fichier : l'Explorateur avec le fichier sélectionné sous Windows, son dossier ailleurs (xdg-open ne
    /// sait pas sélectionner un fichier).
    /// </summary>
    public static void Reveal(string path)
    {
        if (OperatingSystem.IsWindows())
        {
            Process.Start(new ProcessStartInfo("explorer.exe", $"/select,\"{path}\"") { UseShellExecute = true })?.Dispose();
        }
        else
        {
            Open(Path.GetDirectoryName(path)!);
        }
    }
}
