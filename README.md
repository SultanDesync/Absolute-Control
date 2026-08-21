# Absolute Control

Absolute Control is the native shared configuration menu for the modular Absolute Starfield
control suite. Independently installed SFSE plugins publish settings, actions, bindings, and live
status through a versioned C ABI while retaining ownership of their configuration and gameplay
behavior.

> **Status:** Pre-release product and SDK development. The native host and its first provider
> integrations are runtime-proven on Starfield 1.16.244, but the public SDK is not frozen.

## Branches

- `main` is the validated, buildable host baseline. Only the canonical `AbsoluteControlPanel`
  target is eligible for product packaging.
- `sdk` carries the evolving public headers, author examples, code generation, integration records,
  and preview contracts. ABI-breaking experiments stay there until they have a migration story and
  runtime evidence.
- The former
  [Absolute Control Panel Research](https://github.com/SultanDesync/Absolute-Control-Panel-Research)
  repository remains the historical laboratory and evidence trail.

The opt-in `AbsoluteControlPanelResearchDev` target is temporarily retained for regression tooling.
It is non-packageable and is not part of the product runtime.

## Product responsibilities

Absolute Control owns:

- the additive PauseMenu launch entry and dedicated native Scaleform menu;
- keyboard, mouse, and gamepad navigation;
- common control rendering, help text, dirty-state dialogs, Apply, Cancel, and close workflows;
- dynamic discovery and enumeration of independently installed provider modules; and
- versioned, capability-gated SDK contracts and optional suite coordination services.
- an experimental semantic-composition lane for host-owned cards, rows, status,
  provider-evaluated conditions, bounded anchor navigation, and associated live
  plots/range meters.

Provider mods continue to own:

- their settings model, validation, persistence, profiles, and reload behavior;
- binding meaning and collision policy;
- gameplay hooks, control-channel ownership, compatibility policy, and failure behavior; and
- their ability to run when Absolute Control is absent or incompatible.

There is no Dear ImGui or renderer interception in the product host, no ESM/ESP requirement, and no
hard link from the host to AbsoluteHOTAS, Absolute Power, Absolute Head Tracking, or AbsoluteZero.

## Current baseline

The promoted baseline includes:

- an additive PauseMenu entry and dedicated Scaleform movie; the standalone recovery hotkey is
  unbound by default and may be opted into through the INI;
- dynamic vertical module navigation and provider-owned page tabs;
- typed controls, actions, live components, selected-control help, and bounded text input;
- draft/apply/cancel transactions, guarded close routing, and stale-setting handling;
- provider-owned keyboard and physical-controller binding capture with native conflict reassignment;
- accessible segmented-grid editing, section headers, and compact inline action rows;
- copied descriptors, callback leases, capacity admission, teardown, and fail-optional discovery;
- runtime integrations for AbsoluteHOTAS, Absolute Head Tracking, AbsoluteZero, and Absolute Power;
  and
- a pinned maintained CommonLibSF dependency plus exact-version compatibility gating.

See [current state](docs/CURRENT-STATE.md), [architecture](docs/ARCHITECTURE.md),
[design decisions](docs/DECISIONS.md), [module API](docs/MODULE-API.md),
[SDK status](docs/SDK-STATUS.md), [subscriber UI standard](docs/SUBSCRIBER-UI-STANDARD.md),
[SDK release plan](docs/SDK-RELEASE-PLAN.md), and the
[technical debt register](docs/DEBT-REGISTER.md) for the authoritative implementation and release
gates.

## Build and test

Clone recursively, then build with Windows, MSVC C++23, and xmake 3.0 or newer:

```powershell
git clone --recurse-submodules https://github.com/SultanDesync/Absolute-Control.git
cd Absolute-Control
xmake
xmake test
.\tools\process\validate-current.cmd
```

The runtime also requires SFSE and Address Library for SFSE Plugins. Artifact manifests—not
directory scans—are authoritative for deployment and package contents.

The automated baseline covers native lifecycle and ABI behavior, SDK generation and compile tests,
source-built SWF provenance, artifact boundaries, and compatibility packaging. In-game validation
remains mandatory for changes to launch, input, provider transactions, or runtime integration.

## SDK development

SDK consumers dynamically query the host, validate ABI version, `structSize`, capabilities, and
required function pointers, and remain functional when the optional host is missing. Public headers
do not transfer state or configuration ownership to Absolute Control.

Work on `sdk` currently focuses on:

1. a header-only bridge between provider binding callbacks and the Absolute Input Bus;
2. canonical unbinding semantics;
3. labeled Choice and bounded `TextInput` definition-language support;
4. declarative live-channel code generation after those descriptors stabilize; and
5. packaging and cross-subscriber ABI qualification.

## Development standard

This project uses AI-assisted research, reverse engineering, implementation, and testing. Generated
work is held to the same standard as any contribution: narrow interfaces, reproducible builds,
observable runtime evidence, privacy checks, and human intervention at consequential decisions.

## Related projects

- [AbsoluteHOTAS](https://github.com/SultanDesync/AbsoluteHOTAS)
- [AbsoluteZero Ship Control](https://github.com/SultanDesync/AbsoluteZero-Ship-Control)
- [Maintained CommonLibSF](https://github.com/libxse/commonlibsf)

## License

GPL-3.0. See [LICENSE](LICENSE).
