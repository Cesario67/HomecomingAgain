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
- **Sharper shadows**: 8192x8192 shadow map instead of 2048x2048, with the slope bias scaled to match.
- **Adjustable film grain**: off, halved or as in the original.
- **Map no longer cut off above 720p**: the game clipped the map screen to a fixed 1280x720 area; the mod extends it to
  the whole picture (1080p, 1440p, 4K...) while keeping the map at its original size and place.
- **Examined items above 720p**: the item just picked up (knife, first aid kit...) was cut off in a corner of the
  screen, or stretched. It is now centred, whole and correctly proportioned, as in 720p.
- **DLSS (DLAA, experimental)**: NVIDIA AI anti-aliasing, more stable and sharper than SMAA, at native resolution.
  NVIDIA RTX cards only; the launcher option also installs DXVK (Direct3D 9 on Vulkan). As the game is 32-bit and DLSS
  is 64-bit, a small 64-bit program (`HomecomingAgain.Dlss.exe`) receives the game's images, applies DLSS and hands
  them back before the interface. Off by default.
- **High quality game shaders**: SSAO and DLSS also work in this mode (16-bit scene).

### Controller

- **Xbox or PlayStation button icons**: menus, the inventory, the controls screen and in-game prompts (Open,
  Save...) show A, B, X, Y (or cross, circle, square, triangle) instead of the digits 1 to 4 of the PC version. The
  icons come from the game's own data. By default, Xbox icons while a controller is plugged in, PC icons otherwise.
- **Real button names** on the controls screen (LB, LT, D-pad up...) instead of "Button 6", and the right stick icon
  next to "Zoom" on the map.
- **No more accidental pauses**: A and B are left to the game, which handles the controller in its own menus.
- **PlayStation controller without Steam Input** (DualShock 4, DualSense, being tested): the game only knows Xbox
  controllers; with no Xbox controller plugged in, the PlayStation controller is presented to it as an Xbox one, with
  PlayStation icons in auto mode (`[Input] PlayStationPad`, on by default).
- **Hold-to-mash QTE** (accessibility, off by default): during a button-mashing prompt, holding the button counts as
  repeated presses (`[Input] QteHold=1`, speed `QteHoldRate`, checkbox in the launcher). Controller only.
- **Orbit camera** (experimental): with the camera fix, the right stick turns the camera alone around the character.
  Enabled in the launcher (Controller tab). Holding the left trigger (L2) suspends the orbit (`OrbitGuardFollow`).

### Comfort

- **Skip cutscenes**: the Delete key skips video cutscenes.
- **Cheats** (off by default): god mode (the player loses no health) and unlimited ammo, as checkboxes in the
  launcher (Performance and stability tab) or in the `[Cheats]` section of `HomecomingAgain.ini` (`God=1`,
  `UnlimitedAmmo=1`). Verified in game.
- **Faster startup logos**: the title screen appears after about 5 s instead of 23. The logos (Konami, Double Helix,
  legal screen) play fast-forwarded; the game has already finished loading while they show.
- **Launcher** (`HomecomingAgain.exe`): all settings, install and uninstall, in English (default) and French.

## Installation

Run `HomecomingAgain.exe`, select the game folder, then **Install**. The launcher copies the mod into `Bin`, applies
the 4 GB patch and keeps a copy of every file it changes. **Uninstall** restores everything.

### Linux and Steam Deck (Proton): experimental, not tested yet

The game runs in Proton, and the mod (a Windows DLL) with it. The launcher also comes as a Linux build:
1. Download `HomecomingAgain` (Linux build), make it executable (`chmod +x HomecomingAgain`) and run it. It finds the
   game in the usual Steam libraries (`~/.local/share/Steam`, `~/.steam/steam`, Flatpak).
2. **Install**, then copy the line from the Installation tab into the game's Steam launch options. It starts with
   `WINEDLLOVERRIDES="version=n,b"`: without it, Wine uses its own `version.dll` and the mod is not loaded.

**Mod not working after installing with the first 0.9.4 Linux launcher?** That launcher copied itself into `Bin` as
`HomecomingAgain`, the name of the mod's own folder, which prevented the mod from starting. Install again with the
current launcher (it removes that file and copies itself as `HomecomingAgain-launcher`), then copy the new launch
line. Or by hand: rename `Bin/HomecomingAgain` to `HomecomingAgain-launcher` and update the launch options. When the
mod is running, `Bin/HomecomingAgain/logs` holds its log.

Without the Linux launcher: copy `version.dll` and `HomecomingAgain.ini` into `Bin`, and set the launch options to
`WINEDLLOVERRIDES="version=n,b" %command%`.

The DLSS option is greyed out on Linux: Proton already ships its own DXVK, and DLSS there needs settings
(dxvk-nvapi, DLL overrides) that have not been tested.

## Settings

In the launcher, or in `Bin\HomecomingAgain.ini`:

| Section | Setting | Effect |
|---|---|---|
| `[Frames]` | `TargetFps=60` | Game frame rate (0 = original 30 fps) |
| `[Window]` | `Borderless=1` | Borderless fullscreen |
| `[Graphics]` | `SMAA`, `Anisotropy` | Anti-aliasing, texture filtering |
| `[Graphics]` | `SSAO`, `SsaoStrength` | Ambient occlusion and its strength |
| `[Graphics]` | `ShadowResolution=8192` | Shadow map size (2048 originally, 4096 or 8192) |
| `[Graphics]` | `FilmGrain=100` | Film grain in % (0 = none) |
| `[Graphics]` | `HorPlus=0` | Wider field of view on screens wider than 16:9 (the launcher checks it from the 21:9 format); `HorPlusEngine=1` lets the engine build its own projection and culling, `ClipFinalCopy=1` fixes the black screen in non 16:9 sizes |
| `[Graphics]` | `FullScreenUiClip=1` | Map no longer cut off above 720p (0 = original) |
| `[Audio]` | `ChannelFix=1` | Fixed sound (0 = original sound) |
| `[Movies]` | `SkipKey=46` | Key to skip cutscenes (46 = Delete, 0 = none) |
| `[Movies]` | `SkipLogos=1` | Fast-forward the startup logos (0 = original) |
| `[Input]` | `ButtonIcons=0` | Button icons: 0 = auto (Xbox if a controller is plugged in), 1 = PC (original), 2 = Xbox, 3 = PlayStation |
| `[Camera]` | `CameraFix=0` | Controller camera fix (based on Emertels' Camera Fix): the character turns with the right stick, proportional vertical response, separate aim sensitivity. Speeds `HorizontalSpeed`, `VerticalSpeed`, `AimHorizontalSpeed`, `AimVerticalSpeed` (0.25 to 3.0). The launcher also frees the right stick in `Engine\binds_pc_mjs.cfg` |
| `[Camera]` | `Mode=Follow` | With `CameraFix=1`: `Follow` = the character turns and the camera follows; `Orbit` (experimental) = the right stick turns the camera around the character, who stays put |
| `[Camera]` | `OrbitGuardFollow=1` | With `Mode=Orbit`: `1` = while the left trigger (L2, knife guard) is held, the orbit is suspended (camera back behind the character, right stick turns him); `0` = the orbit stays active |
| `[DLSS]` | `ShareFrames`, `Jitter`, `Apply` | DLSS (DLAA): all three at 1 to enable it, with DXVK (done by the launcher) |

## Known limitations

- Exclusive fullscreen still crashes on Alt-Tab (original game): use borderless mode.
- Controllers: only XInput controllers (Xbox and compatible) are read by the game. Dead zones, camera sensitivity
  and vibration are still the original ones.
- DLSS: experimental. Only camera motion is passed on: slight trails are possible on fast-moving characters.

## Building

Requires Build Tools for Visual Studio 2022 (C++ workload), x86 target, and the .NET 10 SDK for the launcher:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

## License

GPL-3.0-or-later. No game files are included or distributed; this project contains only its own code.

Additional permission under GNU GPL version 3 section 7: if you modify this Program, or any covered work, by
linking or combining it with the NVIDIA DLSS SDK (or a modified version of that library), containing parts covered
by the terms of the NVIDIA RTX SDKs License, the licensors of this Program grant you additional permission to convey
the resulting work. Corresponding Source for a non-source form of such a combination shall not include the source
code for the parts of the NVIDIA DLSS SDK used as well as that of the covered work. The NVIDIA DLSS SDK itself is
not covered by the GPL and stays under the NVIDIA RTX SDKs License.

## Third-party

- Uses SMAA. Copyright (C) 2011 by Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro and Diego
  Gutierrez. See `third_party/smaa/LICENSE.txt`.
- Controller camera fix ported from Camera Fix by Emerson Teles (Emertels), MIT license, Copyright (c) 2026 Emerson
  Teles. See `third_party/camerafix/LICENSE.txt` and `third_party/camerafix/ORIGINE.md`.
- DXVK interop interfaces (excerpt of `d3d9_interfaces.h`), zlib license. Copyright (c) 2017 Philip Rebohle,
  (c) 2019 Joshua Ashton, (c) 2019 Robin Kertels. See `third_party/dxvk/LICENSE.txt`.
- NVIDIA DLSS SDK (`nvngx_dlss.dll` and the NGX library linked into `HomecomingAgain.Dlss.exe`), NVIDIA RTX SDKs
  License, see `LICENSE-NVIDIA-DLSS.txt` in the package. Not stored in this repository: downloaded at build time.
  NVIDIA and DLSS are trademarks of NVIDIA Corporation; this project is not sponsored or endorsed by NVIDIA.
