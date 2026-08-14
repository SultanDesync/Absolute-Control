# In-game test matrix

> **Status:** Current evidence ledger and pending regression list. A row marked pending is not a
> failed feature claim; it is work that has not yet earned the stated confidence.

Each accepted result records the game/runtime versions, host and SWF artifacts, profile class,
steps, semantic evidence, crash observation, and whether human UX judgment was involved. Local
logs and screenshots remain ignored.

## Status vocabulary

- **Verified** — completed with the evidence appropriate to the behavior.
- **Observed** — manually seen and useful, but lacks a complete artifact/evidence record.
- **Build verified** — automated tests pass; in-game validation is incomplete.
- **Pending** — scenario not completed on the current implementation.
- **Superseded** — result belongs to an earlier implementation and is retained only as context.

## Accepted runtime evidence

The current architecture-hardening tree has no new runtime/UX result. Its automated product
process passed the release build, 8/8 native tests, 6 SDK tests, generated fixture check/compile,
25-entry catalogue, complete ten-source SWF provenance, artifact fixtures, canonical manifest, and
compatibility ZIP. Runtime rows below are retained evidence from earlier artifacts unless stated.

| Area | Status | Environment/evidence | Result |
|---|---|---|---|
| Native menu registration and movie bridge | Verified | Starfield 1.16.244 isolated baseline; lifecycle and bridge events | Dedicated menu factory, source SWF, `_root` bridge, model application, show/hide, and return to gameplay succeeded. |
| Additive PauseMenu entry, isolated | Verified | Host DLL SHA256 `2A87059AE94B4B5A75B4423B7092E55FBC82F3CB5A56C9AA2C44D409886A9F16`; 25 retained-save cycles | 25 boundaries, listeners, injections, and completed cycles; zero timeouts, rejections, new dumps, or observed crashes. One Control Panel show/hide was followed by five more cycles. |
| Additive PauseMenu entry, heavy profile | Verified for broad compatibility | Heavily modified 600+ mod profile with multiple UI mods | Entry populated without replacing a vanilla SWF and subsequently opened consistently. This is broad compatibility evidence, not universal compatibility. |
| Multiple subscriber modules | Observed | Absolute Head Tracking plus AbsoluteZero installed together | Both modules appeared in the vertical sidebar and exposed usable, isolated pages/tabs. |
| Absolute Head Tracking | Verified | Three-page integration on Starfield 1.16.244 | General, one scrollable 12-control Axes page, and Bindings rendered and applied provider-owned values. |
| AbsoluteZero | Observed | Paired subscriber run plus build/contract tests | Mouse Alignment page was present and usable. Exact subscriber artifact/persistence transcript was not retained, so the catalog remains conservatively build-verified. |
| Keyboard binding capture | Verified | Head Tracking integration | Single keys and Ctrl/Alt/Shift chords recorded through the native input stream and round-tripped through provider draft/persistence. Escape cancel and clear behavior have automated coverage. |
| Slider pointer drag | Verified | DLL SHA256 `407B1A8E509830B1580BF959A032167B4E148FC35CCCD61CD646FFB249BED207`; SWF SHA256 `9387416553805EF554ECDEFA4F6B7126E25B51775FF3B0142A78196AA0A26917` | Click-to-position, continuous bidirectional movement, redraw survival, reversal, and clean release worked. Runtime evidence recorded down, repeated writes, and up; human judgment rated the sliders as working well. |
| Mouse pointer navigation | Observed | Current semantic hit-target movie | Module/page/control/footer clicks were usable; rendered sprites owned hit testing. |
| Apply and Cancel | Observed plus automated coverage | Head Tracking and AbsoluteZero providers | Apply reached provider persistence; Cancel restored provider state. Cross-page dirty rejection and close rollback have native tests. |

## Known observations requiring continued monitoring

- Two infrequent PauseMenu/close crashes occurred in earlier builds. The later lifecycle-owned
  revision was introduced after those reports and did not reproduce them in 25 consecutive cycles.
  This reduces but does not close crash monitoring.
- An earlier scripted controller research probe left apparent right-stick values frozen near a
  negative diagonal even though vJoy monitoring appeared centered. Disabling vJoy/Steam Input and
  removing that automation restored normal mouse authority. Product builds must not drive a
  controller during startup or menu use.
- Initial PauseMenu population in the heavy profile varied across early builds, sometimes requiring
  repeated opens. The event-driven insertion revision later populated on first entry in observed
  runs. Patch/profile testing must continue to record first-open versus retry behavior.

## Pending input validation

| Scenario | Status | Required evidence |
|---|---|---|
| Mouse wheel up and down | Pending clean regression | Scroll overflowing Axes controls in both directions, plus hovered sidebar and overflowing tabs; confirm no setting mutation. |
| Keyboard-only full route | Pending formal regression | Open from PauseMenu/F2, traverse modules/pages/controls/footer, edit, Apply, Cancel, and Close without mouse. |
| Xbox-compatible controller only | Not connected | The native menu currently rejects gamepad events. Implement routing/suppression first, then prove full navigation, edit, Apply/Cancel/Close, no leaked command, and no ghost/stuck axis. |
| Input-device transition | Partly blocked | Mouse/keyboard switching needs a current regression; controller transition/reseed awaits controller routing. |
| Mouse binding capture | Not implemented | Capture, clear, cancel, conflict behavior, and round-trip. |
| Controller/HOTAS binding capture | Not implemented | Device identity, noisy axes, held buttons/POV, cancel/timeout, and reseed. |
| Close during capture/save/live update | Pending | Reliable cancel/rollback and no replayed input. |

## Pending context and lifecycle validation

| Scenario | Status |
|---|---|
| Main/title menu load with no attempt to show the panel | Pending formal regression |
| PauseMenu on foot, ship seat, docked, landed, and in space | Partial/observed; matrix incomplete |
| Save/load, death/reload, fast travel, and return to main menu | Pending |
| Alt-tab, focus loss, display-mode change, and controller hot-plug | Pending |
| Missing, corrupt, old, newer, or wrong-root bridge SWF | Pending failure injection; source tests cover canonical wrong-root fail-closed hide |
| External hide/destruction while a provider page is dirty | Session-destructor rollback is automated; engine-driven external hide remains pending runtime validation. |
| Provider absent, incompatible, rejecting, failing, or disappearing | Headless coverage; in-game failure injection pending |
| Save/reload/read-back failure | Pending failure injection |
| Starfield/SFSE update with stale Address Library or mappings | Pending; use `RUNTIME-UPDATE-RUNBOOK.md` |

## Pending display validation

| Scenario | Status |
|---|---|
| 1280x720 | Pending |
| 1920x1080 | Verified design canvas; broader UI-scale combinations pending |
| 2560x1440 | Observed; formal layout record pending |
| 3840x2160 | Pending |
| 21:9 and 32:9 | Observed during research; current shell regression pending |
| Starfield UI scaling/accessibility settings | Pending |
| Long/pseudo-localized labels and missing glyphs | Pending; pixel glyph set is temporary |

## Pending compatibility validation

| Scenario | Status |
|---|---|
| Vanilla UI isolated profile | Verified for current core path |
| Heavy 600+ mod/UI profile | Broad compatibility verified for invocation and basic use |
| Representative PauseMenu replacement/overhaul variants | Partial; exact matrix not recorded |
| Inventory/HUD/UI-overhaul mods | Broad profile observation only |
| Frame generation and overlay/capture tools | Pending formal matrix; Control Panel has no project renderer hook |

## Required invariants

- Vanilla menus remain usable after any host failure.
- The panel always has a reachable close path.
- PauseMenu is closed before the dedicated panel opens; the panel never covers PauseMenu.
- No input is emitted to gameplay while capture owns the session.
- Held inputs are reseeded before gameplay resumes.
- No disk access occurs from render/advance callbacks.
- No unbounded ActionScript payload, telemetry queue, or string crosses the bridge.
- Rendered sprites own pointer hit regions; native code does not maintain duplicate pixel geometry.
- A disabled or absent host changes no save data and requires no cleanup.
- A provider remains usable when the host is missing, incompatible, or rejected.
