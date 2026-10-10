namespace SHHFix.Launcher.Core;

/// <summary>
/// Remet tous les réglages du launcher à leurs valeurs par défaut (celles de la DLL), sauf l'affichage choisi par
/// le joueur (mode et résolution), qui dépend de son écran. Les fichiers sont modifiés en place.
/// </summary>
public static class DefaultSettings
{
    public static PatchSettings For(PatchSettings current) => new()
    {
        DisplayMode = current.DisplayMode,
        Width = current.Width,
        Height = current.Height,
    };
}
