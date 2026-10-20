using CommunityToolkit.Mvvm.ComponentModel;
using HomecomingAgain.Launcher.Core;

namespace HomecomingAgain.Launcher.ViewModels;

/// <summary>
/// Onglet Avancé : les réglages de HomecomingAgain.ini qui s'adressent à ceux qui savent ce qu'ils font (réglage
/// fin de la SSAO, ordre des canaux, nombre de journaux conservés...). Les nombres sont <c>decimal?</c> parce que le
/// champ numérique d'Avalonia est nul quand le joueur efface le texte : la valeur précédente est alors gardée.
/// </summary>
public sealed partial class AdvancedViewModel : LocalizedViewModel
{
    private readonly PatchSettings _defaults = new();

    [ObservableProperty]
    public partial decimal? SsaoRadius { get; set; } = 1.5m;

    [ObservableProperty]
    public partial decimal? SsaoFadeStart { get; set; } = 150;

    [ObservableProperty]
    public partial decimal? SsaoFadeEnd { get; set; } = 450;

    [ObservableProperty]
    public partial bool D3D9On12 { get; set; }

    /// <summary>Ordre des six canaux du moteur ([Audio] ChannelOrder), par exemple 2,4,1,3,5,0.</summary>
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ChannelOrderInvalid))]
    public partial string ChannelOrder { get; set; } = "2,4,1,3,5,0";

    /// <summary>Un ordre invalide est remplacé par celui par défaut à l'enregistrement.</summary>
    public bool ChannelOrderInvalid => !PatchSettings.IsValidChannelOrder(ChannelOrder);

    /// <summary>Code de touche Windows pour passer les vidéos ([Movies] SkipKey), 0 = aucune.</summary>
    [ObservableProperty]
    public partial decimal? SkipKey { get; set; } = 46;

    [ObservableProperty]
    public partial decimal? ReportIntervalSeconds { get; set; } = 10;

    [ObservableProperty]
    public partial decimal? DumpKeepCount { get; set; } = 10;

    [ObservableProperty]
    public partial decimal? LogKeepCount { get; set; } = 30;

    public void Load(PatchSettings settings)
    {
        SsaoRadius = (decimal)settings.SsaoRadius;
        SsaoFadeStart = settings.SsaoFadeStart;
        SsaoFadeEnd = settings.SsaoFadeEnd;
        D3D9On12 = settings.D3D9On12;
        ChannelOrder = settings.AudioChannelOrder;
        SkipKey = settings.SkipKey;
        ReportIntervalSeconds = settings.ReportIntervalSeconds;
        DumpKeepCount = settings.DumpKeepCount;
        LogKeepCount = settings.LogKeepCount;
    }

    public PatchSettings ApplyTo(PatchSettings settings) => settings with
    {
        SsaoRadius = SsaoRadius is { } radius ? (double)radius : _defaults.SsaoRadius,
        SsaoFadeStart = ToInt(SsaoFadeStart, _defaults.SsaoFadeStart),
        SsaoFadeEnd = ToInt(SsaoFadeEnd, _defaults.SsaoFadeEnd),
        D3D9On12 = D3D9On12,
        AudioChannelOrder = PatchSettings.IsValidChannelOrder(ChannelOrder) ? ChannelOrder.Trim() : _defaults.AudioChannelOrder,
        SkipKey = ToInt(SkipKey, _defaults.SkipKey),
        ReportIntervalSeconds = ToInt(ReportIntervalSeconds, _defaults.ReportIntervalSeconds),
        DumpKeepCount = ToInt(DumpKeepCount, _defaults.DumpKeepCount),
        LogKeepCount = ToInt(LogKeepCount, _defaults.LogKeepCount),
    };

    private static int ToInt(decimal? value, int fallback) => value is { } number ? (int)Math.Round(number) : fallback;
}
