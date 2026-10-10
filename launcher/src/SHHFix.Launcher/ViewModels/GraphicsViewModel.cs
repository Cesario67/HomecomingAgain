using CommunityToolkit.Mvvm.ComponentModel;
using SHHFix.Launcher.Core;

namespace SHHFix.Launcher.ViewModels;

/// <summary>Onglet Graphismes : affichage et qualité du jeu (vars_pc.cfg), SMAA et filtrage du mod (SHHFix.ini).</summary>
public sealed partial class GraphicsViewModel : LocalizedViewModel
{
    public IReadOnlyList<Choice<DisplayMode>> DisplayModes { get; } =
    [
        Choice<DisplayMode>.Localized(DisplayMode.Borderless, "ModeBorderless"),
        Choice<DisplayMode>.Localized(DisplayMode.Fullscreen, "ModeFullscreen"),
        Choice<DisplayMode>.Localized(DisplayMode.Windowed, "ModeWindowed"),
    ];

    public IReadOnlyList<Choice<int>> AnisotropyLevels { get; } =
    [
        Choice<int>.Localized(0, "AnisoOff"),
        new(2, "2x"),
        new(4, "4x"),
        new(8, "8x"),
        new(16, "16x"),
    ];

    public IReadOnlyList<Choice<(int Width, int Height)>> Resolutions { get; private set; } = BuildResolutions(1920, 1080);

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ShowFullscreenWarning))]
    public partial Choice<DisplayMode> SelectedDisplayMode { get; set; }

    [ObservableProperty]
    public partial Choice<(int Width, int Height)> SelectedResolution { get; set; }

    [ObservableProperty]
    public partial bool Smaa { get; set; } = true;

    [ObservableProperty]
    public partial Choice<int> SelectedAnisotropy { get; set; }

    [ObservableProperty]
    public partial bool HighQualityShaders { get; set; }

    [ObservableProperty]
    public partial bool FullResolutionTextures { get; set; }

    /// <summary>Le plein écran exclusif expose au plantage à l'Alt-Tab du jeu d'origine.</summary>
    public bool ShowFullscreenWarning => SelectedDisplayMode.Value == DisplayMode.Fullscreen;

    public GraphicsViewModel()
    {
        SelectedDisplayMode = DisplayModes[0];
        SelectedResolution = Resolutions.First(choice => choice.Value == (1920, 1080));
        SelectedAnisotropy = AnisotropyLevels[^1];
        Loc.Instance.LanguageChanged += RefreshLabels;
    }

    private void RefreshLabels()
    {
        foreach (ChoiceBase choice in DisplayModes.Cast<ChoiceBase>().Concat(AnisotropyLevels))
        {
            choice.RefreshLabel();
        }
    }

    public void Load(PatchSettings settings)
    {
        Resolutions = BuildResolutions(settings.Width, settings.Height);
        OnPropertyChanged(nameof(Resolutions));
        SelectedDisplayMode = DisplayModes.First(choice => choice.Value == settings.DisplayMode);
        SelectedResolution = Resolutions.First(choice => choice.Value == (settings.Width, settings.Height));
        Smaa = settings.Smaa;
        SelectedAnisotropy = AnisotropyLevels.LastOrDefault(choice => choice.Value <= settings.Anisotropy) ?? AnisotropyLevels[0];
        HighQualityShaders = settings.HighQualityShaders;
        FullResolutionTextures = settings.FullResolutionTextures;
    }

    public PatchSettings ApplyTo(PatchSettings settings) => settings with
    {
        DisplayMode = SelectedDisplayMode.Value,
        Width = SelectedResolution.Value.Width,
        Height = SelectedResolution.Value.Height,
        Smaa = Smaa,
        Anisotropy = SelectedAnisotropy.Value,
        HighQualityShaders = HighQualityShaders,
        FullResolutionTextures = FullResolutionTextures,
    };

    private static IReadOnlyList<Choice<(int Width, int Height)>> BuildResolutions(int currentWidth, int currentHeight)
    {
        List<(int Width, int Height)> sizes =
            [(1280, 720), (1600, 900), (1920, 1080), (2560, 1080), (2560, 1440), (3440, 1440), (3840, 2160)];
        if (!sizes.Contains((currentWidth, currentHeight)))
        {
            sizes.Add((currentWidth, currentHeight));
        }
        return sizes.OrderBy(size => size.Width * size.Height)
                    .Select(size => new Choice<(int Width, int Height)>(size, $"{size.Width} × {size.Height}"))
                    .ToList();
    }
}
