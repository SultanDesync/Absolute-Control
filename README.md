# Absolute Control — Native Mod Options for Starfield

Absolute Control is a shared native configuration menu for modular Starfield SFSE plugins.
Independent modules register settings, actions, bindings, profiles, live telemetry, and diagnostics
inside one consistent **MOD OPTIONS** interface while retaining ownership of their own configuration
and gameplay behavior.

The current release line is Absolute Suite `0.2.0-beta.1`, a guided all-in-one installer containing
Absolute Control plus optional HOTAS, Power, Head Tracking, and mouse-steering modules.

## Why “Absolute”?

The project began by solving absolute throttle control: a physical throttle position should map
directly to a ship's requested thrust instead of behaving like a relative increase/decrease key.
That input scheme gave AbsoluteHOTAS—and eventually the wider modular ecosystem—its name.

## Included ecosystem

- **Absolute Control** — required native menu host, shared input capture, telemetry presentation,
  module registry, and suite diagnostics.
- **AbsoluteHOTAS 5.0.1** — DirectInput HOTAS/HOSAS flight control, ship buttons, throttle tuning,
  profiles, layers, macros, and calibration.
- **Absolute Power 0.2.0-alpha** — priority-based ship-power presets and allocation controls.
- **Absolute Head Tracking 0.2.0-alpha** — OpenTrack-compatible rotational cockpit tracking.
- **AbsoluteZero 0.2.0** — mouse alignment and optional locked-reticule steering.

Every gameplay module remains independently installable and continues to function when Absolute
Control is absent or incompatible.

## Requirements

- Starfield `1.16.244`
- [SFSE](https://sfse.silverlock.org/) `0.2.20` or later
- Address Library for SFSE Plugins for the Absolute Control host
- OpenTrack only when using Absolute Head Tracking
- No ESM or ESP

The gameplay modules do not all require Address Library; the all-in-one suite lists it because the
Absolute Control host currently does.

## Installation and use

1. Install the Absolute Suite archive with MO2 or Vortex.
2. Use the guided FOMOD to select the gameplay modules you want.
3. Launch Starfield through SFSE.
4. Open the Pause Menu and select **MOD OPTIONS**.
5. Choose a module from the left sidebar and configure its pages.

Absolute Control provides common navigation, Apply/Cancel, dirty-close dialogs, binding capture,
help text, and live presentation. The selected module still validates and persists every change.

## Current menu capabilities

- Dynamic module registration and page tabs.
- Pointer, keyboard, and gamepad navigation.
- Toggles, sliders, choices, text, actions, bindings, profiles, and record collections.
- Keyboard and provider-owned HOTAS/controller capture with conflict resolution.
- Live graphs, range markers, allocation grids, telemetry, and direct tuning controls.
- Pinned editing context for profile and shift-layer selection.
- Provider-owned Apply, Cancel, validation, persistence, and fallback behavior.
- Registered-module, API-bus, subscriber, and runtime diagnostics.
- Configuration/registration sorting and recovery-key preferences in the built-in Control page.

The product menu is a dedicated native Scaleform interface. It does not use Dear ImGui, replace
`pausemenu.swf`, intercept a graphics renderer, or require gameplay modules to link against the
host.

## Modular ownership

Absolute Control never takes ownership of gameplay hooks or module configuration files. This keeps
the suite composable and prevents one menu from becoming a hard runtime dependency.

Important paired behavior:

- When AbsoluteZero and AbsoluteHOTAS are installed together, AbsoluteZero owns pitch/yaw mouse
  steering and HOTAS yields those stick axes. HOTAS retains roll, strafe, throttle, buttons, and
  profiles.
- When standalone Absolute Head Tracking is installed with HOTAS, Head Tracking owns cockpit camera
  composition and HOTAS parks its embedded legacy tracker.
- Absolute Power owns preset allocation and persistence; HOTAS only provides its optional Input Bus.

## Standalone module pages

- [Absolute Head Tracking](https://www.nexusmods.com/starfield/mods/17872)
- [AbsoluteZero](https://www.nexusmods.com/starfield/mods/17460)
- [AbsoluteHOTAS](https://www.nexusmods.com/starfield/mods/16668)
- [Absolute Power](https://github.com/SultanDesync/Absolute-Power) — standalone Nexus page coming
  soon

The standalone pages remain authoritative for detailed setup instructions, compatibility notes,
and module-specific changelogs.

## SDK beta access

Absolute Control is also an integration SDK for other SFSE mods. Providers can register native
pages, settings, input capture, live telemetry, and optional shared services without transferring
gameplay ownership to the host.

The SDK is currently a coordinated private beta. Mod authors interested in an integrated menu can
message the author through Nexus Mods to apply for beta access and integration support. The public
SDK release is coming soon.

See the [SDK overview](sdk/README.md), [SDK status](docs/SDK-STATUS.md),
[module API](docs/MODULE-API.md), and [subscriber UI standard](docs/SUBSCRIBER-UI-STANDARD.md).

## Building

Clone recursively, then build with Windows, MSVC C++23, and xmake 3.0 or newer:

```powershell
git clone --recurse-submodules https://github.com/SultanDesync/Absolute-Control.git
cd Absolute-Control
xmake f -m release
xmake build AbsoluteControlPanel
.\tools\process\build-interface.ps1
```

Artifact manifests—not directory scans—are authoritative for deployment and package contents.
In-game validation remains mandatory for changes to launch, input, provider transactions, or native
runtime integration.

## License

GPL-3.0. See [LICENSE](LICENSE).
