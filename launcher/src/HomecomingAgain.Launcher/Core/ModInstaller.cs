using System.Diagnostics;

namespace HomecomingAgain.Launcher.Core;

/// <summary>État de l'installation du mod dans un dossier de jeu.</summary>
public sealed record InstallStatus(ExeInfo Exe, bool HasOriginalExeBackup, string? InstalledModVersion,
                                   bool HasForeignVersionDll, bool HasGameSettings)
{
    public bool IsModInstalled => InstalledModVersion is not null;
}

/// <summary>
/// Installation, mise à jour et désinstallation de HomecomingAgain. Un seul fichier du jeu est modifié, après copie de
/// l'original : l'en-tête de SilentHill.exe (patch 4 Go, voir <see cref="ExePatcher"/>). Le reste du mod tient dans
/// <c>Bin\version.dll</c> et <c>Bin\HomecomingAgain.ini</c>. Les réglages existants de l'utilisateur ne sont jamais écrasés.
/// </summary>
public static class ModInstaller
{
    private const string SettingsFile = "HomecomingAgain.ini";
    private static readonly string[] LicenseFiles = ["LICENSE.txt", "LICENSE-SMAA.txt", "LICENSE-CAMERAFIX.txt"];

    public static InstallStatus ReadStatus(GamePaths paths) => new(
        ExePatcher.ReadInfo(paths.GameExe),
        File.Exists(paths.OriginalExeBackup),
        ModVersionOf(paths.ModDll),
        File.Exists(paths.ModDll) && ModVersionOf(paths.ModDll) is null,
        File.Exists(paths.GameSettings));

    /// <summary>Le dossier contient-il les fichiers du mod à installer (paquet) ?</summary>
    public static bool IsPackageDirectory(string directory) =>
        ModVersionOf(Path.Combine(directory, GamePaths.ModDllName)) is not null;

    /// <summary>Version de notre version.dll (null si absente ou si c'est une autre DLL, celle de Windows par exemple).</summary>
    public static string? ModVersionOf(string dllPath)
    {
        if (!File.Exists(dllPath))
        {
            return null;
        }
        FileVersionInfo info = FileVersionInfo.GetVersionInfo(dllPath);
        return info.FileDescription?.Contains("Homecoming Again", StringComparison.Ordinal) == true ? info.FileVersion : null;
    }

    /// <summary>Installe ou met à jour la DLL, les réglages absents, les licences et le launcher. Renvoie le compte rendu.</summary>
    public static IReadOnlyList<string> Install(string packageDirectory, GamePaths paths, string? launcherExecutable)
    {
        List<string> report = new();
        ExeInfo exe = ExePatcher.ReadInfo(paths.GameExe);
        if (!paths.ContainsGame || !exe.IsValid || !exe.IsX86)
        {
            throw new InvalidDataException(Loc.Get("InstallInvalidGame"));
        }
        EnsureGameNotRunning(paths);
        if (!exe.IsKnownVersion)
        {
            report.Add(Loc.Get("InstallUnknownVersion"));
        }
        // 4 Go d'espace d'adressage au lieu de 2 : nécessaire aux résolutions élevées et au sur-échantillonnage.
        report.Add(ExePatcher.EnableLargeAddressAware(paths.GameExe)
            ? Loc.Get("InstallLaaApplied")
            : Loc.Get("InstallLaaAlready"));

        string sourceDll = Path.Combine(packageDirectory, GamePaths.ModDllName);
        if (ModVersionOf(sourceDll) is not { } version)
        {
            throw new FileNotFoundException(Loc.Get("InstallDllMissing"), sourceDll);
        }
        // Une version.dll qui n'est pas la nôtre (autre mod) est mise de côté, jamais supprimée.
        if (File.Exists(paths.ModDll) && ModVersionOf(paths.ModDll) is null)
        {
            File.Copy(paths.ModDll, paths.ModDllBackup, overwrite: true);
            report.Add(Loc.Get("InstallDllKept"));
        }
        File.Copy(sourceDll, paths.ModDll, overwrite: true);
        report.Add(Loc.Get("InstallModInstalled", version));

        string sourceIni = Path.Combine(packageDirectory, SettingsFile);
        if (File.Exists(paths.ModIni))
        {
            report.Add(Loc.Get("InstallFileKept", SettingsFile));
        }
        else if (File.Exists(sourceIni))
        {
            File.Copy(sourceIni, paths.ModIni);
            report.Add(Loc.Get("InstallFileInstalled", SettingsFile));
        }

        if (IniDocument.Load(paths.ModIni).GetBool("Compat", "MissingTexts", true))
        {
            int added = TextPatcher.Apply(paths);
            if (added > 0)
            {
                report.Add(Loc.Get("InstallTextsAdded", added));
            }
        }

        foreach (string license in LicenseFiles)
        {
            string source = Path.Combine(packageDirectory, license);
            if (File.Exists(source))
            {
                File.Copy(source, Path.Combine(paths.BinDirectory, "HomecomingAgain-" + license), overwrite: true);
            }
        }

        // Le launcher est aussi copié dans Bin, pour pouvoir servir d'option de lancement Steam.
        if (launcherExecutable is not null && File.Exists(launcherExecutable))
        {
            string target = Path.Combine(paths.BinDirectory, Path.GetFileName(launcherExecutable));
            if (!string.Equals(Path.GetFullPath(launcherExecutable), Path.GetFullPath(target),
                               StringComparison.OrdinalIgnoreCase))
            {
                File.Copy(launcherExecutable, target, overwrite: true);
                report.Add(Loc.Get("InstallLauncherCopied"));
            }
        }
        return report;
    }

    /// <summary>
    /// Retire le mod : notre version.dll est supprimée (une version.dll d'un autre mod mise de côté est remise) et
    /// SilentHill.exe d'origine est restauré. Les réglages, journaux et diagnostics sont conservés.
    /// </summary>
    public static IReadOnlyList<string> Uninstall(GamePaths paths, string? packageDirectory = null)
    {
        List<string> report = new();
        EnsureGameNotRunning(paths);
        if (packageDirectory is not null)
        {
            report.AddRange(DlssInstaller.RemoveAll(packageDirectory, paths));
        }
        if (File.Exists(paths.ModDll) && ModVersionOf(paths.ModDll) is not null)
        {
            File.Delete(paths.ModDll);
            report.Add(Loc.Get("UninstallDllRemoved"));
        }
        if (File.Exists(paths.ModDllBackup))
        {
            File.Move(paths.ModDllBackup, paths.ModDll, overwrite: true);
            report.Add(Loc.Get("UninstallDllRestored"));
        }
        if (File.Exists(paths.OriginalExeBackup))
        {
            ExePatcher.RestoreOriginal(paths.GameExe);
            report.Add(Loc.Get("UninstallExeRestored"));
        }
        if (TextPatcher.Restore(paths) > 0)
        {
            report.Add(Loc.Get("UninstallTextsRestored"));
        }
        // Sans le mod, le stick droit doit de nouveau tourner le personnage.
        if (ControllerBinds.Save(paths, cameraFix: false))
        {
            report.Add(Loc.Get("UninstallBindsRestored"));
        }
        report.Add(Loc.Get("UninstallKept"));
        return report;
    }

    /// <summary>
    /// Enregistre les réglages du jeu (vars_pc.cfg). Avant la première modification, une copie de secours du
    /// fichier d'origine est faite (jamais écrasée ensuite).
    /// </summary>
    public static void SaveGameSettings(GamePaths paths, KeyValueDocument settings)
    {
        EnsureGameNotRunning(paths);
        if (File.Exists(paths.GameSettings) && !File.Exists(paths.GameSettingsBackup))
        {
            File.Copy(paths.GameSettings, paths.GameSettingsBackup);
        }
        settings.Save(paths.GameSettings);
    }

    /// <summary>Commandes du stick droit selon le correctif de caméra (voir <see cref="ControllerBinds"/>).</summary>
    public static void SaveControllerBinds(GamePaths paths, bool cameraFix)
    {
        if (File.Exists(paths.GameBinds) && ControllerBinds.IsApplied(File.ReadAllText(paths.GameBinds)) != cameraFix)
        {
            EnsureGameNotRunning(paths);
            ControllerBinds.Save(paths, cameraFix);
        }
    }

    /// <summary>
    /// Adds the game's missing texts (<paramref name="enabled"/>) or puts the original language files back (see
    /// <see cref="TextPatcher"/>). Does nothing when the game files are already in the requested state.
    /// </summary>
    public static void SaveMissingTexts(GamePaths paths, bool enabled)
    {
        bool backedUp = TextPatcher.Languages.Any(language => File.Exists(TextPatcher.BackupPath(paths, language)));
        bool missing = TextPatcher.Languages.Any(language =>
            File.Exists(TextPatcher.FilePath(paths, language)) &&
            TextPatcher.CountMissing(File.ReadAllText(TextPatcher.FilePath(paths, language), System.Text.Encoding.Latin1),
                                     TextPatcher.EmbeddedLines(language)) > 0);
        if (enabled ? !missing : !backedUp)
        {
            return;
        }
        EnsureGameNotRunning(paths);
        if (enabled)
        {
            TextPatcher.Apply(paths);
        }
        else
        {
            TextPatcher.Restore(paths);
        }
    }

    // Le jeu réécrit vars_pc.cfg à sa fermeture : modifier les réglages pendant qu'il tourne serait perdu.
    private static void EnsureGameNotRunning(GamePaths paths)
    {
        if (GameProcess.IsRunning(paths))
        {
            throw new InvalidOperationException(Loc.Get("GameRunningClose"));
        }
    }
}
