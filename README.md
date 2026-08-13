# Starfield Local Options Panel (SLOP)

SLOP - the **Starfield Local Options Panel** - is a standalone Starfield/SFSE research project
for proving a native Mod Configuration Menu host.  Its first target is moving the modular
Absolute control suite from Dear ImGui overlays to one shared native Starfield menu.

## AI-forward development

This project unapologetically adopts AI-assisted research, reverse engineering, implementation,
testing, and provider integration to advance Starfield modding.  It does not treat manual typing
as a proxy for quality.  It requires reproducible builds, narrow interfaces, fail-optional
integration, automated in-game evidence, privacy checks, and human intervention at consequential
decisions.  Generated work is judged by the same observable behavior and maintenance contract as
any other contribution.
It does not depend on, load, or alter AbsoluteHOTAS, Absolute Workbench, Absolute Power,
Absolute Head Tracking, or AbsoluteZero.

The repository deliberately starts with the risky seam rather than product functionality:

- register an independent `GameMenuBase` with Starfield's UI manager;
- load a dedicated Scaleform movie without a D3D12/DXGI/WndProc overlay hook;
- prove keyboard, mouse, and controller focus, navigation, capture, and close behavior;
- expose a small, versioned ActionScript-to-C++ bridge;
- expose a versioned C ABI through which independently installed modules register pages and
  typed controls;
- discover a safe dedicated binding or additive PauseMenu launch entry;
- record enough evidence to decide whether a native replacement is supportable; and
- define the state, command, telemetry, and workflow surface required to approximate the
  current ImGui workbenches later, without copying daughter-module policy into this project.

## Current state

This branch contains a proven native-menu probe and the first SLOP provider ABI, not a supported
MCM release.
It includes:

- a pinned maintained CommonLibSF submodule;
- an SFSE plugin target and native-menu registration probe;
- an explicit lifecycle state machine with unit tests;
- a source-built minimal ActionScript movie with a pinned compiler recipe;
- a bounded deploy/launch/evidence/screenshot/watchdog harness;
- guarded title, Continue, save-load, and PauseMenu automation performed through game tasks;
- a high-signal magenta framebuffer oracle and repeatable PauseMenu build/teardown loop;
- a copied-descriptor C ABI and synthetic subscriber with toggle, slider, and binding controls;
- an ImGui parity inventory and acceptance matrix; and
- staged research gates, failure policy, and promotion criteria.

The plugin registers only after SFSE post-data-load.  The verified rendering probe opens a
dedicated Scaleform movie, completes a versioned ActionScript/native bridge handshake, and
force-closes through a native watchdog.  The active harness now requires PauseMenu to be closed
before SLOP opens; the earlier overlay arrangement is not an accepted product design.  It does not
inject buttons into vanilla menus, replace a vanilla SWF, or own gameplay/configuration state.
See [the harness contract](docs/RESEARCH-HARNESS.md) for the evidence workflow.
The Starfield 1.16.244 relocation bridge needed by the current CommonLibSF snapshot is
documented in
[the compatibility note](docs/COMMONLIBSF-COMPATIBILITY.md).

The current vertical slice is evidenced on Starfield 1.16.244. The movie and native bridge
executed, the screenshot contained 16,384 sentinel pixels, and SLOP opened only after the runner
proved PauseMenu was closed. The synthetic module registered three controls through ABI version
1; native keyboard commands changed the toggle and level, vJoy device 1 supplied an enumerated
button binding, provider-owned persistence matched, and explicit close returned directly to
gameplay with PauseMenu still closed. No ESM/ESP is required for the DLL-to-menu-to-SWF path.

## Research outcome

The experiment succeeds only if a native menu can provide all of the following without a
graphics overlay hook:

1. launch from a dedicated binding or an additive PauseMenu entry without replacing PauseMenu;
2. deterministic keyboard, mouse, and gamepad operation;
3. safe dirty-state, save, discard, modal, and close workflows;
4. hardware binding capture and live telemetry at acceptable latency;
5. dynamic navigation for independently installed Absolute modules;
6. compatibility with supported resolutions, UI scaling, and common menu mods; and
7. a version-resilient failure mode that cannot disable daughter gameplay.

See [the research charter](docs/RESEARCH-CHARTER.md),
[SLOP module API](docs/MODULE-API.md),
[AI integration harness](docs/AI-INTEGRATION-HARNESS.md),
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
and Address Library for SFSE Plugins. Bootstrap and build the pinned interface toolchain with:

```powershell
.\tools\research\bootstrap-interface-toolchain.ps1
.\tools\research\build-interface.ps1
```

The tests cover lifecycle transitions plus API query/version behavior, descriptor copying,
duplicate rejection, refresh, and unregistration. Executable tests complement rather than
replace captured in-game evidence.

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
