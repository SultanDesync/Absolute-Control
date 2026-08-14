# Native Menu Contract

> **Status:** Target product contract. Implemented exceptions and validation confidence are tracked
> in [current implementation state](CURRENT-STATE.md). In particular, dropdown labels, text/numeric
> editors, confirmation modals, and the target snapshot bridge below are not all implemented.

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

## Presentation and workspace schema

- Subscriber mods occupy one vertical sidebar row each. Provider pages never render in that
  sidebar.
- The sidebar contains no page buttons, setting controls, or permanent action cluster. Its width
  is determined only by readable mod names and navigation affordances.
- The selected mod's pages occupy a horizontal tab row across the top of the workspace.
- The center workspace is reserved for the active page's controls and selected-control help.
- The shell must use the 1920x1080 design canvas efficiently: control rows, headings, gutters,
  and actions are sized from a shared density scale rather than independently oversized boxes.
- Mouse hit regions are generated from the same layout constants used to draw each element. A
  movie/router revision mismatch must be rejected during packaging rather than shipped.
- Wheel input scrolls the region under the pointer. In the control workspace it moves the
  control viewport and selection, never silently changes a setting value. Sidebar and overflowing
  tab regions scroll only their own navigation collection.
- The native menu translates Starfield's direction-specific mouse `ButtonEvent`s (`0x800/+120`
  up and `0x900/-120` down) into the movie's pointer-wheel method. Flash `MOUSE_WHEEL` remains
  only a fallback because the
  current game path does not dispatch it to this custom movie. Duplicate native delivery of the
  same notch is suppressed before the movie is invoked.

The intended direction is Skyrim MCM's proven information architecture expressed in a
source-built Starfield/Absolute visual language, with one deliberate refinement: the compact mod
sidebar remains visible while browsing and editing. Selecting a mod replaces the horizontal page
tabs and workspace without entering another nested navigation level. This preserves one-click
switching between mods while retaining pages scoped to the selected mod, dense settings rows, a
scrolling options workspace, persistent focus, and contextual help for the selected setting. It
does not mean copying Bethesda/SkyUI assets or private implementation details merely to imitate
their artwork.

The density target is functional rather than decorative: at 1080p the workspace should normally
show roughly ten simple setting rows, with label and value aligned on one row. Long descriptions
belong in a selected-setting help region instead of making every option a tall card. Apply/Cancel
are compact footer actions or a dirty-state bar, not a permanent right-side column consuming
workspace.

The footer is an input-aware command bar, not a permanent keyboard tutorial. Apply, Cancel, and
Close/Back display the active device's standard glyph or short prompt and the entire displayed
prompt is a clickable pointer target. Apply and Cancel communicate their disabled/clean state;
Close/Back remains reachable. Routine movement instructions such as W/S and A/D are omitted once
standard menu navigation works. Context-specific prompts such as rebind, clear, dropdown open, or
slider adjustment appear only while their relevant control has focus.

Controls render as widgets appropriate to their semantics rather than uniform full-width cards:

- booleans use compact, clearly styled toggle switches;
- bounded integers and floats use a slider with an aligned numeric value/editor;
- enumerated values use a dropdown with provider-supplied labels;
- free-form bounded strings use a text box;
- bindings use a focused capture field with clear/rebind affordances;
- actions use content-sized buttons; and
- headers, dividers, help, and validation messages participate in measured flow layout without
  pretending to be editable settings.

The workspace lays these widgets out from measured content and shared spacing rules. Simple rows
may share label/value columns, while sliders, long text, binding fields, and grouped controls may
span available width. Fixed card heights and one-size-fits-all hitboxes are not part of the
release layout contract.

The component and authoring boundary is defined in
[the menu definition language](MENU-DEFINITION-LANGUAGE.md).

## Target product flow

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

## Target bridge surface

This is the desired long-term bridge shape. The executable prototype currently uses the bounded
flat `dispatch` ABI documented in [Bridge Protocol v1](BRIDGE-PROTOCOL-V1.md), plus direct
`applyModel` and pointer methods. Do not implement against this target list as if it were the
current ActionScript ABI.

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
