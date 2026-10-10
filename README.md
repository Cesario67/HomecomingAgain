# SHHFix

A fix and enhancement mod for *Silent Hill: Homecoming* on PC (Steam), aimed at making it run on modern
machines: crash and frame-rate fixes, graphics improvements and proper Xbox controller support.

**Status: early development.** Nothing is released yet; the project has not been compiled or tested in-game.
Research notes: [docs/recherche-web.md](docs/recherche-web.md) (French). Open work: [docs/a-faire.md](docs/a-faire.md).

## Goals

1. Crashes and frame pacing on current hardware and Windows 10/11.
2. Graphics: resolution and aspect ratio, field of view, anti-aliasing, map and UI.
3. Xbox 360 / Xbox controller support.

## Building

Requires Build Tools for Visual Studio 2022 (C++ workload), x86 target:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

## License

GPL-3.0-or-later. No game files are included or distributed; this project contains only its own code.

## Third-party

Uses SMAA. Copyright (C) 2011 by Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro and Diego
Gutierrez. See `third_party/smaa/LICENSE.txt`.
