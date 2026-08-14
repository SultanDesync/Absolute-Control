# Current implementation state

> **Authority:** This is the primary statement of what exists today. Product contracts describe
> intended behavior; historical research documents describe how the project reached this point.
> When either conflicts with this file, this file governs until the conflict is resolved.

This snapshot describes host code checkpoint `1ce2f6f` and the 2026-08-13 Starfield 1.16.244
runtime evidence. The project is experimental and is not a supported Nexus or SDK release.

## Evidence vocabulary

- **Runtime verified** — observed in Starfield and backed by a recorded manual result or structured
  evidence.
- **Observed** — seen in Starfield and useful for direction, but lacking a complete artifact or
  scenario record.
- **Build verified** — compiles and passes automated tests, but lacks a complete current in-game
  result.
- **Implemented, validation pending** — code exists, but its relevant regression has not been
  completed or recorded.
- **Specified** — accepted target design only; not connected to the shipped movie/runtime.
- **Historical** — retained research provenance, not current operating guidance.

## Product and runtime boundary

| Capability | Status | Current behavior |
|---|---|---|
| Native custom menu registration | Runtime verified | Registers an independent `GameMenuBase` and loads a dedicated source-built SWF. |
| Additive PauseMenu entry | Runtime verified | Appends one Control Panel action after PauseMenu population without replacing `pausemenu.swf`. |
| F2 fallback | Runtime verified | Opens the panel from gameplay when neither MainMenu nor PauseMenu is active. |
| Main-menu entry | Not implemented | The project deliberately pursued PauseMenu composition first. |
| ESM/ESP dependency | Not required | The DLL factory and SWF path need no data plugin. |
| Runtime support | Exact-gated | Native mappings are verified only for Starfield 1.16.244. |
| CommonLibSF | Internal dependency | The host still builds against a pinned CommonLibSF snapshot and Address Library. Missing menu IDs are supplied by an exact source overlay. No CommonLibSF type crosses the provider ABI. |
| PauseMenu replacement | Rejected design | The panel closes PauseMenu and opens as its own menu; it never covers or replaces PauseMenu. |

## Shell and navigation

| Capability | Status | Current behavior |
|---|---|---|
| Multiple subscriber modules | Observed | Independently compiled Head Tracking and AbsoluteZero pages populated one menu; the exact paired artifact transcript was not retained. |
| Persistent mod sidebar | Runtime verified | One vertical row per module, with 13 visible rows. |
| Horizontal page tabs | Runtime verified | Pages for the selected module appear across the workspace, with six visible tabs. |
| Dense control workspace | Runtime verified | Ten compact rows, selected-setting help, and a scrollbar at 1080p. |
| Clickable footer | Observed plus automated coverage | Apply, Cancel, and Close are compact pointer targets. |
| Mouse clicks | Observed plus automated coverage | Rendered ActionScript sprites own pointer hit-testing and emit semantic commands. |
| Slider click and drag | Runtime verified | Click-to-position and continuous bidirectional drag survive redraw after each draft write. |
| Mouse wheel | Implemented, validation pending | Native direction-specific wheel events route to the hovered sidebar, tabs, or workspace. A clean final two-direction regression is not yet recorded. |
| Keyboard navigation | Observed plus automated coverage | Individual module, page, control, adjustment, action, Apply, Cancel, and Close routes work; a formal keyboard-only end-to-end regression remains pending. |
| Controller navigation | Implemented, validation pending | Router support exists, but a clean controller-only regression is outstanding after the earlier test-input/ghost-axis incident. |
| Typography | Temporary | The source-built SWF uses an embedded pixel glyph set; alignment, glyph coverage, and localization remain unfinished. |

## Standard controls and transactions

| Capability | Status | Current behavior |
|---|---|---|
| Boolean toggle | Runtime verified | Reads and writes provider-owned Boolean drafts. |
| Integer/float slider | Runtime verified | Step adjustment, click-to-position, and held drag are available. |
| Action | Runtime verified | Invokes a provider callback; Head Tracking recenter is the first subscriber example. |
| Keyboard binding capture | Runtime verified | Records a single key plus Ctrl/Alt/Shift modifiers, supports cancel and clear, and writes a normalized string to the provider draft. |
| Mouse/controller binding capture | Not implemented | Flags are reserved, but only keyboard chords are complete. |
| Choice | Incomplete | The host model and movie can step an integer value, but ABI v1 has no provider-supplied choice labels or real dropdown. |
| Bounded text input | Not implemented | Reserved for a future ABI/schema revision. |
| Numeric text editor | Not implemented | Sliders display values but do not provide direct typing. |
| Apply | Observed plus automated coverage | Calls the active provider page's save/persistence callback. |
| Cancel | Observed plus automated coverage | Calls provider rollback and clears host dirty state. |
| Dirty close | Provisional implementation | Close automatically invokes provider Cancel. A confirmation modal is target design, not current behavior. |
| Cross-page dirty navigation | Build verified | Navigation is rejected until the dirty page is applied or cancelled. |

## Subscribers

| Subscriber | Checkpoint | Evidence | Current integration |
|---|---|---|---|
| Absolute Head Tracking | `674491e` on `agent/absolute-control-panel-integration` | Runtime verified | Three pages: General, one 12-control scrollable Axes page, and Bindings. Control Panel takes precedence; Workbench and standalone ImGui remain fallbacks. |
| AbsoluteZero Ship Control | `1a4cefc` on `agent/absolute-control-panel-integration` | Build verified adapter; paired runtime observed | One Mouse Alignment page. It currently consumes the preserved legacy `SLOP_QueryApi` ABI alias. Workbench, INI, and hotkeys remain supported. |
| Absolute Power | None | Not started | Intended first advanced/compound-component subscriber after ordinary components stabilize. |
| AbsoluteHOTAS | None | Not started | Its large ImGui surface, device capture, graphs, and detents remain parity targets. |

## Advanced components

The experimental live/compound API has headless tests for range bands/markers, telemetry rings,
segmented allocation grids, stale data, visibility, and bounded operations. It is **not connected
to the current MenuSession/Scaleform renderer** and is not part of the provider ABI release
candidate. Dynamic HOTAS graphs, power pips, and the diagnostic side-scrolling spaceship are not
implemented in game.

## Harness state

| Tooling | Status | Notes |
|---|---|---|
| Native build and four host tests | Current | `xmake` and `xmake test`. |
| Source-built SWF | Current | Pinned Flex/PlayerGlobal toolchain and checked build metadata. |
| SDK generator/tests/catalog | Current | Deterministic code generation, six Python tests, compile fixture, and catalog validation. |
| Direct shortcut launch | Current | The ignored manifest's `shortcut` is authoritative; launch it directly without navigating MO2 or using desktop control. |
| Runtime command mailbox | Current | Bounded commands with accepted/completed evidence. |
| Retained PauseMenu cycle | Current | Avoids replaying title/save-load for lifecycle investigation. |
| Profile preparation | Available | Verifies or deliberately enables named requirements in one explicit MO2 profile. |
| `run-probe.ps1` full success gate | Historical/inconsistent | It still requires the removed magenta sentinel and follows the research-era startup sequence. It must not be cited as a current passing end-to-end test. |
| Packaging compatibility script | Needs normalization | It still targets the research-named DLL artifact rather than the product-named build. |

See [the harness status](RESEARCH-HARNESS.md) for supported workflows and limitations.

## Runtime evidence and known risk

- The additive PauseMenu path populated in an isolated baseline and a heavily modified 600+ mod
  profile without replacing a vanilla SWF.
- The lifecycle-owned implementation completed 25 consecutive isolated PauseMenu opens, including
  Control Panel show/hide followed by five additional cycles, with no new crash or timeout.
- Two infrequent PauseMenu/close crashes were observed in earlier builds before the lifecycle
  revision. They were not reproduced in the 25-cycle result, but crash monitoring remains open.
- Slider dragging was manually judged to work well; structured evidence showed down, repeated
  writes, direction changes, and release.

Exact artifacts and pending scenarios are recorded in [the test matrix](TEST-MATRIX.md).

## Explicit release gaps

The project is not release-ready until at least the following are resolved:

- permanent ABI/package naming and removal or migration of research aliases;
- choice labels/dropdowns, text/numeric editing, section metadata, and dirty-close UX;
- controller-only navigation and mouse/controller binding capture;
- localization, font/glyph coverage, UI scale, resolution, and ultrawide validation;
- current end-to-end deploy/launch/test automation without the removed sentinel;
- a repeatable Starfield patch/update result using the update runbook;
- failure injection for missing/corrupt SWF, provider failures, and incompatible versions; and
- release packaging, licensing/notices, versioning, and public SDK examples.

The release checklist is maintained in [SDK status](SDK-STATUS.md).
