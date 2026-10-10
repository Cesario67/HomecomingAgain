using System.Text;

namespace HomecomingAgain.Launcher.Core;

/// <summary>
/// Fichier « clé=valeur » sans sections, comme <c>Engine\vars_pc.cfg</c> du jeu (une ligne par réglage, ordre
/// alphabétique, fins de ligne CRLF). Modifié en place : les lignes inconnues et l'ordre sont conservés ; une clé
/// absente est ajoutée à la fin. Les noms de clés sont comparés sans tenir compte de la casse, comme le fait le jeu
/// pour ses réglages (« FullScreen », « fpsLimit »...), mais la casse d'origine est gardée à l'écriture.
/// </summary>
public sealed class KeyValueDocument
{
    private readonly List<string> _lines;

    private KeyValueDocument(List<string> lines) => _lines = lines;

    public static KeyValueDocument Load(string path) =>
        new(File.Exists(path) ? File.ReadAllLines(path, Encoding.ASCII).ToList() : new List<string>());

    public static KeyValueDocument Parse(string content) =>
        new(content.Replace("\r\n", "\n").Split('\n').Where(line => line.Length > 0).ToList());

    public string? Get(string key)
    {
        int index = FindKey(key);
        return index < 0 ? null : _lines[index][(_lines[index].IndexOf('=') + 1)..].Trim();
    }

    public int GetInt(string key, int fallback) =>
        Get(key) is { } value && int.TryParse(value, out int number) ? number : fallback;

    public bool GetBool(string key, bool fallback) => Get(key) switch
    {
        { } value when value.Equals("true", StringComparison.OrdinalIgnoreCase) => true,
        { } value when value.Equals("false", StringComparison.OrdinalIgnoreCase) => false,
        _ => fallback,
    };

    public void Set(string key, string value)
    {
        int index = FindKey(key);
        if (index >= 0)
        {
            string name = _lines[index][.._lines[index].IndexOf('=')];
            _lines[index] = $"{name}={value}";
        }
        else
        {
            _lines.Add($"{key}={value}");
        }
    }

    public void SetInt(string key, int value) => Set(key, value.ToString());

    // Le jeu écrit « true » / « false ».
    public void SetBool(string key, bool value) => Set(key, value ? "true" : "false");

    public void Save(string path) => File.WriteAllText(path, string.Join("\r\n", _lines) + "\r\n", Encoding.ASCII);

    public override string ToString() => string.Join("\n", _lines);

    private int FindKey(string key)
    {
        for (int i = 0; i < _lines.Count; i++)
        {
            int equals = _lines[i].IndexOf('=');
            if (equals > 0 && string.Equals(_lines[i][..equals].Trim(), key, StringComparison.OrdinalIgnoreCase))
            {
                return i;
            }
        }
        return -1;
    }
}
