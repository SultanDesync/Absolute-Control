# Absolute Control Panel Research

`Absolute-Control-Panel-Research` is a standalone Starfield/SFSE research project for proving
that the Absolute control suite can move from a Dear ImGui overlay to a native Starfield menu.
It does not depend on, load, or alter AbsoluteHOTAS, Absolute Workbench, Absolute Power,
Absolute Head Tracking, or AbsoluteZero.

The repository deliberately starts with the risky seam rather than product functionality:

- register an independent `GameMenuBase` with Starfield's UI manager;
- load a dedicated Scaleform movie without a D3D12/DXGI/WndProc overlay hook;
- prove keyboard, mouse, and controller focus, navigation, capture, and close behavior;
- expose a small, versioned ActionScript-to-C++ bridge;
- discover safe main-menu and pause-menu launch entries;
- record enough evidence to decide whether a native replacement is supportable; and
- define the state, command, telemetry, and workflow surface required to approximate the
  current ImGui workbenches later, without copying daughter-module policy into this project.

## Current state

This bootstrap is a **compile-time and research-contract baseline**, not a usable menu mod.
It includes:

- a pinned maintained CommonLibSF submodule;
- an SFSE plugin target and native-menu registration probe;
- an explicit lifecycle state machine with unit tests;
- a draft ActionScript boundary for the future SWF;
- an ImGui parity inventory and acceptance matrix; and
- staged research gates, failure policy, and promotion criteria.

The plugin registers only after SFSE post-data-load. It does not inject buttons into vanilla
menus and does not open the probe automatically. The registration path remains disabled until
a reproducible SWF toolchain and an in-game smoke test establish the menu root contract.

## Research outcome

The experiment succeeds only if a native menu can provide all of the following without a
graphics overlay hook:

1. launch from both Starfield's main menu and pause menu;
2. deterministic keyboard, mouse, and gamepad operation;
3. safe dirty-state, save, discard, modal, and close workflows;
4. hardware binding capture and live telemetry at acceptable latency;
5. dynamic navigation for independently installed Absolute modules;
6. compatibility with supported resolutions, UI scaling, and common menu mods; and
7. a version-resilient failure mode that cannot disable daughter gameplay.

See [the research charter](docs/RESEARCH-CHARTER.md),
[native menu contract](docs/NATIVE-MENU-CONTRACT.md), and
[ImGui parity matrix](docs/IMGUI-PARITY-MATRIX.md) for the complete definition.

## Build and test

Clone recursively, or initialize the pinned dependency after cloning:

```powershell
git submodule update --init --recursive
xmake
xmake test
```

The project requires Windows, MSVC with C++23 support, xmake 3.0 or newer, SFSE at runtime,
and Address Library for SFSE Plugins. A build proves only the C++ boundary. The SWF toolchain
and in-game deployment are intentionally later research gates.

## Repository boundaries

- No Dear ImGui, renderer interception, or window-procedure hook.
- No gameplay injection, configuration ownership, or daughter-module linking.
- No replacement of a vanilla SWF in a released artifact unless runtime composition is proven
  impossible and the compatibility cost is explicitly accepted.
- No claim of product readiness until every promotion gate is backed by captured evidence.

## Related projects

- [AbsoluteHOTAS](https://github.com/SultanDesync/AbsoluteHOTAS)
- [AbsoluteZero Ship Control](https://github.com/SultanDesync/AbsoluteZero-Ship-Control)
- [Maintained CommonLibSF](https://github.com/libxse/commonlibsf)

## License

GPL-3.0. See [LICENSE](LICENSE).
