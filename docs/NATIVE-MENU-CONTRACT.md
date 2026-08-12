# Native Menu Contract

## Ownership

The native shell owns presentation mechanics only:

- menu registration and launch;
- navigation, focus, scrolling, modals, and controller hints;
- view state such as expanded sections and selected route;
- rendering immutable snapshots; and
- translating user gestures into typed commands.

Providers own domain semantics:

- configuration schema and defaults;
- draft validation and sanitization;
- device discovery and capture interpretation;
- live preview and telemetry production;
- atomic save, reload, and read-back verification; and
- all gameplay/runtime hooks.

The research plugin must not parse another module's INI or receive STL/ImGui objects across a DLL
boundary.

## Proposed flow

```text
vanilla launch entry
  -> UIMessageQueue show AbsoluteControlPanelMenu
  -> SWF ready(bridgeVersion)
  -> C++ returns shell snapshot + discovered provider descriptors
  -> SWF renders route
  -> user gesture emits typed command
  -> provider validates/mutates draft
  -> C++ publishes a replacement snapshot
  -> save invokes provider save/reload/read-back
  -> close is allowed or routed through the dirty-state modal
```

Snapshots are immutable and generation-numbered. Commands carry the generation they were based
on so stale UI gestures can be rejected instead of overwriting newer state.

## Minimal bridge v1

ActionScript to C++:

- `ready(bridgeVersion)`;
- `dispatchCommand(bridgeVersion, commandName, payload)`;
- `requestSnapshot(bridgeVersion, knownGeneration)`;
- `reportFocus(bridgeVersion, controlId, inputDevice)`; and
- `closeRequested(bridgeVersion)`.

C++ to ActionScript:

- `applySnapshot(snapshot)`;
- `showOperationResult(result)`;
- `setInputMode(mode)`;
- `showFatalDiagnostic(diagnostic)`; and
- `closeAccepted()`.

Payloads are bounded and validated. Unknown fields are ignored for additive compatibility;
unknown command names, incompatible major versions, excessive arrays/strings, and invalid numeric
values are rejected with a diagnostic.

## Representative view model

The vertical-slice snapshot must cover the hard interaction types before broader porting:

- route/category/page descriptors;
- text, boolean, enum, integer, and floating settings;
- enabled/disabled and visible/hidden dependencies;
- a device binding field with capture status;
- a bounded live series for an axis/pose graph;
- dirty, validation, save, reload, and read-back status;
- modal state and requested close reason; and
- accessibility text and current input-device hints.

## Threading and cadence

- All Scaleform object calls occur on the game/UI thread.
- Provider telemetry is copied into bounded process-owned snapshots; no provider pointer is held
  by ActionScript.
- Live data is rate-limited and coalesced. Configuration is not read from disk per frame.
- Capture and save commands are asynchronous from the UI's perspective and report completion by
  generation/result events.

## Launch integration

The preferred implementation adds one `ABSOLUTE CONTROL PANEL` entry to the vanilla model at
runtime and handles its selection without replacing the entire vanilla SWF. The research record
must identify the data provider/event seam for both `MainMenu` and `PauseMenu`.

If runtime composition is impossible, an SWF-patch fallback must document exact conflicting files,
load-order behavior, update maintenance, localization implications, and how other UI mods can
patch or opt out. That fallback cannot be promoted silently.
