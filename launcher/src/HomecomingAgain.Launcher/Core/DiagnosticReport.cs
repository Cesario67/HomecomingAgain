using System.IO.Compression;

namespace HomecomingAgain.Launcher.Core;

/// <summary>
/// Rapport de bogue en un clic : un zip avec le dernier journal du mod, le dernier minidump s'il y en a un, et les
/// réglages (mod, jeu, touches) qui suffisent d'habitude à reproduire un problème. Rien n'est supprimé ni modifié.
/// </summary>
public static class DiagnosticReport
{
    public const string ReportsFolderName = "rapports";

    /// <summary>Fichier le plus récemment écrit (et non le dernier par ordre alphabétique : d'anciens noms coexistent).</summary>
    public static string? Latest(string directory, string pattern) =>
        Directory.Exists(directory)
            ? new DirectoryInfo(directory).GetFiles(pattern).OrderByDescending(file => file.LastWriteTimeUtc)
                                          .FirstOrDefault()?.FullName
            : null;

    /// <summary>Fichiers mis dans le rapport : seulement ceux qui existent.</summary>
    public static IReadOnlyList<string> CollectFiles(GamePaths paths)
    {
        string?[] candidates =
        [
            Latest(paths.LogsDirectory, "*.log"),
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
        if (Latest(paths.LogsDirectory, "*.log") is null && Latest(paths.CrashDumpsDirectory, "*.dmp") is null)
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
