using System.Text;
using System.Text.RegularExpressions;

namespace HomecomingAgain.Launcher.Core;

/// <summary>
/// Commandes de la manette dans <c>Engine\binds_pc_mjs.cfg</c>, nécessaires au correctif de caméra ([Camera] CameraFix,
/// porté du Camera Fix d'Emertels). Dans le jeu d'origine, le stick droit tourne aussi le personnage
/// (<c>setbind 1 COMMAND_TURNLEFT CONTROLLER 0 AXIS_RX ...</c>) ; avec le correctif, c'est la caméra qui le fait
/// tourner, et il tournerait deux fois. Ces deux lignes sont donc neutralisées (<c>DEVICE_INVALID</c>, comme le
/// demande le Camera Fix), et la ligne d'origine est gardée en commentaire juste au-dessus pour la remettre quand le
/// correctif est désactivé. Les touches du clavier (<c>addbind</c>) et le reste du fichier ne changent pas.
/// </summary>
public static partial class ControllerBinds
{
    // Commentaire placé au-dessus d'une ligne neutralisée : il contient la ligne d'origine.
    private const string Marker = "#hca-camera-fix ";

    [GeneratedRegex(@"^\s*setbind\s+1\s+(COMMAND_TURNLEFT|COMMAND_TURNRIGHT)\s+CONTROLLER\s+0\s+AXIS_RX\b",
                    RegexOptions.IgnoreCase)]
    private static partial Regex StickTurnLine();

    [GeneratedRegex(@"^\s*setbind\s+1\s+(COMMAND_TURNLEFT|COMMAND_TURNRIGHT)\s+DEVICE_INVALID\b", RegexOptions.IgnoreCase)]
    private static partial Regex DisabledTurnLine();

    /// <summary>Contenu du fichier avec le stick droit libéré (correctif actif) ou remis comme à l'origine.</summary>
    public static string Apply(string content, bool cameraFix)
    {
        string newline = content.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
        List<string> lines = content.Replace("\r\n", "\n").Split('\n').ToList();
        List<string> result = new(lines.Count + 2);
        for (int i = 0; i < lines.Count; i++)
        {
            string line = lines[i];
            if (line.StartsWith(Marker, StringComparison.Ordinal))
            {
                string original = line[Marker.Length..];
                bool nextIsDisabled = i + 1 < lines.Count && DisabledTurnLine().IsMatch(lines[i + 1]);
                if (cameraFix)
                {
                    // Déjà appliqué : la ligne neutralisée est gardée telle quelle (ou refaite si elle manque).
                    result.Add(line);
                    result.Add(nextIsDisabled ? lines[i + 1] : DisabledFrom(original));
                }
                else
                {
                    result.Add(original);
                }
                if (nextIsDisabled)
                {
                    i++;
                }
                continue;
            }
            if (cameraFix && StickTurnLine().Match(line) is { Success: true } match)
            {
                result.Add(Marker + line);
                result.Add(Disabled(match.Groups[1].Value));
                continue;
            }
            result.Add(line);
        }
        return string.Join(newline, result);
    }

    /// <summary>Le stick droit est-il libéré dans ce contenu ?</summary>
    public static bool IsApplied(string content) =>
        content.Replace("\r\n", "\n").Split('\n').Any(line => line.StartsWith(Marker, StringComparison.Ordinal));

    /// <summary>
    /// Applique <see cref="Apply"/> au fichier du jeu s'il existe, avec une copie de secours avant la première
    /// modification (jamais écrasée ensuite). Renvoie true si le fichier a changé.
    /// </summary>
    public static bool Save(GamePaths paths, bool cameraFix)
    {
        if (!File.Exists(paths.GameBinds))
        {
            return false;
        }
        string content = File.ReadAllText(paths.GameBinds, Encoding.ASCII);
        string updated = Apply(content, cameraFix);
        if (updated == content)
        {
            return false;
        }
        if (!File.Exists(paths.GameBindsBackup))
        {
            File.Copy(paths.GameBinds, paths.GameBindsBackup);
        }
        File.WriteAllText(paths.GameBinds, updated, Encoding.ASCII);
        return true;
    }

    private static string DisabledFrom(string original) =>
        StickTurnLine().Match(original) is { Success: true } match ? Disabled(match.Groups[1].Value) : original;

    private static string Disabled(string command) =>
        $"setbind 1 {command.ToUpperInvariant()} DEVICE_INVALID 0 0";
}
