# Native Menu Research Charter

> **Status:** Historical research charter. It preserves the original questions and SLOP-era gate
> names. Use [current implementation state](CURRENT-STATE.md), [design decisions](DECISIONS.md), and
> [the current test matrix](TEST-MATRIX.md) for present claims.

## Question

Can a standalone SFSE plugin host a shared SLOP configuration menu on Starfield's native menu
stack, open it through a dedicated binding or additive PauseMenu interaction, and approximate
the utility of current Dear ImGui implementations without assuming ownership of any provider?

This repository exists to answer that question with executable evidence. It is not a rewrite of
the suite and is not a place to develop flight, head-tracking, alignment, or power policy.

## Hypotheses

- CommonLibSF's `UI::RegisterMenu`, `GameMenuBase`, `UIMessageQueue`, and Scaleform bridge are
  sufficient for a custom menu on the supported Starfield runtime.
- The menu can own focus, cursor visibility, input modality, pausing, and back-stack behavior
  without D3D12/DXGI/WndProc interception.
- A product launch route can be composed at runtime through a dedicated binding or additive
  PauseMenu interaction. Replacing `mainmenu.swf` or `pausemenu.swf`, or leaving the custom menu
  layered over PauseMenu, is outside the accepted design.
- Renderer-neutral snapshots and commands can express the existing workbench utility while
  daughter APIs retain validation, preview, persistence, and gameplay ownership.
- Live HID telemetry and binding capture can cross the native UI boundary without visible lag or
  frame-driven filesystem work.

## Research gates

| Gate | Evidence required | Exit condition |
|---|---|---|
| R0 - Build | Recursive clean clone, plugin build, state tests | Reproducible locally and in CI |
| R1 - Movie | Pinned AS3/SWF build recipe and minimal root movie | Registered menu opens and closes repeatedly |
| R2 - Bridge | Version negotiation, snapshot, command, error path | Bidirectional calls survive invalid input |
| R3 - Input | Keyboard, mouse, Xbox-style controller, focus trace | No stuck focus, double input, or leaked gameplay command |
| R4 - Composition | Two providers discovered through the C ABI | Dynamic pages do not link module C++ objects |
| R5 - Launch | Dedicated binding or additive PauseMenu interaction | SLOP opens exclusively and returns to the correct prior state |
| R6 - Utility | Representative slider, toggle, binding capture, live graph, dirty modal | One vertical slice matches the ImGui workflow |
| R7 - Compatibility | Runtime/resolution/UI-mod matrix and failure injection | Failure leaves the game and providers usable |

Every gate records runtime version, SFSE version, CommonLibSF commit, Address Library version,
deployed files, reproduction steps, logs, screenshots/video where useful, and pass/fail notes.

## Current result

Retained ResearchDev evidence satisfies R1 for Starfield 1.16.244. That dedicated movie constructed
and drew, performed a native `ready(1)` bridge callback, entered the active render-order array,
produced the required framebuffer sentinel, and closed through its research watchdog without
damaging vanilla PauseMenu. Repeated PauseMenu build/teardown cycles also passed in one retained
save. This is historical research evidence, not a watchdog dependency in the canonical host: the
current release path treats a missing bridge root as `RuntimeFault` and queues an explicit hide.

The bridge has also completed keyboard command and immutable snapshot round-trips. A synthetic
provider registers a toggle, slider, and binding through ABI version 1. A complete isolated run
proved PauseMenu closed before SLOP opened, API-backed changes and persistence, enumerated vJoy
button capture, and explicit close back to gameplay with PauseMenu still closed. Dynamic
rendering from two independently compiled providers, full controller navigation, and a product
invocation route remain open gates.

## Failure policy

- Menu initialization fails closed and logs one actionable reason.
- No research failure may disable gameplay, alter daughter configuration, or prevent Starfield's
  vanilla menus from opening.
- A missing or incompatible SWF prevents registration or launch; it never creates a blank modal
  that traps input.
- An incompatible bridge version shows a diagnostic page with a reliable close path.
- A launch-entry experiment must be removable independently from the registered custom menu.

## Promotion criteria

Research may propose integration into the Absolute ecosystem only after R0-R7 are evidenced and the
following decisions are explicit:

- runtime support and update strategy;
- licensing and reproducibility of the SWF toolchain and assets;
- menu-mod conflict surface;
- module ABI and renderer-neutral presentation contract;
- measured telemetry and capture latency;
- accessibility, localization, controller, and ultrawide behavior; and
- rollback plan preserving the existing manual/ImGui path during migration.

Until then, canonical `AbsoluteControlPanel.dll` remains an unsupported development product and
`AbsoluteControlPanelResearchDev.dll` remains explicitly non-packageable. Neither may be presented
as a supported suite dependency; the retired `AbsoluteControlPanelResearch.dll` is never deployed.
