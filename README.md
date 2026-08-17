# Absolute Control Panel

Absolute Control Panel is a standalone Starfield/SFSE development project for a native shared
configuration-menu host. Its first target is moving the modular Absolute control suite from
Dear ImGui overlays to one native Starfield menu. It is not release-ready.

## AI-forward development

This project unapologetically adopts AI-assisted research, reverse engineering, implementation,
testing, and provider integration to advance Starfield modding.  It does not treat manual typing
as a proxy for quality.  It requires reproducible builds, narrow interfaces, fail-optional
integration, automated in-game evidence, privacy checks, and human intervention at consequential
decisions.  Generated work is judged by the same observable behavior and maintenance contract as
any other contribution.
It does not depend on, load, or alter AbsoluteHOTAS, Absolute Workbench, Absolute Power,
Absolute Head Tracking, or AbsoluteZero.

## Start here

- [Current implementation state](docs/CURRENT-STATE.md) — authoritative implemented, verified,
  pending, and specified capability matrix.
- [Design decisions](docs/DECISIONS.md) — architectural choices and their rationale.
- [Scalability, transactions, and teardown](docs/SCALABILITY-TRANSACTIONS-AND-TEARDOWN.md) —
  hundreds-scale target, dirty route/close policy, publication boundary, and teardown order.
- [SDK status](docs/SDK-STATUS.md) — public-contract sources and release gates.
- [Architecture map](docs/ARCHITECTURE.md) — product/research, native, ActionScript, threading,
  and dependency boundaries.
- [Absolute Control UX and visual design brief](docs/ABSOLUTE-CONTROL-DESIGN-BRIEF.md) —
  full-canvas layout, interaction, branding, Absolute Power screens, and design constraints.
- [Technical debt register](docs/DEBT-REGISTER.md) — resolved audit findings, current risks, and
  manual evidence still required.
- [Starfield runtime update runbook](docs/RUNTIME-UPDATE-RUNBOOK.md) — patch-cycle recovery and
  validation procedure.

The repository deliberately starts with the risky seam rather than product functionality:

- register an independent `GameMenuBase` with Starfield's UI manager;
- load a dedicated Scaleform movie without a D3D12/DXGI/WndProc overlay hook;
- prove keyboard and mouse behavior, then implement and prove controller and broader capture;
- expose a small, versioned ActionScript-to-C++ bridge;
- expose a versioned C ABI through which independently installed modules register pages and
  typed controls;
- discover a safe dedicated binding or additive PauseMenu launch entry;
- record enough evidence to decide whether a native replacement is supportable; and
- define the state, command, telemetry, and workflow surface required to approximate the
  current ImGui workbenches later, without copying daughter-module policy into this project.

## Current state

This branch contains a proven native-menu host and the first Absolute Control Panel provider ABI,
not a supported release.
It includes:

- a pinned maintained CommonLibSF submodule;
- an SFSE plugin target and native-menu registration probe;
- an explicit lifecycle state machine with unit tests;
- a source-built minimal ActionScript movie with a pinned compiler recipe;
- a current automated product validator plus componentized ResearchDev deploy, direct-launch,
  mailbox, evidence, screenshot, and watchdog tooling; disposable builder v1 is archived;
- retained guarded title, Continue, save-load, and PauseMenu research commands performed through
  game tasks rather than used as the default UX-validation path;
- a repeatable PauseMenu build/teardown and readiness trace;
- a copied-descriptor C ABI, synthetic subscriber, and fail-optional product subscribers including
  Absolute Power;
- an ImGui parity inventory and acceptance matrix; and
- staged research gates, failure policy, and promotion criteria.

The plugin registers after SFSE post-data-load. The verified host adds an entry to the populated
PauseMenu list at runtime, closes PauseMenu when selected, and opens a dedicated Scaleform movie.
It does not replace a vanilla SWF or own subscriber gameplay/configuration state. F2 remains the
fallback entry point.
See [the harness contract](docs/RESEARCH-HARNESS.md) for the evidence workflow.
The Starfield 1.16.244 relocation bridge needed by the current CommonLibSF snapshot is
documented in
[the compatibility note](docs/COMMONLIBSF-COMPATIBILITY.md).

The retained vertical slice is evidenced on Starfield 1.16.244, including repeated first-open
PauseMenu population in a heavily modified profile and a 25-open lifecycle regression on the
isolated baseline with no timeout, rejection, crash, or new dump. Dynamic subscriber pages, keyboard and mouse
navigation, draft/apply/cancel transactions, and provider-owned persistence cross the native and
Scaleform bridge. The first host-owned keyboard chord capture and Absolute Head Tracking's current
General, Axes, and Bindings pages are runtime-verified. Mods occupy the vertical sidebar and each selected mod's pages occupy
the horizontal tab row. No ESM/ESP is required.

The current architecture-hardening tree has passed the automated product process: release build,
9/9 native tests, 7 SDK generator tests, generated-header compile, 26-entry catalogue, complete
10-source SWF provenance, artifact fixtures, canonical manifest, and compatibility ZIP. Runtime
and UX are explicitly `not_run` for that tree.

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
[Absolute Control Panel module API](docs/MODULE-API.md),
[AI integration harness](docs/AI-INTEGRATION-HARNESS.md),
[builder evaluation runbook](docs/BUILDER-RUNBOOK.md),
[disposable builder-iteration policy](docs/process/DISPOSABLE-ITERATIONS.md),
[native menu contract](docs/NATIVE-MENU-CONTRACT.md), and
[menu definition language](docs/MENU-DEFINITION-LANGUAGE.md),
[live and compound components](docs/LIVE-COMPONENTS.md),
[SDK status and release checklist](docs/SDK-STATUS.md),
[ImGui parity matrix](docs/IMGUI-PARITY-MATRIX.md) for the complete definition.

The machine-readable development catalogue and local MCP server live in
[`catalog`](catalog/README.md).

## Build and test

Clone recursively, or initialize the pinned dependency after cloning:

```powershell
git submodule update --init --recursive
xmake
xmake test
.\tools\process\validate-current.cmd
```

The project requires Windows, MSVC with C++23 support, xmake 3.0 or newer, SFSE at runtime,
and Address Library for SFSE Plugins. Bootstrap and build the pinned interface toolchain with:

```powershell
.\tools\research\bootstrap-interface-toolchain.ps1
.\tools\research\build-interface.ps1
```

The default target is canonical `AbsoluteControlPanel.dll`. The opt-in
`AbsoluteControlPanelResearchDev` target contains automation, synthetic-provider, DirectInput, and
experimental live-component code and is explicitly non-packageable. Artifact manifests, not
directory scans, are the authority for deployment and package inputs.

Tests cover lifecycle, ABI readiness, copied descriptors, capacity admission, callback leases,
dirty unregister, refresh/generation, stale commands, source boundaries, and artifact integrity.
Executable tests complement rather than replace captured in-game evidence.

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
