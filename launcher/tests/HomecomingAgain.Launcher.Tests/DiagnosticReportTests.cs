using System.IO.Compression;
using HomecomingAgain.Launcher.Core;
using Xunit;

namespace HomecomingAgain.Launcher.Tests;

public sealed class DiagnosticReportTests : IDisposable
{
    private readonly string _root = Path.Combine(Path.GetTempPath(), "hca-tests", Guid.NewGuid().ToString("N"));
    private readonly GamePaths _paths;

    public DiagnosticReportTests()
    {
        _paths = new GamePaths(_root);
        Directory.CreateDirectory(_paths.LogsDirectory);
        Directory.CreateDirectory(Path.Combine(_root, "Engine"));
    }

    public void Dispose()
    {
        if (Directory.Exists(_root))
        {
            Directory.Delete(_root, recursive: true);
        }
    }

    private static void WriteAt(string path, string content, DateTime writeTime)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, content);
        File.SetLastWriteTimeUtc(path, writeTime);
    }

    [Fact]
    public void LatestIsTheMostRecentlyWrittenNotTheLastByName()
    {
        // Les anciens journaux SHHFix_ passent devant HomecomingAgain_ par ordre alphabétique.
        WriteAt(Path.Combine(_paths.LogsDirectory, "SHHFix_20261010-105902-659.log"), "ancien", new DateTime(2026, 10, 10));
        WriteAt(Path.Combine(_paths.LogsDirectory, "HomecomingAgain_20261018-133245-902.log"), "récent", new DateTime(2026, 10, 18));

        Assert.Equal("HomecomingAgain_20261018-133245-902.log",
                     Path.GetFileName(DiagnosticReport.Latest(_paths.LogsDirectory, "*.log")));
    }

    [Fact]
    public void LatestIsNullWhenTheFolderIsMissing()
    {
        Assert.Null(DiagnosticReport.Latest(Path.Combine(_root, "absent"), "*.log"));
    }

    [Fact]
    public void NoReportWithoutLogOrMinidump()
    {
        File.WriteAllText(_paths.GameSettings, "god=true");

        Assert.Null(DiagnosticReport.Create(_paths, new DateTime(2026, 10, 18, 14, 0, 0)));
        Assert.False(Directory.Exists(Path.Combine(_paths.ModDataDirectory, DiagnosticReport.ReportsFolderName)));
    }

    [Fact]
    public void ReportHoldsLatestLogLatestDumpAndSettings()
    {
        WriteAt(Path.Combine(_paths.LogsDirectory, "HomecomingAgain_1.log"), "vieux", new DateTime(2026, 10, 1));
        WriteAt(Path.Combine(_paths.LogsDirectory, "HomecomingAgain_2.log"), "journal", new DateTime(2026, 10, 18));
        WriteAt(Path.Combine(_paths.CrashDumpsDirectory, "SHH_a.dmp"), "dump", new DateTime(2026, 10, 18));
        File.WriteAllText(_paths.ModIni, "[Cheats]");
        File.WriteAllText(_paths.GameSettings, "god=true");

        string zipPath = DiagnosticReport.Create(_paths, new DateTime(2026, 10, 18, 14, 5, 9))!;

        Assert.Equal("HomecomingAgain_rapport_20261018-140509.zip", Path.GetFileName(zipPath));
        using ZipArchive zip = ZipFile.OpenRead(zipPath);
        Assert.Equal(["HomecomingAgain.ini", "HomecomingAgain_2.log", "SHH_a.dmp", "vars_pc.cfg"],
                     zip.Entries.Select(entry => entry.Name).Order(StringComparer.Ordinal));
        using StreamReader reader = new(zip.GetEntry("HomecomingAgain_2.log")!.Open());
        Assert.Equal("journal", reader.ReadToEnd());
    }

    [Fact]
    public void ReadsALogTheGameStillHasOpen()
    {
        string log = Path.Combine(_paths.LogsDirectory, "HomecomingAgain_1.log");
        File.WriteAllText(log, "en cours");
        using FileStream held = new(log, FileMode.Open, FileAccess.ReadWrite, FileShare.ReadWrite);

        Assert.NotNull(DiagnosticReport.Create(_paths, new DateTime(2026, 10, 18)));
    }
}
