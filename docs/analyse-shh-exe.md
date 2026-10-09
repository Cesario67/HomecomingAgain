# Analyse de SilentHill.exe

À remplir après installation du jeu. Règles (voir CLAUDE.md) : désassembler dans le scratchpad, jamais dans le
dépôt ; consigner adresses et descriptions en distinguant **vérifié** (désassemblage, dump, test) de
**hypothèse**.

## Identité

| Élément | Valeur |
|---|---|
| Version / source | Steam, appid 19000 |
| Horodatage PE | à relever |
| Bitness | à relever (supposé 32 bits) |
| ASLR, LARGE_ADDRESS_AWARE, DEP | à relever |
| DRM (SteamStub) | à relever |

## Imports utiles

À relever : `d3d9.dll`, `xinput*.dll`, `dinput8.dll`, `winmm.dll`, `kernel32!Sleep`, `QueryPerformanceCounter`.
