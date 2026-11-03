# Absolute Control

Flight controls for Starfield: HOTAS and joystick flight, mouse steering, and head
tracking, built on one input bus.

> **Status: rewrite in progress (`rewrite` branch). Nothing here is released.**
> The current release is Absolute Suite / Absolute Control **0.3.0-beta.1**. Its
> source is on [`release/0.3`](../../tree/release/0.3) and tag `v0.3.0-beta.1`.
> HOTAS 5.2.0, Head Tracking 1.0.0-beta.1, AbsoluteZero 1.3.0 and Absolute Power
> 0.3.0-alpha stay frozen in their own repositories until this rewrite replaces them.

## Why a rewrite

The 0.3 suite was five mods, a bespoke native menu framework, an SDK and a component
catalog. Most of the maintenance cost sat in the menu host and in the boundaries
between mods, not in flight control. This rewrite keeps the flight-control features
and drops the rest.

## Scope

| In | Out |
|---|---|
| HOTAS / joystick axes, throttle and buttons | The bespoke Scaleform menu host and SDK |
| Mouse steering (formerly AbsoluteZero), as a small optional module | Macros |
| Head tracking, as three more axes on the input bus | Absolute Power |
| Simple profiles, with an importer for existing INIs | Absolute Workbench, the external configurator |

## Architecture

```text
device providers ──► input bus ──► bindings + profiles ──► flight / camera outputs
 DirectInput          copied frames,                         one writer per output
 OpenTrack (planned)  device identity,
                      lifecycle, capture
```

- **Input bus** (`src/runtime/`). One runtime owns every device provider and
  publishes copied, sequenced frames. Readers never touch hardware. It starts
  independently of any menu and refuses to run beside AbsoluteHOTAS.
- **Head tracking** becomes an OpenTrack provider publishing yaw, pitch and roll
  channels. The camera reads bus channels like any other binding.
- **Configuration** is an INI per feature with strong presets. The in-game UI is a
  thin ImGui page on [SFSE Menu Framework](https://www.nexusmods.com/starfield/mods/18201),
  kept as plain ImGui render functions so the host could be swapped.
- **Mouse steering** is configured by INI, presets and a few hotkeys, plus one
  slider page.

The design record behind these choices is in [`docs/history/`](docs/history/).

## What exists today

- The headless input runtime: `DeviceStore`, `BackendRuntime`, `DirectInputProvider`,
  `HeadlessCore`, `LegacyHotasToken`, with tests.
- A plugin that starts the runtime at SFSE post-post-load and logs the device
  inventory at post-data-load. There are no game hooks, outputs or UI yet.

## Building

Requires Visual Studio 2026 Build Tools (C++23) and xmake 3.x.

```bash
git submodule update --init --recursive
xmake f -c -p windows -a x64 -m releasedbg -y
xmake -y
xmake test
```

Pass `-p windows` explicitly. From Git Bash, xmake otherwise selects Git's bundled
MinGW and package installs fail. The build never deploys itself. Copy
`build/windows/x64/releasedbg/AbsoluteControl.dll` into `Data/SFSE/Plugins` by hand.

## License

See [LICENSE](LICENSE), [CREDITS](CREDITS.md) and [third-party notices](THIRD-PARTY-NOTICES.md).
