using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HomecomingAgain.Launcher.Core;

namespace HomecomingAgain.Launcher.ViewModels;

/// <summary>Onglet Installation : état, installation / mise à jour, désinstallation, option Steam, diagnostic.</summary>
public sealed partial class StatusViewModel(MainWindowViewModel main, string packageDirectory) : LocalizedViewModel
{
    public bool IsPackage { get; } = ModInstaller.IsPackageDirectory(packageDirectory);

    public string PackageVersion =>
        ModInstaller.ModVersionOf(Path.Combine(packageDirectory, GamePaths.ModDllName)) ?? "?";

    [ObservableProperty]
    public partial string GameVersion { get; set; } = string.Empty;

    [ObservableProperty]
    public partial string MemoryState { get; set; } = string.Empty;

    [ObservableProperty]
    public partial string ModState { get; set; } = string.Empty;

    [ObservableProperty]
    public partial string GameSettingsState { get; set; } = string.Empty;

    public string InstallButtonText => IsPackage ? Loc.Get("InstallButtonVersion", PackageVersion) : Loc.Get("InstallButton");

    public string SteamLaunchOption =>
        main.Paths is { } paths ? $"\"{Path.Combine(paths.BinDirectory, "HomecomingAgain.exe")}\" %command%" : string.Empty;

    /// <summary>Après un changement de langue : le texte du bouton d'installation est recalculé.</summary>
    public void NotifyLanguageChanged() => OnPropertyChanged(nameof(InstallButtonText));

    public void Load(GamePaths paths)
    {
        InstallStatus status = ModInstaller.ReadStatus(paths);
        GameVersion = !status.Exe.IsValid ? Loc.Get("ExeUnreadable")
            : status.Exe.IsKnownVersion ? Loc.Get("ExeKnownVersion")
            : Loc.Get("ExeUnknownVersion", status.Exe.Timestamp.ToString("X8"));
        MemoryState = status.Exe.LargeAddressAware ? Loc.Get("LaaApplied") : Loc.Get("LaaMissing");
        ModState = status.IsModInstalled ? Loc.Get("ModInstalledState", status.InstalledModVersion ?? "?")
            : status.HasForeignVersionDll ? Loc.Get("ModForeignDll")
            : Loc.Get("ModNotInstalled");
        GameSettingsState = status.HasGameSettings ? Loc.Get("GameSettingsFound") : Loc.Get("GameSettingsMissing");
        OnPropertyChanged(nameof(SteamLaunchOption));
    }

    private bool CanInstall() => IsPackage;

    [RelayCommand(CanExecute = nameof(CanInstall))]
    private void Install() => main.Run(() =>
    {
        IReadOnlyList<string> report = ModInstaller.Install(packageDirectory, main.Paths!, Environment.ProcessPath);
        main.Reload();
        return string.Join(" ", report);
    });

    [RelayCommand]
    private async Task UninstallAsync()
    {
        if (!await main.ConfirmAsync(Loc.Get("UninstallConfirm")))
        {
            return;
        }
        main.Run(() =>
        {
            IReadOnlyList<string> report = ModInstaller.Uninstall(main.Paths!, packageDirectory);
            main.Reload();
            return string.Join(" ", report);
        });
    }

    [RelayCommand]
    private void OpenLatestLog() => main.Run(() =>
    {
        string? latest = Directory.Exists(main.Paths!.LogsDirectory)
            ? Directory.GetFiles(main.Paths.LogsDirectory, "*.log").OrderDescending().FirstOrDefault()
            : null;
        if (latest is null)
        {
            return Loc.Get("NoLogYet");
        }
        GameProcess.Open(latest);
        return Loc.Get("LogOpened", Path.GetFileName(latest));
    });

    [RelayCommand]
    private void OpenModFolder() => main.Run(() =>
    {
        Directory.CreateDirectory(main.Paths!.ModDataDirectory);
        GameProcess.Open(main.Paths.ModDataDirectory);
        return string.Empty;
    });
}
