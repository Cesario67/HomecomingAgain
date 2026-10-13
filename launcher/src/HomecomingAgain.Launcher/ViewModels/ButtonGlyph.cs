namespace HomecomingAgain.Launcher.ViewModels;

/// <summary>
/// Aperçu d'une icône de bouton de face (A, B, X, Y) dans le launcher : symbole, couleur (#RRGGBB) et, pour les
/// symboles PlayStation, forme dessinée (géométrie de 16 × 16 au format des chemins XAML). Les caractères ✕ ○ □ △
/// n'ont ni la même taille ni la même hauteur selon la police : dessinés, ils restent centrés dans le rond.
/// </summary>
public sealed record ButtonGlyph(string Symbol, string Color, string Shape = "")
{
    public bool HasShape => Shape.Length > 0;

    private static readonly IReadOnlyList<ButtonGlyph> Pc =
        [new("1", "#D23C3C"), new("2", "#D23C3C"), new("3", "#D23C3C"), new("4", "#D23C3C")];

    private static readonly IReadOnlyList<ButtonGlyph> Xbox =
        [new("A", "#3DAA4E"), new("B", "#D9372E"), new("X", "#2F73D8"), new("Y", "#E2B21C")];

    private static readonly IReadOnlyList<ButtonGlyph> PlayStation =
    [
        new("✕", "#7D9BE0", "M3,3 L13,13 M13,3 L3,13"),
        new("○", "#E05A5A", "M8,2.5 A5.5,5.5 0 1 1 7.99,2.5 Z"),
        new("□", "#D888C8", "M3,3 L13,3 L13,13 L3,13 Z"),
        new("△", "#43B794", "M8,2.5 L14,12.5 L2,12.5 Z"),
    ];

    /// <summary>
    /// Icônes affichées en jeu pour un réglage <c>[Input] ButtonIcons</c> (0 auto, 1 PC, 2 Xbox 360, 3 PS3), dans
    /// l'ordre A, B, X, Y. Auto montre les icônes Xbox, celles d'une manette branchée.
    /// </summary>
    public static IReadOnlyList<ButtonGlyph> For(int buttonIcons) => buttonIcons switch
    {
        1 => Pc,
        3 => PlayStation,
        _ => Xbox,
    };
}
