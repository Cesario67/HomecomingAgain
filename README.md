# Homecoming Again

A fix and enhancement mod for *Silent Hill: Homecoming* on PC (Steam), to make it run properly on modern machines.

**Status: in development, not released yet.** The fixes below are tested in-game on one machine (Windows 11,
RTX 5070 Ti) with the Steam version of the game. Research notes: [docs/recherche-web.md](docs/recherche-web.md)
(French). Open work: [docs/a-faire.md](docs/a-faire.md).

## What it fixes

### Stability and frame rate

- **60 frames per second**: the game is locked to 30 fps by a value hard-coded in its engine (the game's `fpsLimit`
  setting has no effect on it). The mod replaces that cap with the chosen frame rate, without modifying game files.
- **Borderless fullscreen**: the game runs in a window covering the whole screen. No more Alt-Tab crash, which
  affects the original exclusive fullscreen.
- **Loading freezes with the Steam overlay**: the game could hang while loading in windowed mode. Fixed (window
  messages handled while waiting, Direct3D protected against concurrent calls).
- **4 GB patch**: the game can use 4 GB of memory instead of 2 (applied by the launcher, original kept and restored
  on uninstall).
- **Diagnostics**: on a crash or a hang, a report and a diagnostic file (minidump) are written to
  `Bin\HomecomingAgain`.

### Sound

- **Sound in one ear only**: on headphones or stereo speakers, most sounds (effects, voices, music) came out of one
  side. This is a known flaw of the PC port. The game mixes everything into a single 5.1 output whose sound
  placement is wrong, and its XAudio2 version converts it badly to stereo. The mod does the conversion itself and
  properly mixes it down to stereo: every sound is back in its left/right position, without replacing audio files
  or installing a driver.

### Graphics

- **SMAA**: anti-aliasing that smooths jagged edges.
- **16x anisotropic filtering**: sharp textures on the ground and at an angle.
- **Ambient occlusion (SSAO)**: subtle darkening of corners and contact between objects, computed from the
  engine's depth and applied before the interface.
- **Sharper shadows**: 4096x4096 shadow map instead of 2048x2048.
- **Adjustable film grain**: off, halved or as in the original.

### Comfort

- **Skip cutscenes**: the Delete key skips video cutscenes.
- **Launcher** (`HomecomingAgain.exe`): all settings, install and uninstall, in English and French.

## Installation

Run `HomecomingAgain.exe`, select the game folder, then **Install**. The launcher copies the mod into `Bin`, applies
the 4 GB patch and keeps a copy of every file it changes. **Uninstall** restores everything.

## Settings

In the launcher, or in `Bin\HomecomingAgain.ini`:

| Section | Setting | Effect |
|---|---|---|
| `[Frames]` | `TargetFps=60` | Game frame rate (0 = original 30 fps) |
| `[Window]` | `Borderless=1` | Borderless fullscreen |
| `[Graphics]` | `SMAA`, `Anisotropy` | Anti-aliasing, texture filtering |
| `[Graphics]` | `SSAO`, `SsaoStrength` | Ambient occlusion and its strength |
| `[Graphics]` | `ShadowResolution=4096` | Shadow map size (2048 originally) |
| `[Graphics]` | `FilmGrain=100` | Film grain in % (0 = none) |
| `[Audio]` | `ChannelFix=1` | Fixed sound (0 = original sound) |
| `[Movies]` | `SkipKey=46` | Key to skip cutscenes (46 = Delete, 0 = none) |

## Known limitations

- Exclusive fullscreen still crashes on Alt-Tab (original game): use borderless mode.
- Controllers: not handled yet.
- DLSS: in preparation (experimental, inactive).

## Building

Requires Build Tools for Visual Studio 2022 (C++ workload), x86 target, and the .NET 10 SDK for the launcher:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

## License

GPL-3.0-or-later. No game files are included or distributed; this project contains only its own code.

## Third-party

- Uses SMAA. Copyright (C) 2011 by Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro and Diego
  Gutierrez. See `third_party/smaa/LICENSE.txt`.
- DXVK interop interfaces (excerpt of `d3d9_interfaces.h`), zlib license. Copyright (c) 2017 Philip Rebohle,
  (c) 2019 Joshua Ashton, (c) 2019 Robin Kertels. See `third_party/dxvk/LICENSE.txt`.
