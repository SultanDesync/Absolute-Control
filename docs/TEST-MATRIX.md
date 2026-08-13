# In-Game Test Matrix

Each result records exact versions, deployed artifacts, logs, reproduction steps, and evidence.

## Current PauseMenu regression evidence

The current Starfield 1.16.244 baseline build (`SHA256
2A87059AE94B4B5A75B4423B7092E55FBC82F3CB5A56C9AA2C44D409886A9F16`) completed 25
consecutive PauseMenu opens in one retained loaded save. One Control Panel show/hide was followed
by five of those cycles to reproduce the previously observed close-then-pause failure shape.

- 25 insertion boundaries, advance listeners, entry injections, and completed cycles;
- zero advance timeouts, listener rejections, injection rejections, or injection exceptions;
- every PauseMenu boundary, advance callback, and mutation occurred on one recorded UI/Scaleform
  thread; and
- Starfield remained responsive and Windows Error Reporting produced no new dump.

This is a regression result for the isolated baseline. The earlier heavily modified profile remains
the broad compatibility result; neither result replaces the remaining context, input, display, and
failure-injection matrix below.

## Current slider-drag regression evidence

The 2026-08-13 Testing Baseline build (`AbsoluteControlPanel.dll` SHA256
`407B1A8E509830B1580BF959A032167B4E148FC35CCCD61CD646FFB249BED207` and
`AbsoluteControlPanelMenu.swf` SHA256
`9387416553805EF554ECDEFA4F6B7126E25B51775FF3B0142A78196AA0A26917`) was manually
validated in game.

- click-to-position remained functional;
- held pointer movement continuously updated slider values in both directions;
- dragging survived the model redraw caused by every draft write;
- releasing the pointer ended the drag cleanly; and
- runtime evidence recorded one down, multiple writes, and one up for each observed gesture.

## Contexts

- initial title/main menu before loading a save;
- pause menu on foot, in a ship seat, docked, landed, and in space;
- menu opened and closed repeatedly;
- save/load transition, death/reload, fast travel, and return to main menu;
- alt-tab, display-mode change, resolution change, and controller hot-plug; and
- missing, corrupt, old, and newer-bridge SWF failure cases.

## Input

- mouse-only;
- slider click-to-position and held dragging in both directions, including leaving the track while
  held and confirming that release ends capture;
- wheel scrolling over the mod sidebar, overflowing page tabs, and control workspace, including
  confirmation that wheel input does not mutate a value;
- keyboard-only including Tab/Shift+Tab/arrows/Enter/Escape;
- Xbox-compatible controller only;
- transition between mouse and controller while open;
- HOTAS binding capture with noisy axes and held buttons; and
- close during capture, save, telemetry update, and modal presentation.

## Display

- 1280x720, 1920x1080, 2560x1440, and 3840x2160;
- 21:9 and 32:9 ultrawide;
- every supported Starfield UI scaling/accessibility setting; and
- long pseudo-localized labels and missing-glyph diagnostics.

## Compatibility and failure injection

- vanilla UI;
- representative main-menu and pause-menu SWF mods;
- representative inventory/HUD/UI-overhaul mods;
- frame generation and common overlay/capture tools, confirming no project render hook;
- provider absent, incompatible, throwing/failing, and disappearing during the session;
- save failure, reload failure, and read-back mismatch; and
- Starfield/SFSE update with stale Address Library or CommonLibSF metadata.

## Required invariants

- Vanilla menus remain usable after any failure.
- The research menu always has a reachable close path.
- No input is emitted to gameplay while capture owns the session.
- Held inputs are reseeded before gameplay resumes.
- No disk access occurs from render/advance callbacks.
- No unbounded ActionScript payload, telemetry queue, or string crosses the bridge.
- Visible pointer targets and native hit regions come from the same layout revision and geometry.
- A disabled or absent research plugin changes no save data and requires no cleanup.
