using System.Runtime.InteropServices;
using Avalonia;
using SHHFix.Launcher.Core;

namespace SHHFix.Launcher;

internal static partial class Program
{
    /// <summary>Commande transmise par Steam (option de lancement « "...\SHHFix.exe" %command% »).</summary>
    public static IReadOnlyList<string> SteamCommand { get; private set; } = [];

    [STAThread]
    public static int Main(string[] args)
    {
        if (args.Length > 0 && args[0] == "install")
        {
            return RunInstallCommand(args.Skip(1).ToArray());
        }
        SteamCommand = args;
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
        return 0;
    }

    public static AppBuilder BuildAvaloniaApp() =>
        AppBuilder.Configure<App>().UsePlatformDetect().LogToTrace();

    /// <summary>
    /// Mode ligne de commande, sans fenêtre (tests, automatisation) :
    /// SHHFix.exe install "&lt;dossier du jeu, son dossier Bin ou SilentHill.exe&gt;"
    /// Code de sortie : 0 = succès, 1 = échec, 2 = usage incorrect.
    /// </summary>
    private static int RunInstallCommand(string[] args)
    {
        AttachToParentConsole();
        string? target = args.FirstOrDefault(argument => !argument.StartsWith("--", StringComparison.Ordinal));
        if (target is null)
        {
            Console.Error.WriteLine(Loc.Get("CliUsage"));
            return 2;
        }
        string directory = Directory.Exists(target) ? target : Path.GetDirectoryName(Path.GetFullPath(target))!;
        if (GamePaths.ResolveRoot(Path.GetFullPath(directory)) is not { } root)
        {
            Console.Error.WriteLine(Loc.Get("CliNotGame", target));
            return 1;
        }
        try
        {
            GamePaths paths = new(root);
            foreach (string line in ModInstaller.Install(EmbeddedPackage.Resolve(AppContext.BaseDirectory), paths, Environment.ProcessPath))
            {
                Console.WriteLine(line);
            }
            return 0;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException
                                              or InvalidOperationException or InvalidDataException)
        {
            Console.Error.WriteLine(Loc.Get("CliFailure", exception.Message));
            return 1;
        }
    }

    // Une application fenêtrée n'a pas de console : on se rattache à celle du terminal appelant, s'il y en a une.
    private static void AttachToParentConsole()
    {
        if (OperatingSystem.IsWindows())
        {
            AttachConsole(-1);
        }
    }

    [LibraryImport("kernel32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static partial bool AttachConsole(int processId);
}
