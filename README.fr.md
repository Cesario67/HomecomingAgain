# SHHFix

Mod de correction et d'amélioration pour *Silent Hill: Homecoming* sur PC (Steam), pour le faire fonctionner
sur les machines actuelles : plantages et cadence d'images, graphismes et prise en charge correcte des
manettes Xbox.

**État : développement initial.** Rien n'est publié ; le projet n'a pas encore été compilé ni testé en jeu.
Recherches : [docs/recherche-web.md](docs/recherche-web.md). Travail en cours : [docs/a-faire.md](docs/a-faire.md).

## Objectifs

1. Plantages et cadence sur le matériel actuel et Windows 10/11.
2. Graphismes : résolution et ratio, champ de vision, anticrénelage, carte et interface.
3. Manettes Xbox 360 / Xbox.

## Compilation

Build Tools for Visual Studio 2022 (charge C++), cible x86 :

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

## Licence

GPL-3.0-or-later. Aucun fichier du jeu n'est inclus ni distribué : le dépôt ne contient que son propre code.
