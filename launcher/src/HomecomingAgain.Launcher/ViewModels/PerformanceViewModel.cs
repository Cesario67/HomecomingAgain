using CommunityToolkit.Mvvm.ComponentModel;
using HomecomingAgain.Launcher.Core;

namespace HomecomingAgain.Launcher.ViewModels;

/// <summary>Onglet Performances et stabilité : cadence, compatibilité et diagnostic (HomecomingAgain.ini).</summary>
public sealed partial class PerformanceViewModel : LocalizedViewModel
{
    public IReadOnlyList<Choice<int>> FrameRates { get; private set; } =
    [
        Choice<int>.Localized(0, "Fps30Original"),
        new(60, "60"),
        new(75, "75"),
        new(120, "120"),
        new(144, "144"),
        new(165, "165"),
    ];

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ShowHighFrameRateWarning))]
    public partial Choice<int> SelectedFrameRate { get; set; }

    /// <summary>Sons envoyés d'un seul côté mélangés vers la stéréo par le mod ([Audio] ChannelFix).</summary>
    [ObservableProperty]
    public partial bool AudioChannelFix { get; set; } = true;

    [ObservableProperty]
    public partial bool PreciseTimer { get; set; } = true;

    /// <summary>Logos de démarrage accélérés jusqu'à l'écran titre ([Movies] SkipLogos).</summary>
    [ObservableProperty]
    public partial bool SkipLogos { get; set; } = true;

    /// <summary>Mode dieu ([Cheats] God).</summary>
    [ObservableProperty]
    public partial bool God { get; set; }

    /// <summary>Munitions illimitées ([Cheats] UnlimitedAmmo).</summary>
    [ObservableProperty]
    public partial bool UnlimitedAmmo { get; set; }

    [ObservableProperty]
    public partial bool WindowThreadLock { get; set; } = true;

    [ObservableProperty]
    public partial bool D3DMultithreaded { get; set; } = true;

    [ObservableProperty]
    public partial bool CrashDumps { get; set; } = true;

    [ObservableProperty]
    public partial bool FullMemoryDumps { get; set; }

    [ObservableProperty]
    public partial bool HangDetection { get; set; } = true;

    [ObservableProperty]
    public partial bool FrameStats { get; set; } = true;

    /// <summary>Au-delà de 60 i/s, le comportement du jeu n'a pas été vérifié.</summary>
    public bool ShowHighFrameRateWarning => SelectedFrameRate.Value > 60;

    public PerformanceViewModel()
    {
        SelectedFrameRate = FrameRates[1];
        Loc.Instance.LanguageChanged += () => FrameRates[0].RefreshLabel();
    }

    public void Load(PatchSettings settings)
    {
        SelectedFrameRate = FrameRates.FirstOrDefault(choice => choice.Value == settings.TargetFps)
                            ?? AddCustomFrameRate(settings.TargetFps);
        AudioChannelFix = settings.AudioChannelFix;
        PreciseTimer = settings.PreciseTimer;
        SkipLogos = settings.SkipLogos;
        God = settings.God;
        UnlimitedAmmo = settings.UnlimitedAmmo;
        WindowThreadLock = settings.WindowThreadLock;
        D3DMultithreaded = settings.D3DMultithreaded;
        CrashDumps = settings.CrashDumps;
        FullMemoryDumps = settings.FullMemoryDumps;
        HangDetection = settings.HangDetection;
        FrameStats = settings.FrameStats;
    }

    public PatchSettings ApplyTo(PatchSettings settings) => settings with
    {
        TargetFps = SelectedFrameRate.Value,
        AudioChannelFix = AudioChannelFix,
        PreciseTimer = PreciseTimer,
        SkipLogos = SkipLogos,
        God = God,
        UnlimitedAmmo = UnlimitedAmmo,
        WindowThreadLock = WindowThreadLock,
        D3DMultithreaded = D3DMultithreaded,
        CrashDumps = CrashDumps,
        FullMemoryDumps = FullMemoryDumps,
        HangDetection = HangDetection,
        FrameStats = FrameStats,
    };

    // Valeur saisie à la main dans HomecomingAgain.ini : proposée telle quelle plutôt que perdue.
    private Choice<int> AddCustomFrameRate(int fps)
    {
        Choice<int> choice = new(fps, fps.ToString());
        FrameRates = [.. FrameRates, choice];
        OnPropertyChanged(nameof(FrameRates));
        return choice;
    }

}
