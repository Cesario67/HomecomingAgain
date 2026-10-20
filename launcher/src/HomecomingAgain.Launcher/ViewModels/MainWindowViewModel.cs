using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HomecomingAgain.Launcher.Core;

namespace HomecomingAgain.Launcher.ViewModels;

/// <summary>Fenêtre principale : dossier du jeu, onglets, enregistrement des réglages et lancement.</summary>
public sealed partial class MainWindowViewModel : LocalizedViewModel
{
    private readonly IReadOnlyList<string> _steamCommand;
    private readonly string _packageDirectory;

    /// <param name="launcherDirectory">Dossier du launcher : le jeu y est cherché en premier.</param>
    /// <param name="packageDirectory">Dossier des fichiers du mod à installer (voir <see cref="EmbeddedPackage"/>).</param>
    public MainWindowViewModel(string launcherDirectory, IReadOnlyList<string> steamCommand, string? packageDirectory = null)
    {
        _steamCommand = steamCommand;
        _packageDirectory = packageDirectory ?? launcherDirectory;
        Graphics.DlssAvailable = DlssInstaller.IsAvailable(_packageDirectory, OperatingSystem.IsWindows());
        Graphics.DlssUnsupportedOnLinux = !OperatingSystem.IsWindows();
        SelectedLanguage = Languages.First(choice => choice.Value == Loc.Current);
        Loc.Instance.LanguageChanged += OnLanguageChanged;
        Status = new StatusViewModel(this, packageDirectory ?? launcherDirectory);
        if (GameLocator.FindGameDirectory(launcherDirectory) is { } directory)
        {
            SetGameDirectory(directory);
        }
        else
        {
            Message = Loc.Get("GameNotFound");
        }
    }

    /// <summary>Langues proposées (noms écrits dans leur propre langue, jamais traduits).</summary>
    public IReadOnlyList<Choice<Language>> Languages { get; } =
    [
        new(Language.French, "Français"),
        new(Language.English, "English"),
    ];

    [ObservableProperty]
    public partial Choice<Language> SelectedLanguage { get; set; }

    partial void OnSelectedLanguageChanged(Choice<Language> value) => Loc.SetLanguage(value.Value);

    /// <summary>
    /// Après un changement de langue : les textes lus sur le disque (état, sauvegardes) sont recalculés. Les
    /// réglages en cours d'édition ne sont pas relus, pour ne pas perdre les modifications non enregistrées.
    /// </summary>
    private void OnLanguageChanged()
    {
        Message = string.Empty;
        if (Paths is not null)
        {
            Status.Load(Paths);

        }
        Status.NotifyLanguageChanged();
    }

    public GraphicsViewModel Graphics { get; } = new();
    public PerformanceViewModel Performance { get; } = new();
    public AdvancedViewModel Advanced { get; } = new();
    public StatusViewModel Status { get; }

    public GamePaths? Paths { get; private set; }

    /// <summary>Fourni par la vue : affiche une question oui/non.</summary>
    public Func<string, Task<bool>> ConfirmAsync { get; set; } = _ => Task.FromResult(false);

    /// <summary>
    /// Fourni par la vue, appelé quand le launcher a été démarré par Steam et vient de lancer le jeu : la
    /// fenêtre se cache et l'application attend la fin du jeu. Steam considère en effet que le jeu tourne tant
    /// que le programme qu'il a lancé (ce launcher) tourne : overlay et temps de jeu en dépendent.
    /// </summary>
    public Action WaitForGameThenExit { get; set; } = () => { };

    /// <summary>Attend que SilentHill.exe démarre (jusqu'à une minute), puis qu'il se termine.</summary>
    public async Task WaitForGameAsync()
    {
        if (Paths is null)
        {
            return;
        }
        for (int attempt = 0; attempt < 60 && !GameProcess.IsRunning(Paths); attempt++)
        {
            await Task.Delay(TimeSpan.FromSeconds(1));
        }
        while (GameProcess.IsRunning(Paths))
        {
            await Task.Delay(TimeSpan.FromSeconds(2));
        }
    }

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(HasGame))]
    [NotifyCanExecuteChangedFor(nameof(PlayCommand), nameof(SaveSettingsCommand))]
    public partial string GameDirectory { get; set; } = string.Empty;

    [ObservableProperty]
    public partial string Message { get; set; } = string.Empty;

    public bool HasGame => Paths is not null;

    public bool LaunchedBySteam => _steamCommand.Count > 0;

    /// <summary>Choisit un dossier de jeu (au démarrage ou depuis le bouton « Changer »).</summary>
    public void SetGameDirectory(string directory)
    {
        // Le dossier racine du jeu ou son sous-dossier Bin sont acceptés.
        if (GamePaths.ResolveRoot(directory) is not { } root)
        {
            Message = Loc.Get("ExeNotFoundIn", directory);
            return;
        }
        Paths = new GamePaths(root);
        GameLocator.RememberDirectory(root);
        GameDirectory = root;
        Reload();
        Message = LaunchedBySteam ? Loc.Get("LaunchedBySteam") : string.Empty;
    }

    /// <summary>Relit l'état et les réglages depuis le disque.</summary>
    public void Reload()
    {
        if (Paths is null)
        {
            return;
        }
        Status.Load(Paths);
        PatchSettings settings = PatchSettings.Read(IniDocument.Load(Paths.ModIni), KeyValueDocument.Load(Paths.GameSettings));
        Graphics.Load(settings);
        Performance.Load(settings);
        Advanced.Load(settings);
    }

    /// <summary>Exécute une action en affichant son résultat ou son erreur dans la barre d'état.</summary>
    public void Run(Func<string> action)
    {
        try
        {
            Message = action();
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException
                                              or InvalidOperationException or InvalidDataException
                                              or System.ComponentModel.Win32Exception)
        {
            Message = Loc.Get("ErrorPrefix", exception.Message);
        }
    }

    private bool CanUseGame() => HasGame;

    /// <summary>
    /// Écrit les deux fichiers : HomecomingAgain.ini (mod) et vars_pc.cfg (jeu, copie de secours faite à la première
    /// modification). Si le jeu n'a encore jamais été lancé, vars_pc.cfg n'existe pas : seul le mod est réglé.
    /// </summary>
    private string WriteSettings(PatchSettings settings)
    {
        // Fichiers de l'option DLSS (DXVK, programme DLSS) avant ses réglages : sans eux, Apply=1 ne servirait à rien.
        IReadOnlyList<string> dlss = DlssInstaller.Apply(settings.Dlss, _packageDirectory, Paths!);
        IniDocument mod = IniDocument.Load(Paths!.ModIni);
        settings.WriteMod(mod);
        mod.Save(Paths.ModIni);
        ModInstaller.SaveControllerBinds(Paths, settings.CameraFix);
        if (!File.Exists(Paths.GameSettings))
        {
            return string.Join(" ", dlss.Prepend(Loc.Get("SettingsSavedModOnly")));
        }
        KeyValueDocument game = KeyValueDocument.Load(Paths.GameSettings);
        settings.WriteGame(game);
        ModInstaller.SaveGameSettings(Paths, game);
        return string.Join(" ", dlss.Prepend(Loc.Get("SettingsSaved")));
    }

    private PatchSettings CurrentSettings() => Advanced.ApplyTo(Performance.ApplyTo(Graphics.ApplyTo(new PatchSettings())));

    [RelayCommand(CanExecute = nameof(CanUseGame))]
    private void SaveSettings() => Run(() => WriteSettings(CurrentSettings()));

    [RelayCommand(CanExecute = nameof(CanUseGame))]
    private async Task ResetSettingsAsync()
    {
        bool confirmed = await ConfirmAsync(Loc.Get("ResetConfirm"));
        if (!confirmed)
        {
            return;
        }
        Run(() =>
        {
            WriteSettings(DefaultSettings.For(CurrentSettings()));
            Reload();
            return Loc.Get("ResetDone");
        });
    }

    [RelayCommand(CanExecute = nameof(CanUseGame))]
    private void Play() => Run(() =>
    {
        if (GameProcess.IsRunning(Paths!))
        {
            return Loc.Get("GameAlreadyRunning");
        }
        WriteSettings(CurrentSettings());
        GameProcess.Launch(Paths!, _steamCommand);
        if (LaunchedBySteam)
        {
            WaitForGameThenExit();
        }
        return Loc.Get("GameLaunched");
    });
}
