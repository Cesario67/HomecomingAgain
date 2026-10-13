using System.Security.Cryptography;

namespace HomecomingAgain.Launcher.Core;

/// <summary>
/// Fichiers de l'option DLSS dans <c>Bin</c> : DXVK (<c>d3d9.dll</c>, Direct3D 9 sur Vulkan, nécessaire au partage des
/// images), le programme DLSS 64 bits, le moteur DLSS de NVIDIA et leurs licences. Installés quand l'option est activée ;
/// à sa désactivation, seule <c>d3d9.dll</c> est retirée (le jeu revient à Direct3D 9 normal), les autres fichiers ne
/// servent à rien sans elle. Une <c>d3d9.dll</c> qui n'est pas la nôtre (autre mod) est mise de côté, jamais supprimée.
/// </summary>
public static class DlssInstaller
{
    public const string DxvkDll = "d3d9.dll";
    public const string HelperExe = "HomecomingAgain.Dlss.exe";
    public const string NvidiaDll = "nvngx_dlss.dll";
    private static readonly string[] Licenses = ["LICENSE-DXVK.txt", "LICENSE-NVIDIA-DLSS.txt"];

    private static string ForeignBackup(GamePaths paths) => Path.Combine(paths.BinDirectory, DxvkDll + ".avant-hca");

    /// <summary>Le paquet contient-il tout ce qu'il faut pour l'option DLSS ?</summary>
    public static bool IsAvailable(string packageDirectory) =>
        new[] { DxvkDll, HelperExe, NvidiaDll }.All(name => File.Exists(Path.Combine(packageDirectory, name)));

    /// <summary>La d3d9.dll de Bin est-elle celle du paquet (même contenu) ?</summary>
    public static bool IsOurDxvk(string packageDirectory, GamePaths paths)
    {
        string installed = Path.Combine(paths.BinDirectory, DxvkDll);
        string source = Path.Combine(packageDirectory, DxvkDll);
        return File.Exists(installed) && File.Exists(source) && SameContent(installed, source);
    }

    /// <summary>Installe ou retire les fichiers selon l'option ; renvoie le compte rendu (vide si rien n'a changé).</summary>
    public static IReadOnlyList<string> Apply(bool enabled, string packageDirectory, GamePaths paths) =>
        enabled ? Install(packageDirectory, paths) : Remove(packageDirectory, paths);

    public static IReadOnlyList<string> Install(string packageDirectory, GamePaths paths)
    {
        if (!IsAvailable(packageDirectory))
        {
            throw new FileNotFoundException(Loc.Get("DlssPackageMissing"));
        }
        List<string> report = new();
        string dxvk = Path.Combine(paths.BinDirectory, DxvkDll);
        if (File.Exists(dxvk) && !IsOurDxvk(packageDirectory, paths) && !File.Exists(ForeignBackup(paths)))
        {
            File.Copy(dxvk, ForeignBackup(paths));
            report.Add(Loc.Get("DlssForeignKept"));
        }
        bool changed = false;
        foreach (string name in new[] { DxvkDll, HelperExe, NvidiaDll })
        {
            changed |= CopyIfDifferent(Path.Combine(packageDirectory, name), Path.Combine(paths.BinDirectory, name));
        }
        foreach (string license in Licenses)
        {
            string source = Path.Combine(packageDirectory, license);
            if (File.Exists(source))
            {
                CopyIfDifferent(source, Path.Combine(paths.BinDirectory, "HomecomingAgain-" + license));
            }
        }
        if (changed)
        {
            report.Add(Loc.Get("DlssInstalled"));
        }
        return report;
    }

    public static IReadOnlyList<string> Remove(string packageDirectory, GamePaths paths)
    {
        List<string> report = new();
        string dxvk = Path.Combine(paths.BinDirectory, DxvkDll);
        if (IsOurDxvk(packageDirectory, paths))
        {
            File.Delete(dxvk);
            report.Add(Loc.Get("DlssRemoved"));
            if (File.Exists(ForeignBackup(paths)))
            {
                File.Move(ForeignBackup(paths), dxvk);
                report.Add(Loc.Get("DlssForeignRestored"));
            }
        }
        return report;
    }

    /// <summary>Désinstallation du mod : DXVK retiré comme ci-dessus, et nos autres fichiers DLSS supprimés.</summary>
    public static IReadOnlyList<string> RemoveAll(string packageDirectory, GamePaths paths)
    {
        List<string> report = new(Remove(packageDirectory, paths));
        foreach (string name in new[] { HelperExe, "HomecomingAgain-LICENSE-DXVK.txt", "HomecomingAgain-LICENSE-NVIDIA-DLSS.txt" })
        {
            File.Delete(Path.Combine(paths.BinDirectory, name));
        }
        // nvngx_dlss.dll : seulement si c'est la nôtre (même contenu que le paquet).
        string nvidia = Path.Combine(paths.BinDirectory, NvidiaDll);
        string source = Path.Combine(packageDirectory, NvidiaDll);
        if (File.Exists(nvidia) && File.Exists(source) && SameContent(nvidia, source))
        {
            File.Delete(nvidia);
        }
        return report;
    }

    private static bool CopyIfDifferent(string source, string target)
    {
        if (File.Exists(target) && SameContent(source, target))
        {
            return false;
        }
        File.Copy(source, target, overwrite: true);
        return true;
    }

    private static bool SameContent(string a, string b)
    {
        if (new FileInfo(a).Length != new FileInfo(b).Length)
        {
            return false;
        }
        using FileStream first = File.OpenRead(a);
        using FileStream second = File.OpenRead(b);
        return SHA256.HashData(first).AsSpan().SequenceEqual(SHA256.HashData(second));
    }
}
