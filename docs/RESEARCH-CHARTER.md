# Native Menu Research Charter

## Question

Can a standalone SFSE plugin host an Absolute control panel on Starfield's native menu stack,
launch it from the vanilla main and pause menus, and approximate the utility of the current Dear
ImGui implementations without assuming ownership of any daughter module?

This repository exists to answer that question with executable evidence. It is not a rewrite of
the suite and is not a place to develop flight, head-tracking, alignment, or power policy.

## Hypotheses

- CommonLibSF's `UI::RegisterMenu`, `GameMenuBase`, `UIMessageQueue`, and Scaleform bridge are
  sufficient for a custom menu on the supported Starfield runtime.
- The menu can own focus, cursor visibility, input modality, pausing, and back-stack behavior
  without D3D12/DXGI/WndProc interception.
- Vanilla launch entries can be composed at runtime. Direct replacement of `mainmenu.swf` or
  `pausemenu.swf` is a fallback with a known UI-mod conflict cost, not the default design.
- Renderer-neutral snapshots and commands can express the existing workbench utility while
  daughter APIs retain validation, preview, persistence, and gameplay ownership.
- Live HID telemetry and binding capture can cross the native UI boundary without visible lag or
  frame-driven filesystem work.

## Research gates

| Gate | Evidence required | Exit condition |
|---|---|---|
| R0 — Build | Recursive clean clone, plugin build, state tests | Reproducible locally and in CI |
| R1 — Movie | Reproducible AS3/SWF toolchain and minimal root movie | Registered menu opens and closes repeatedly |
| R2 — Bridge | Version negotiation, snapshot, command, error path | Bidirectional calls survive invalid input |
| R3 — Input | Keyboard, mouse, Xbox-style controller, focus trace | No stuck focus, double input, or leaked gameplay command |
| R4 — Launch | Main-menu and pause-menu entries | Both launch paths work without a global hotkey |
| R5 — Utility | Representative slider, toggle, binding capture, live graph, dirty modal | One vertical slice matches the ImGui workflow |
| R6 — Composition | Two fake providers discovered independently | Dynamic pages do not link module C++ objects |
| R7 — Compatibility | Runtime/resolution/UI-mod matrix and failure injection | Failure leaves the game and daughters usable |

Every gate records runtime version, SFSE version, CommonLibSF commit, Address Library version,
deployed files, reproduction steps, logs, screenshots/video where useful, and pass/fail notes.

## Failure policy

- Menu initialization fails closed and logs one actionable reason.
- No research failure may disable gameplay, alter daughter configuration, or prevent Starfield's
  vanilla menus from opening.
- A missing or incompatible SWF prevents registration or launch; it never creates a blank modal
  that traps input.
- An incompatible bridge version shows a diagnostic page with a reliable close path.
- A launch-entry experiment must be removable independently from the registered custom menu.

## Promotion criteria

Research may propose integration into Absolute Workbench only after R0–R7 are evidenced and the
following decisions are explicit:

- runtime support and update strategy;
- licensing and reproducibility of the SWF toolchain and assets;
- menu-mod conflict surface;
- module ABI and renderer-neutral presentation contract;
- measured telemetry and capture latency;
- accessibility, localization, controller, and ultrawide behavior; and
- rollback plan preserving the existing manual/ImGui path during migration.

Until then, `AbsoluteControlPanelResearch.dll` and its assets remain an experimental package and
must not be presented as a supported suite dependency.
