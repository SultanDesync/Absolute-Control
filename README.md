# Absolute Control

Flight controls for Starfield: HOTAS and joystick flight, mouse steering, and head
tracking, built on one input bus and configured through a native in-game menu.

> **Status: 1.0.0-rc.1, a release candidate built from the `rewrite` branch.** It replaces
> AbsoluteHOTAS and Absolute Head Tracking: remove both before installing it. AbsoluteZero
> stays a separate mod and works beside it. The 0.3 suite's source is on
> [`release/0.3`](../../tree/release/0.3) and tag `v0.3.0-beta.1`.

## Why a rewrite

The 0.3 suite was five mods, a native menu built as a general configuration platform, an
SDK and a component catalog. Most of the maintenance cost sat in that platform scope and in
the boundaries between mods, not in flight control or the menu itself. This rewrite keeps
the flight-control features and the native menu, and drops the platform.

## Scope

| In | Out |
|---|---|
| HOTAS / joystick axes, throttle and buttons | The SDK, component catalog and menu code generator |
| Mouse steering (formerly AbsoluteZero), as a small optional module | Third-party module support |
| Head tracking, as three more axes on the input bus | Macros |
| The native Scaleform menu, opened from **ABSOLUTE CONTROL** pause-menu and title-screen entries | Absolute Power |
| Simple profiles, with an importer for existing INIs | Absolute Workbench, the external configurator, research tooling |

Only the current Steam release of Starfield is supported (1.16.244 today).

## Architecture

```text
device providers ──► input bus ──► bindings + profiles ──► flight / camera outputs
 DirectInput          copied frames,                         one writer per output
 OpenTrack            device identity,
                      lifecycle, capture
                            │
                            ▼
                native menu (Scaleform) ── edits drafts, issues commands
```

- **Input bus** (`src/runtime/`). One runtime owns every device provider and publishes
  copied, sequenced frames. Readers never touch hardware. It starts independently of the
  menu and refuses to run beside AbsoluteHOTAS.
- **Native menu** (`src/ui/`, `src/scaleform/`, `interface/`). A Scaleform menu registered
  with the game, opened from the pause menu, the title screen or a hotkey. Its row is
  appended after the existing pause-menu rows, preserving their order, and coexists with
  OSF UI. See [PauseMenu compatibility](docs/PAUSE-MENU-COMPATIBILITY.md) and
  [OSF UI compatibility](docs/OSF-UI-COMPATIBILITY.md).
- **Head tracking** is an OpenTrack provider publishing yaw, pitch and roll axes. Look
  yaw, pitch and roll each take two bound axes that add, so a tracker and a mini stick share
  one pipeline. The cockpit camera hook and the weapon field-of-fire limit come from Absolute
  Head Tracking 1.0.0-beta.1.
- **Mouse steering** is configured by INI, presets and a few hotkeys, plus one menu page.

The menu depends on about 20 engine mappings recovered for 1.16.244 that CommonLibSF does
not yet provide. The build overlays them onto CommonLibSF's `RE/IDs.h` (see
[CommonLibSF compatibility](docs/COMMONLIBSF-COMPATIBILITY.md)), and every game update
needs the [runtime update runbook](docs/RUNTIME-UPDATE-RUNBOOK.md). Contributing these
mappings upstream is the main lever for reducing that cost.

The design record behind these choices is in [`docs/history/`](docs/history/).

## What's in 1.0.0-rc.1

- Six flight axes with shaping, a lever throttle with idle, reverse, cruise and boost zones,
  roll and sideways strafe together, and the pilot gate.
- 25 ship buttons that press the game's own keys and follow in-game rebinds, with target
  system cycling.
- The reticle: the mouse, the flight stick, its own axes, or held centred.
- Head tracking through OpenTrack, mini-stick look, and the weapon field-of-fire limit.
- Walk speed on an axis.
- One settings file, `AbsoluteControl.ini`, with a one-time import of AbsoluteHOTAS and
  Absolute Head Tracking settings, and a native menu page for each area.

## Building

Requires Visual Studio 2026 Build Tools (C++23) and xmake 3.x. Rebuilding the menu SWF
additionally needs the pinned Apache Flex `mxmlc` 4.16.1 (`tools/research/build-interface.ps1`);
the compiled SWF in `interface/dist/` matches the committed ActionScript sources.

```bash
git submodule update --init --recursive
xmake f -c -p windows -a x64 -m releasedbg -y
xmake -y
xmake test
```

Pass `-p windows` explicitly. From Git Bash, xmake otherwise selects Git's bundled MinGW
and package installs fail. The build never deploys itself. The plugin keeps the 0.3 file
name so it replaces the installed host in place:

- `build/windows/x64/releasedbg/AbsoluteControlPanel.dll` → `Data/SFSE/Plugins/`
- `config/AbsoluteControlPanel.ini` → `Data/SFSE/Plugins/`
- `interface/dist/AbsoluteControlPanelMenu.swf` and `AbsoluteWalkGauge.swf` → `Data/Interface/`

## Known gaps

- Release acceptance for the September menu work is still open: both load orders with OSF UI,
  21:9 and 32:9, keyboard/mouse/controller activation and 25 open/close cycles. OSF UI is now
  2.0.0; coexistence was tested against 1.6.0.

## License

See [LICENSE](LICENSE), [CREDITS](CREDITS.md) and [third-party notices](THIRD-PARTY-NOTICES.md).
