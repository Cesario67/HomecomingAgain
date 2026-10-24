using System.IO.Compression;

namespace HomecomingAgain.Launcher.Core;

/// <summary>
/// Rapport de bogue en un clic : un zip avec le dernier journal du mod (et celui du DLSS), le dernier minidump, et les
/// réglages (mod, jeu, touches) qui suffisent d'habitude à reproduire un problème. Rien n'est supprimé ni modifié.
/// </summary>
public static class DiagnosticReport
{
    public const string ReportsFolderName = "rapports";

    // Journaux du mod (un par lancement). Le dossier contient aussi le journal du programme DLSS, souvent écrit en
    // dernier, et d'anciens journaux SHHFix_ : un simple *.log les prenait (constaté le 18/10/2026).
    private const string ModLogPattern = "HomecomingAgain_*.log";
    private const string DlssLogName = "HomecomingAgain.Dlss.log";

    /// <summary>
    /// Fichier non vide le plus récemment écrit (et non le dernier par ordre alphabétique : d'anciens noms coexistent).
    /// Un minidump raté reste à 0 octet (constaté le 18/10/2026, juste après un vrai) : il est ignoré.
    /// </summary>
    public static string? Latest(string directory, string pattern) =>
        Directory.Exists(directory)
            ? new DirectoryInfo(directory).GetFiles(pattern).Where(file => file.Length > 0)
                                          .OrderByDescending(file => file.LastWriteTimeUtc).FirstOrDefault()?.FullName
            : null;

    /// <summary>Dernier journal du mod.</summary>
    public static string? LatestModLog(GamePaths paths) => Latest(paths.LogsDirectory, ModLogPattern);

    /// <summary>Fichiers mis dans le rapport : seulement ceux qui existent.</summary>
    public static IReadOnlyList<string> CollectFiles(GamePaths paths)
    {
        string?[] candidates =
        [
            LatestModLog(paths),
            Path.Combine(paths.LogsDirectory, DlssLogName),
            Latest(paths.CrashDumpsDirectory, "*.dmp"),
            paths.ModIni,
            paths.GameSettings,
            paths.GameBinds,
        ];
        return candidates.OfType<string>().Where(File.Exists).ToList();
    }

    /// <summary>
    /// Crée le zip dans <c>Bin\HomecomingAgain\rapports</c> et renvoie son chemin, ou null s'il n'y a ni journal ni
    /// minidump (le rapport n'aurait alors rien d'utile).
    /// </summary>
    public static string? Create(GamePaths paths, DateTime now)
    {
        IReadOnlyList<string> files = CollectFiles(paths);
        if (LatestModLog(paths) is null && Latest(paths.CrashDumpsDirectory, "*.dmp") is null)
        {
            return null;
        }

        string directory = Path.Combine(paths.ModDataDirectory, ReportsFolderName);
        Directory.CreateDirectory(directory);
        string zipPath = Path.Combine(directory, $"HomecomingAgain_rapport_{now:yyyyMMdd-HHmmss}.zip");
        using ZipArchive zip = ZipFile.Open(zipPath, ZipArchiveMode.Create);
        foreach (string file in files)
        {
            // Le jeu peut tourner et garder son journal ouvert en écriture : lecture partagée.
            using FileStream source = new(file, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            using Stream entry = zip.CreateEntry(Path.GetFileName(file), CompressionLevel.Optimal).Open();
            source.CopyTo(entry);
        }
        return zipPath;
    }
}
