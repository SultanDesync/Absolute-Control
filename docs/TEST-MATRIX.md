# In-Game Test Matrix

Each result records exact versions, deployed artifacts, logs, reproduction steps, and evidence.

## Contexts

- initial title/main menu before loading a save;
- pause menu on foot, in a ship seat, docked, landed, and in space;
- menu opened and closed repeatedly;
- save/load transition, death/reload, fast travel, and return to main menu;
- alt-tab, display-mode change, resolution change, and controller hot-plug; and
- missing, corrupt, old, and newer-bridge SWF failure cases.

## Input

- mouse-only;
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
- A disabled or absent research plugin changes no save data and requires no cleanup.
