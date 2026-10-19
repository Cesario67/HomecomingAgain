using System.Buffers.Binary;
using System.Reflection.PortableExecutable;
using System.Text;

namespace HomecomingAgain.Launcher.Core;

/// <summary>
/// Lit les chaînes de la ressource de version (VS_VERSIONINFO) d'un fichier PE : FileDescription, FileVersion...
/// FileVersionInfo ne le fait que sous Windows : sous Linux, il ne lit que les assemblys .NET et renvoie des champs
/// vides pour une DLL native comme notre version.dll (le launcher Linux croyait alors le paquet absent).
/// </summary>
public static class PeVersionInfo
{
    private const int ResourceTypeVersion = 16; // RT_VERSION
    private const uint SubdirectoryFlag = 0x80000000;

    private static readonly IReadOnlyDictionary<string, string> Empty = new Dictionary<string, string>();

    /// <summary>Chaînes de la première table de StringFileInfo ; vide si le fichier n'en a pas ou n'est pas un PE.</summary>
    public static IReadOnlyDictionary<string, string> ReadStrings(string path)
    {
        try
        {
            using FileStream stream = File.OpenRead(path);
            using PEReader pe = new(stream);
            DirectoryEntry table = pe.PEHeaders.PEHeader?.ResourceTableDirectory ?? default;
            if (table.Size == 0)
            {
                return Empty;
            }
            ReadOnlySpan<byte> resources = pe.GetSectionData(table.RelativeVirtualAddress).GetContent().AsSpan();
            if (FindVersionResource(resources) is not (int rva, int size))
            {
                return Empty;
            }
            ReadOnlySpan<byte> version = pe.GetSectionData(rva).GetContent().AsSpan();
            return ParseStrings(version[..Math.Min(size, version.Length)]);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or BadImageFormatException
                                              or ArgumentException or IndexOutOfRangeException or InvalidOperationException)
        {
            return Empty;
        }
    }

    // Arborescence des ressources : type (RT_VERSION), puis nom, puis langue ; on prend la première de chaque niveau.
    // Les décalages des sous-dossiers sont relatifs au début de la section, celui des données est une RVA.
    private static (int Rva, int Size)? FindVersionResource(ReadOnlySpan<byte> resources)
    {
        uint? names = FindEntry(resources, 0, ResourceTypeVersion);
        if (names is not { } namesOffset || (namesOffset & SubdirectoryFlag) == 0)
        {
            return null;
        }
        uint? languages = FindEntry(resources, (int)(namesOffset & ~SubdirectoryFlag), null);
        if (languages is not { } languagesOffset || (languagesOffset & SubdirectoryFlag) == 0)
        {
            return null;
        }
        uint? data = FindEntry(resources, (int)(languagesOffset & ~SubdirectoryFlag), null);
        if (data is not { } dataOffset || (dataOffset & SubdirectoryFlag) != 0)
        {
            return null;
        }
        // IMAGE_RESOURCE_DATA_ENTRY : RVA des données, taille.
        int entry = (int)dataOffset;
        return ((int)BinaryPrimitives.ReadUInt32LittleEndian(resources[entry..]),
                (int)BinaryPrimitives.ReadUInt32LittleEndian(resources[(entry + 4)..]));
    }

    // IMAGE_RESOURCE_DIRECTORY (16 octets, nombres d'entrées nommées puis numérotées en 12 et 14) suivi
    // d'entrées de 8 octets (nom ou numéro, décalage). Renvoie le décalage de l'entrée de numéro id, ou de la première.
    private static uint? FindEntry(ReadOnlySpan<byte> resources, int directory, int? id)
    {
        int count = BinaryPrimitives.ReadUInt16LittleEndian(resources[(directory + 12)..]) +
                    BinaryPrimitives.ReadUInt16LittleEndian(resources[(directory + 14)..]);
        for (int index = 0; index < count; index++)
        {
            int entry = directory + 16 + index * 8;
            uint name = BinaryPrimitives.ReadUInt32LittleEndian(resources[entry..]);
            if (id is null || name == (uint)id)
            {
                return BinaryPrimitives.ReadUInt32LittleEndian(resources[(entry + 4)..]);
            }
        }
        return null;
    }

    /// <summary>Chaînes d'un bloc VS_VERSIONINFO (contenu brut de la ressource).</summary>
    public static IReadOnlyDictionary<string, string> ParseStrings(ReadOnlySpan<byte> version)
    {
        Block root = ReadBlock(version, 0);
        if (root.Key != "VS_VERSION_INFO")
        {
            return Empty;
        }
        foreach (Block child in Children(version, root))
        {
            if (child.Key != "StringFileInfo")
            {
                continue;
            }
            foreach (Block table in Children(version, child))
            {
                Dictionary<string, string> strings = new();
                foreach (Block entry in Children(version, table))
                {
                    strings[entry.Key] = ReadString(version, entry.ValueStart, entry.End);
                }
                return strings;
            }
        }
        return Empty;
    }

    // Chaque bloc : longueur totale, longueur de la valeur, type (1 = texte, longueur en caractères), clé UTF-16
    // terminée par un zéro, alignement sur 4 octets, valeur, alignement, blocs enfants.
    private readonly record struct Block(string Key, int ValueStart, int ChildrenStart, int End);

    private static Block ReadBlock(ReadOnlySpan<byte> data, int start)
    {
        int length = BinaryPrimitives.ReadUInt16LittleEndian(data[start..]);
        int valueLength = BinaryPrimitives.ReadUInt16LittleEndian(data[(start + 2)..]);
        int type = BinaryPrimitives.ReadUInt16LittleEndian(data[(start + 4)..]);
        int end = Math.Min(start + Math.Max(length, 6), data.Length);
        int keyStart = start + 6;
        string key = ReadString(data, keyStart, end);
        int valueStart = Align4(keyStart + (key.Length + 1) * 2);
        int childrenStart = Align4(valueStart + (type == 1 ? valueLength * 2 : valueLength));
        return new Block(key, valueStart, childrenStart, end);
    }

    private static List<Block> Children(ReadOnlySpan<byte> data, Block parent)
    {
        List<Block> children = new();
        for (int position = parent.ChildrenStart; position + 6 <= parent.End;)
        {
            Block child = ReadBlock(data, position);
            children.Add(child);
            position = Align4(child.End);
        }
        return children;
    }

    // Chaîne UTF-16 terminée par un zéro (ou par la fin du bloc).
    private static string ReadString(ReadOnlySpan<byte> data, int start, int end)
    {
        int position = start;
        while (position + 1 < end && (data[position] | data[position + 1]) != 0)
        {
            position += 2;
        }
        return start < position ? Encoding.Unicode.GetString(data[start..position]) : string.Empty;
    }

    private static int Align4(int offset) => (offset + 3) & ~3;
}
