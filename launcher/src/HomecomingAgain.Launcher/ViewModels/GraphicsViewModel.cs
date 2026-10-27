using CommunityToolkit.Mvvm.ComponentModel;
using HomecomingAgain.Launcher.Core;

namespace HomecomingAgain.Launcher.ViewModels;

/// <summary>Onglet Graphismes : affichage et qualité du jeu (vars_pc.cfg), SMAA, filtrage, SSAO et ombres du mod (HomecomingAgain.ini).</summary>
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

    public IReadOnlyList<Choice<int>> SsaoStrengths { get; } =
    [
        Choice<int>.Localized(50, "SsaoLight"),
        Choice<int>.Localized(100, "SsaoNormal"),
        Choice<int>.Localized(150, "SsaoStrong"),
    ];

    public IReadOnlyList<Choice<int>> ShadowResolutions { get; } =
    [
        Choice<int>.Localized(2048, "ShadowOriginal"),
        new(4096, "4096 × 4096"),
        new(8192, "8192 × 8192"),
    ];

    public IReadOnlyList<Choice<int>> FilmGrainLevels { get; } =
    [
        Choice<int>.Localized(0, "GrainOff"),
        Choice<int>.Localized(50, "GrainHalf"),
        Choice<int>.Localized(100, "GrainOriginal"),
    ];

    /// <summary>Icônes de boutons à l'écran ([Input] ButtonIcons : 0 auto, 1 PC, 2 Xbox 360, 3 PS3).</summary>
    public IReadOnlyList<Choice<int>> ButtonIconSets { get; } =
    [
        Choice<int>.Localized(0, "IconsAuto"),
        Choice<int>.Localized(1, "IconsPc"),
        new(2, "Xbox (A, B, X, Y)"),
        Choice<int>.Localized(3, "IconsPlayStation"),
    ];

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ButtonIconPreview))]
    public partial Choice<int> SelectedButtonIcons { get; set; }

    /// <summary>Aperçu des icônes A, B, X, Y du choix courant.</summary>
    public IReadOnlyList<ButtonGlyph> ButtonIconPreview => ButtonGlyph.For(SelectedButtonIcons.Value);

    public IReadOnlyList<Choice<(int Width, int Height)>> Resolutions { get; private set; } = BuildResolutions(1920, 1080);

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ShowFullscreenWarning))]
    public partial Choice<DisplayMode> SelectedDisplayMode { get; set; }

    [ObservableProperty]
    public partial Choice<(int Width, int Height)> SelectedResolution { get; set; }

    [ObservableProperty]
    public partial bool Smaa { get; set; } = true;

    /// <summary>DLSS en mode DLAA (NVIDIA RTX, DXVK installé avec l'option).</summary>
    [ObservableProperty]
    public partial bool Dlss { get; set; }

    /// <summary>Le launcher contient-il les fichiers de l'option DLSS ? Sinon la case est désactivée.</summary>
    [ObservableProperty]
    public partial bool DlssAvailable { get; set; }

    /// <summary>Sous Linux (Proton), la case est grisée avec une explication (voir DlssInstaller.IsAvailable).</summary>
    [ObservableProperty]
    public partial bool DlssUnsupportedOnLinux { get; set; }

    [ObservableProperty]
    public partial Choice<int> SelectedAnisotropy { get; set; }

    [ObservableProperty]
    public partial bool Ssao { get; set; } = true;

    [ObservableProperty]
    public partial Choice<int> SelectedSsaoStrength { get; set; }

    [ObservableProperty]
    public partial Choice<int> SelectedShadowResolution { get; set; }

    [ObservableProperty]
    public partial Choice<int> SelectedFilmGrain { get; set; }

    /// <summary>Correctif de caméra à la manette ([Camera] CameraFix et commandes du stick droit).</summary>
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(OrbitOptionsEnabled))]
    public partial bool CameraFix { get; set; }

    /// <summary>Caméra orbitale ([Camera] Mode=Orbit), expérimentale.</summary>
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(OrbitOptionsEnabled))]
    public partial bool CameraOrbit { get; set; }

    /// <summary>
    /// Les options de l'orbite (L2...) ne servent que si le correctif de caméra ET l'orbite sont cochés : la DLL ignore
    /// le mode orbite sans le correctif, mais la case de l'orbite garde sa valeur quand elle est grisée.
    /// </summary>
    public bool OrbitOptionsEnabled => CameraFix && CameraOrbit;

    /// <summary>Orbite suspendue tant que la gâchette gauche (L2) est tenue ([Camera] OrbitGuardFollow).</summary>
    [ObservableProperty]
    public partial bool OrbitGuardFollow { get; set; } = true;

    /// <summary>Vitesses de la caméra ([Camera]) : de 0,25 à 3, 1 = calibrage d'origine.</summary>
    [ObservableProperty]
    public partial double CameraHorizontalSpeed { get; set; } = 1.0;

    [ObservableProperty]
    public partial double CameraVerticalSpeed { get; set; } = 1.0;

    [ObservableProperty]
    public partial double CameraAimHorizontalSpeed { get; set; } = 1.0;

    [ObservableProperty]
    public partial double CameraAimVerticalSpeed { get; set; } = 1.0;

    /// <summary>Carte et interface étendues au-delà de 720p ([Graphics] FullScreenUiClip).</summary>
    [ObservableProperty]
    public partial bool FullScreenUiClip { get; set; } = true;

    /// <summary>Perspective de la vue d'inspection des objets rétablie ([Graphics] ObjectViewPerspective).</summary>
    [ObservableProperty]
    public partial bool ObjectViewPerspective { get; set; } = true;

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
        SelectedSsaoStrength = SsaoStrengths[1];
        SelectedShadowResolution = ShadowResolutions[^1];
        SelectedFilmGrain = FilmGrainLevels[^1];
        SelectedButtonIcons = ButtonIconSets[0];
        Loc.Instance.LanguageChanged += RefreshLabels;
    }

    private void RefreshLabels()
    {
        foreach (ChoiceBase choice in DisplayModes.Cast<ChoiceBase>().Concat(AnisotropyLevels).Concat(SsaoStrengths).Concat(ShadowResolutions).Concat(FilmGrainLevels).Concat(ButtonIconSets))
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
        Dlss = settings.Dlss;
        SelectedAnisotropy = AnisotropyLevels.LastOrDefault(choice => choice.Value <= settings.Anisotropy) ?? AnisotropyLevels[0];
        Ssao = settings.Ssao;
        SelectedSsaoStrength = SsaoStrengths.LastOrDefault(choice => choice.Value <= settings.SsaoStrength) ?? SsaoStrengths[0];
        SelectedShadowResolution = ShadowResolutions.LastOrDefault(choice => choice.Value <= settings.ShadowResolution) ?? ShadowResolutions[0];
        SelectedFilmGrain = FilmGrainLevels.LastOrDefault(choice => choice.Value <= settings.FilmGrain) ?? FilmGrainLevels[0];
        SelectedButtonIcons = ButtonIconSets.FirstOrDefault(choice => choice.Value == settings.ButtonIcons) ?? ButtonIconSets[0];
        CameraFix = settings.CameraFix;
        CameraOrbit = settings.CameraOrbit;
        OrbitGuardFollow = settings.OrbitGuardFollow;
        CameraHorizontalSpeed = settings.CameraHorizontalSpeed;
        CameraVerticalSpeed = settings.CameraVerticalSpeed;
        CameraAimHorizontalSpeed = settings.CameraAimHorizontalSpeed;
        CameraAimVerticalSpeed = settings.CameraAimVerticalSpeed;
        FullScreenUiClip = settings.FullScreenUiClip;
        ObjectViewPerspective = settings.ObjectViewPerspective;
        HighQualityShaders = settings.HighQualityShaders;
        FullResolutionTextures = settings.FullResolutionTextures;
    }

    public PatchSettings ApplyTo(PatchSettings settings) => settings with
    {
        DisplayMode = SelectedDisplayMode.Value,
        Width = SelectedResolution.Value.Width,
        Height = SelectedResolution.Value.Height,
        Smaa = Smaa,
        Dlss = Dlss && DlssAvailable,
        Anisotropy = SelectedAnisotropy.Value,
        Ssao = Ssao,
        SsaoStrength = SelectedSsaoStrength.Value,
        ShadowResolution = SelectedShadowResolution.Value,
        FilmGrain = SelectedFilmGrain.Value,
        ButtonIcons = SelectedButtonIcons.Value,
        CameraFix = CameraFix,
        CameraOrbit = CameraOrbit,
        OrbitGuardFollow = OrbitGuardFollow,
        CameraHorizontalSpeed = PatchSettings.ClampSpeed(CameraHorizontalSpeed),
        CameraVerticalSpeed = PatchSettings.ClampSpeed(CameraVerticalSpeed),
        CameraAimHorizontalSpeed = PatchSettings.ClampSpeed(CameraAimHorizontalSpeed),
        CameraAimVerticalSpeed = PatchSettings.ClampSpeed(CameraAimVerticalSpeed),
        FullScreenUiClip = FullScreenUiClip,
        ObjectViewPerspective = ObjectViewPerspective,
        HighQualityShaders = HighQualityShaders,
        FullResolutionTextures = FullResolutionTextures,
    };

    private static IReadOnlyList<Choice<(int Width, int Height)>> BuildResolutions(int currentWidth, int currentHeight)
    {
        List<(int Width, int Height)> sizes =
            [(1280, 720), (1600, 900), (1920, 1080),
             // Common ultrawide sizes: 21:9 (2560x1080, 3440x1440, 3840x1600, 5120x2160) and 32:9 (3840x1080, 5120x1440).
             (2560, 1080), (2560, 1440), (3440, 1440), (3840, 1080), (3840, 1600), (3840, 2160), (5120, 1440), (5120, 2160)];
        if (!sizes.Contains((currentWidth, currentHeight)))
        {
            sizes.Add((currentWidth, currentHeight));
        }
        return sizes.OrderBy(size => size.Width * size.Height)
                    .Select(size => new Choice<(int Width, int Height)>(size, $"{size.Width} × {size.Height}"))
                    .ToList();
    }
}
