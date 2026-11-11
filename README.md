# Smash Melee — experimental PS Vita port

A native port of **Super Smash Bros. Melee (NTSC-U 1.02)** to the
**PlayStation Vita**, developed on the
[`vita-port` branch](https://github.com/zm2283145/melee-Vita/tree/vita-port).

This is not an emulator. The game code from
[doldecomp/melee](https://github.com/doldecomp/melee) and
[melee-pc](https://github.com/999sian/melee-pc) is compiled for the Vita,
and the GameCube platform services are replaced with native input, disc
access, audio, saves, movie playback and a GX renderer (GX → GXM, shaders
built with vitaShaRK).

**The port is experimental.** Expect slowdowns, visual or audio issues and
possible crashes. Back up your saves before trying a new build. Releases are
on GitHub (current: **0.8.15**). Menus and matches are not locked to 60 fps.
One local player uses the built-in controls. Output is 960×544, with graphics
presets and a Settings menu.

You need your own legally obtained, **uncompressed** Melee NTSC-U 1.02 disc
image (`GALE01`), named `GALE01.iso`. Other regions, revisions and compressed
RVZ/CISO images are not supported. No game data or proprietary console module
is distributed with this project, and you do not need a disc image to compile.

---

## PS Vita

### Requirements

- A homebrew-enabled PS Vita with HENkaku/taiHEN and **Enable Unsafe
  Homebrew** turned on, plus [VitaShell](https://github.com/TheOfficialFloW/VitaShell)
  to install VPKs and copy files.
- The Vita runtime shader compiler **`libshacccg.suprx`** at
  **`ur0:data/libshacccg.suprx`**. It is not included. Get it from your own
  Vita software using the usual homebrew setup guides.
- Your `GALE01.iso`, plus free space on `ux0:` for the image (1,459,978,240
  bytes, about 1.36 GiB), the app, saves and the shader cache.

The Release VPK does **not** need VitaDebugger, its kernel companion or
kubridge, and contains no DebugNet logger or GDB server.

### Install and run

1. Download `SmashMeleevita-<version>.vpk` from a **published release** in
   [Releases](https://github.com/zm2283145/melee-Vita/releases).
2. Install it with VitaShell. It uses title ID **`MLVITA002`** and appears as
   **Smash Melee Vita** in LiveArea.
3. Copy your disc image to **`ux0:data/melee/GALE01.iso`**. Check that the
   shader compiler is in place.
4. Launch the LiveArea bubble. The game reads that fixed path; there is no disc
   picker.

This is the full-game VPK, not the older `MLVITA001` diagnostic viewer.

### Controls

The built-in controls act as GameCube controller port 1.

| PS Vita | GameCube / use |
| --- | --- |
| Left stick | Main stick |
| Right stick | C-stick |
| D-pad | D-pad |
| Cross / Circle | A / B |
| Square / Triangle | X / Y (jump) |
| L / R | L / R triggers (shield; full press only) |
| Select | Z (grab) |
| Start | Start / pause |

Touch controls, rumble, external controllers and more than one local player
are not supported on Vita. Button mapping and render resolution are in the
Settings menu below, and also under **Options → Vita Options**. Both save
the same settings.

### Settings

The LiveArea bubble has a **Settings** button. It opens a settings screen
instead of booting Melee. Choices are saved to `ux0:data/melee/graphics.txt`
and read the next time the game starts.

Three presets cover the usual tradeoff:

- **Ultra** aims for 60 fps in every 1v1, with stage cutbacks and potato mode
  on Fountain of Dreams and Yoshi's Story.
- **Performance** keeps lighter stages and 60 fps in 1v1s, with Fountain of
  Dreams dropped to 30.
- **Quality** runs matches at a locked 30 and keeps the sharper picture.

Changing a value off its preset turns the choice into **Custom**. Menus stay
at 60 even when matches are locked to 30. A match with 3 or 4 players, or
Fountain of Dreams without cutbacks, can drop to a locked 30 while the game
logic still runs at full speed.

The same screen sets 16:9 or pillarboxed 4:3, a temporary unlock of every
character and stage (the memory card is not changed), tournament VS rules
(4 stocks, 8 minutes, items off, team attack on, pause off, legal stages),
UCF 0.84 (a wider dashback window and shield drops that do not become spot
dodges), an FPS counter, button remapping, and stick calibration with
deadzones. There is also a benchmark that plays six stages and writes a
report.

### Slippi

Online play is Slippi Direct, and it is off unless the VPK is built with
`-EnableSlippi`. A normal Release build has no online menus.

With that flag, **1P Mode** gains **Online Play**. Two Vitas connect directly
and play a 1v1. Online play needs both games to simulate the match the same
way, so the flag also turns on GameCube-exact floating point. Building that
needs Linux, macOS, or WSL: the Windows VitaSDK GCC cannot load the plugin,
and the host needs a C++ compiler plus GMP headers.

This is early. Expect desyncs and disconnects. Local play does not need the
flag.

### Shader compilation and stutter

New characters, stages, menus and effects compile shaders the first time they
appear, which causes stutter. Compiled shaders are cached in
`ux0:data/melee/shadercache/`, so scenes get smoother as you play. Keep the
cache when updating unless release notes say otherwise.

### Saves

The virtual memory card lives in **`ux0:data/melee/save/`**. Back up that
folder with the game closed, especially before updating. Dolphin `.gci` files
in `save/import/` are imported at boot, and game saves are exported to
`save/export/`. Don't change those files while the game is running.

### Hidden Debug Tools

Release builds include a hidden, session-only version of Melee's developer
tools. In **Options**, press **Up, Up, Down, Down, Left, Right, Left, Right,
Select** on the D-pad to reveal **DEBUG TOOLS**. It disappears again when the
game closes and never writes an unlock to the memory card.

Inside, use the D-pad to move and change values, Cross to select and Circle to
go back. Save-data, memory-card, raw memory and unsupported GameCube hardware
options are shown but disabled. To leave a debug match, press Start, then hold
L and R and press Cross. See the
[Debug Tools Guide](platforms/vita/DEBUG-TOOLS.md) for the item spawner, debug
camera, collision overlays and match shortcuts.

### Building

The build runs on **macOS, Windows, and Linux**. You do not need a disc image
to compile; `GALE01.iso` is only required on the Vita, at
`ux0:data/melee/GALE01.iso`.

On the build machine:

- PowerShell 7 (`pwsh`; on macOS, `brew install powershell`) and Python 3
  (`python3` on macOS and Linux).
- [VitaSDK](https://vitasdk.org/) with GCC 15. The build needs big-endian
  `scalar_storage_order`. Set `VITASDK` and put `$VITASDK/bin` on `PATH`.
- `libvita2d`, taiHEN import stubs, pthread support, and the SDK system stubs,
  installed into that SDK.
- [SceShaccCgExt](https://github.com/bythos14/SceShaccCgExt), then
  [vitaShaRK](https://github.com/Rinnegatamante/vitaShaRK). Install
  SceShaccCgExt first.

From a checkout of the `vita-port` branch:

```powershell
$env:VITASDK = 'C:\vitasdk'
$env:PATH = "$env:VITASDK\bin;$env:PATH"
.\platforms\vita\build-full.ps1 -Configuration Release -Jobs 8
```

On macOS, from the repository root. VitaSDK at `/usr/local/vitasdk` is picked
up when `VITASDK` is unset:

```sh
pwsh -NoProfile -File platforms/vita/build-full.ps1 -Configuration Release -Jobs 8
```

On Linux, set `VITASDK` and `PATH` the same way as Windows, then run:

```sh
pwsh -NoProfile -File platforms/vita/build-full.ps1 -Configuration Release -Jobs 2
```

The package is `build-vita/full/SmashMeleevita.vpk` (title **Smash Melee
Vita**, title ID `MLVITA002`). Install it with VitaShell. Homebrew must be
enabled, with Unsafe Homebrew on, and `libshacccg.suprx` must be at
`ur0:data/libshacccg.suprx`. Release builds are optimized and have no logging,
debugger support, or debug symbols. Add `-EnableSlippi` on Linux, macOS, or
WSL to include Online Play.

More flags and toolchain notes are in the
[Vita build instructions](platforms/vita/README.md). The **Vita build and
draft release** workflow builds every push to `vita-port` and drafts a
release when the version in `platforms/vita/version.json` is bumped.

---

## Bugs and issue reports

Please report problems in
[GitHub Issues](https://github.com/zm2283145/melee-Vita/issues). Check for an
existing report first, then include:

- the release or commit ID, and the firmware and plugins you use;
- the game mode, characters and stage;
- clear steps to reproduce, what you expected, and whether it happens again
  once shaders are cached.

Screenshots or a short video help. Never attach your disc image, game assets,
proprietary modules or personal information.

## Scope

Slippi on Vita is Direct play only, and only in a build made with
`-EnableSlippi`. Rollback netplay is not implemented. The desktop launcher,
settings overlay, custom soundtrack streaming and PC cheat toggles are
PC-only. The inherited [PC README](README-PC.md) and [roadmap](ROADMAP.md)
describe the upstream PC project, not promises for this port.

## Credits and licensing

Thanks to the Melee decompilation and melee-pc contributors, aurora, VitaSDK,
vita2d, vitaShaRK, SceShaccCgExt and the Vita homebrew community. The updated
LiveArea artwork is by Reddit user
[u/cool_pain_6315](https://www.reddit.com/user/cool_pain_6315/). The Vita VPK
includes five alternate Giga Bowser costume archives from the supplied mod
pack.

This project is not affiliated with Nintendo, HAL Laboratory or Sony. Read
[LICENSE.md](LICENSE.md) and [COPYING](COPYING) before redistributing. The
port code and third-party components have their own licenses; the decompiled
game code is not covered by the port's GPL license.
