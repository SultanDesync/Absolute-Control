# Absolute Control — UX and Visual Design Brief

> Handoff brief for an external design agent. Read this document before proposing layouts,
> interaction changes, visual assets, or implementation work. The first deliverable is a design
> specification, not a replacement UI implementation.

## Product in one paragraph

**Absolute Control** is a native, shared Starfield configuration workbench for independently
installed mods in the Absolute family. It is launched from Starfield's Pause Menu and rendered as
its own Scaleform movie. Subscriber mods register their own pages and controls through a versioned
API; the host supplies consistent navigation, transactions, input handling, rendering, and
accessibility. The immediate design target is a polished shell that can support hundreds of
subscribers over time and can present Absolute Power's unusually dense Presets, Automation, and
Diagnostics workbenches without turning into a generic desktop settings application.

The production-facing name is **Absolute Control**. Repository, DLL, menu, API, and other technical
identifiers may still say `AbsoluteControlPanel`; do not rename those as part of design work.

## Assignment

Develop a coherent UX direction and implementable style guide for Absolute Control. Preserve the
proven information architecture while improving hierarchy, density, discoverability, focus,
readability, feedback, and visual fit with Starfield.

The design should feel like it belongs beside Starfield's Pause Menu without pretending to be a
vanilla screen. Favor a source-built, restrained, technical visual language: confident geometry,
clear type hierarchy, economical color, and unambiguous states. Do not copy Bethesda, SkyUI, or
other mods' proprietary art or private implementation.

The proposal must cover both:

1. the reusable shared shell and component system; and
2. the first demanding production subscriber, Absolute Power.

Do not redesign backend behavior, move provider responsibilities into the host, or assume that a
web UI framework is available.

## Canvas decision

Design for a logical **1920×1080 (16:9) stage** and use nearly all of it. The movie already owns and
paints this complete stage. The current prototype unnecessarily confines primary content to a
1580×920 panel at `(170, 80)`, so reclaiming this space does not require a new rendering model.

Use these rules:

- Background and noninteractive framing may reach all four edges.
- Keep important text, focus outlines, and controls inside an initial **48–72 px edge-safe gutter**.
  Propose an exact safe-area token and justify it through 720p legibility and television/overscan
  considerations.
- Treat 1920×1080 as the coordinate system, not the user's physical resolution. Starfield scales the
  movie for 720p, 1080p, 1440p, and 4K.
- On a 21:9 or wider display, preserve a centered, undistorted 16:9 composition. Do not stretch
  controls or typography to fill the physical ultrawide display.
- An adaptive ultrawide canvas is a separate engineering feature. It would require revised viewport,
  pointer-coordinate, layout, and teardown validation and is outside this design pass. An optional
  concept may show ambient background treatment outside the 16:9 safe composition, but the primary
  proposal must not depend on it.
- Closing Absolute Control returns to the Pause Menu so native menu teardown restores the game
  viewport. The visual design must not replace or obscure that lifecycle behavior.

In short: **yes, maximize the complete 16:9 menu area; no, do not equate that with stretching across
21:9.**

## Proven information architecture

Preserve this top-level structure:

- A persistent vertical **module sidebar** lists installed subscriber mods. Provider pages never
  appear in this list.
- A horizontal **page tab row** lists the selected module's pages.
- The central **workspace** contains the active page's controls.
- A nearby **selected-control help region** supplies the longer explanation for the focused control.
- A compact **command bar** contains contextual Apply, Cancel, Back, and control-specific prompts.
- Modal workflows temporarily own focus and clearly present their allowed actions.

The prototype currently exposes 13 modules, six tabs, and about ten simple setting rows before
scrolling. The release design may alter those counts based on measured geometry, but it must retain
the following behavior:

- Hundreds of modules remain viable through a bounded, virtualized/visible sidebar rather than by
  constructing every page and control.
- All installed module summaries may be known, but only the selected module's pages and active
  page's controls are materialized.
- Pointer hit regions and drawn geometry come from the same layout values.
- Mouse wheel input scrolls the region under the pointer. Over the workspace it moves the viewport
  or selection; it must never silently alter the setting under the pointer.
- Avoid deep navigation stacks. Keep the selected module visible while working within its pages.
- Labels and values should usually share one aligned row. Long prose belongs in contextual help,
  not permanently expanded cards.
- Apply/Cancel belong in a compact command or dirty-state bar, not in a permanent right-hand action
  column.

## Ownership boundary — hard restriction

The shell owns presentation mechanics:

- menu launch, close, and return to Pause Menu;
- module/page/control navigation, focus, scrolling, modals, and controller prompts;
- rendering immutable, generation-numbered snapshots;
- translating user gestures into typed commands; and
- showing provider-reported state, readiness, validation, and telemetry truthfully.

Each subscriber/provider owns domain behavior:

- configuration schema and defaults;
- validation and sanitization of drafts;
- device discovery and binding-capture interpretation;
- live preview and telemetry;
- atomic persistence, runtime reload, and readback; and
- all gameplay hooks.

The design must not require Absolute Control to parse a subscriber's INI, infer gameplay policy, or
become necessary for the subscriber's core function. A subscriber with an established profile—or a
user comfortable editing its INI or using its legacy overlay—must remain viable without the shared
UI host.

## Interaction and input requirements

Every primary operation must work with keyboard, mouse, and controller. Hover is supplementary and
must never be the only state cue.

- `Tab` and controller `B` navigate back according to the active context.
- Back from the root menu closes Absolute Control and returns to the Pause Menu.
- Focus must be obvious at a glance and distinguishable from hover, selection, dirty value, and live
  value.
- The command bar is input-aware. Apply, Cancel, and Back show the current device's short prompt or
  glyph, and the complete prompt is clickable.
- Back remains reachable. Apply and Cancel clearly communicate clean/disabled states.
- Do not reserve permanent screen space for generic `W/S` or `A/D` tutorials. Show contextual prompts
  only when a slider, dropdown, binding field, text field, or modal makes them useful.
- Dense visualizations must use compact focus stops. The Absolute Power 6×32 allocation grid must not
  become 192 separate controller/keyboard focus targets.
- No meaning may depend on color alone. Combine color with shape, symbols, labels, texture, or state
  copy.
- Long workspaces must show a persistent proportional scrollbar and above/below hidden counts; wheel
  input must never be the only indication that more controls exist.
- Action rows require a labeled `RUN` or action-specific button. An unlabeled chevron or clickable
  setting name is not a sufficient execution affordance.

Include designs for these interaction states: idle, hover, focused, selected, disabled, unavailable,
dirty, validation warning, validation error, capture/listening, saving/applying, success/readback,
stale telemetry, and terminal provider failure.

## Transactions and close behavior

Edits create a provider-owned draft. Applying validates and persists the draft; cancelling restores
the last accepted provider snapshot. Switching modules, switching pages when appropriate, and
closing the menu must not lose edits silently.

Design the accepted target workflow even where the implementation is still being completed:

- A route or close attempt with pending changes opens an **Apply / Discard / Stay** decision.
- The dialog names the affected module/page and makes the consequence of each action explicit.
- Save/apply failure leaves the user in context, preserves the draft when safe, and shows the
  provider's actionable error.
- Teardown cancels capture, stops input interception, releases snapshots, and returns through the
  native menu stack. Do not propose a visual shortcut that bypasses clean close.

## Component vocabulary

Provide specifications for at least:

- module row, page tab, section heading, divider, help block, and status strip;
- toggle, slider with aligned numeric value, labeled choice/dropdown, text field, numeric field,
  binding-capture field, compact button, destructive button, and content-sized action group;
- scrollbar/position indicator, focus ring, tooltip or contextual help treatment;
- inline validation, global/provider failure, stale-data state, dirty indicator, and progress/apply
  state;
- confirmation modal, Apply/Discard/Stay modal, binding capture modal, and unavailable-feature
  treatment; and
- input-aware command bar for keyboard/mouse and controller.

Components must use measured content and explicit minimum/maximum dimensions. A universal fixed-height,
full-width card is not an acceptable release primitive.

Choice labels and the bounded virtualized dropdown are now runtime capabilities. Richer section
metadata is still not exposed by the current schema; mark proposals that depend on it as an
engineering dependency rather than representing it as already implemented.

For selected-record workbenches, one populated dropdown is authoritative. Its options carry the
compact metadata needed to distinguish records, and a transient selection change does not dirty the
page. Do not duplicate it with Previous/Next actions or another selected-record summary. Rename and
lifecycle controls explicitly target that selection.

## Absolute Power screens

### Shared module header/status

Across all Absolute Power pages, provide a compact summary of:

- backend readiness;
- current and settling preset;
- reactor total, allocated, and available power;
- frontend/binding availability; and
- snapshot age or ship/context availability.

Resolve status with this priority: terminal failure, warning/action required, dirty draft,
contextual unavailability, then ready. Do not show several competing badges or rely on color alone.

### Presets

The Presets page must accommodate:

- one populated source/startup-aware preset selector;
- a bounded rename field and startup toggle;
- compact inline create/duplicate and delete-or-hide/revert action pairs;
- keyboard and optional Input Bus shortcut binding/capture state;
- explicit per-system power-allocation tie-break Choices; and
- a six-system visualization sized to each provider-declared maximum (12 pips for Power).

The six systems need clear installed/not-installed, live current/maximum, draft preview, and clipped
states. Keep the Green-first/Yellow-after-Green/Red-last, live-outline, preview-tick, and hollow-capacity
legend on the visualization itself. Remove the old G/Y/R request-count column and exact tier-slider
duplicates. Each colored pip carries a 1/2/3 glyph; each row provides direct pip cycling, quick-step
buttons, and an explicitly associated tie-break Choice reachable from pointer, keyboard, and
controller focus. Favor a responsive grouping that makes comparison easy without creating a
pip-per-tab-stop trap or requiring every datum to live in a large card. Use the normal host
Apply/Cancel footer. Manual activation is an explicit product decision still to be settled, not a
hidden selector side effect.

### Automation / Cheats

For the early simultaneous release this route is **Automation / Cheats (Coming Soon)**. Render only
provider-published unavailability/status and an immediate Disable All safety action. The detailed
rule-builder requirements below are deferred research until Absolute Power settles a simpler
On-Demand Power contract and any Auto Combat Mode.

This page must contain a persistent, candid **CHANGES GAME BALANCE** warning. Its tone should be
neutral and factual, not celebratory or moralizing.

Provide layouts for:

- a global automation gate, distinct from each rule's enabled state;
- disabled-by-default behavior;
- one populated source/provenance/enabled-aware rule selector plus lifecycle state;
- name, trigger, source, target, target pips/max, threshold/hysteresis, hold time, priority, and a
  plain-language rule summary; and
- a prominent but safe **Disable All Now** action.

Live event sources and active-rule execution telemetry are not implemented yet. The current product
must say **NOT READY** or otherwise show genuine unavailability. Do not fabricate live activity,
enabled execution, or success telemetry in mockups.

### Diagnostics

Organize diagnostic data into these groups:

- Compatibility
- Executor
- Live Ship
- Configuration
- Activation
- Frontends

Distinguish normal contextual absence—such as no ship context—from an actionable fault. Include a
compact paths/support summary suitable for bug reports without dumping a developer console into the
main experience.

## Branding and visual direction

The visible product title is **Absolute Control**. Explore a restrained relationship with
Starfield's Pause Menu typography and spacing so the entry feels intentionally integrated. Do not
assume that Bethesda's font files or proprietary assets can be redistributed.

Provide two title/accent recommendations:

1. a quiet production treatment that sits naturally beside the other Pause Menu entries; and
2. an early-release treatment that is easier to locate, using a restrained accent color, rule,
   marker, or build tag rather than an unrelated novelty typeface.

The current PauseMenu product label is **MOD OPTIONS**. It should inherit the neighboring native
row's typography, scale, selection behavior, and white text. A dark desaturated purple background
such as `#4A365D` is the preferred discoverability experiment, but it must be applied as a row accent
rather than a custom font or purple text, and it is not accepted until normal, hover, selected,
disabled, controller-focus, and high-brightness contrast states are verified in game.

The current prototype palette is dark blue-black with cyan text/accent, gold warning/dirty accents,
soft red errors, and green/yellow/red allocation tiers. It is provisional, not a mandate. A revised
palette must include token names, values, intended semantic use, contrast rationale, and
color-blind-safe companion cues.

Avoid:

- generic sci-fi neon overload, excessive glow, or transparent glass everywhere;
- a desktop/web settings aesthetic transplanted unchanged into the game;
- decorative animation that delays input, masks provider latency, or reduces text clarity;
- tiny all-caps text as the only way to achieve density; and
- visual states that cannot be recreated deterministically in ActionScript 3.

## Technical implementation constraints

The eventual UI is source-built **ActionScript 3 / Scaleform**, compiled into a SWF. The stage is
1920×1080 at 30 FPS. The renderer rebuilds its display tree from immutable snapshots; it is not
HTML/CSS and has no DOM, React, canvas shader stack, or guaranteed modern font engine.

Design within these realities:

- Prefer vector geometry, solid/controlled fills, lines, simple masks, and modest alpha effects.
- Avoid GPU-heavy blur, complex filters, shaders, video backgrounds, or effects whose appearance
  depends on browser compositing.
- Layout, drawing, and hit-testing need shared numeric tokens. Supply exact coordinates, spacing,
  size, state, and z-order guidance—not only a mood board.
- The current prototype uses a temporary pixel glyph renderer. Typography, localization glyph
  coverage, accessibility scaling, and font licensing remain open work. Specify a hierarchy and
  fallback strategy; do not bundle or require an unlicensed font.
- Text may expand under localization. Identify truncation, wrapping, minimum-width, and overflow
  behavior.
- Keep visible work bounded. Do not require rendering every module, page, row, or telemetry event at
  once.
- The design must remain legible when the 1920×1080 stage is scaled down to 1280×720 and remain
  appropriately crisp at 4K.
- All source changes must remain reproducible. Do not edit a binary SWF as the design source.

## Required design deliverables

Produce a self-contained design package containing:

1. A concise design rationale and hierarchy principles.
2. A 1920×1080 shell wireframe with exact safe area, sidebar, tabs, workspace, help, status, and
   command-bar geometry.
3. High-fidelity treatments for Absolute Power Presets, Automation / Cheats, and Diagnostics.
4. Modal/state studies for Apply/Discard/Stay, binding capture, validation failure, provider
   unavailable, stale telemetry, and not-ready functionality.
5. Keyboard/mouse and controller focus/navigation diagrams, including the 6×32 allocation grid.
6. A style-token table covering color, typography, spacing, borders, focus, opacity, animation
   timing, and icon/symbol use.
7. Component specifications with all relevant states and minimum hit-target sizes.
8. A comparison of the quiet production title treatment and the discoverable early-release
   treatment, with one recommendation.
9. Notes identifying which ideas are implementable using the existing schema, which require a host
   UI change, and which require a provider/API extension.
10. A short visual QA checklist for 720p, 1080p, 1440p, 4K, 16:9, 21:9 letterboxed composition,
    keyboard/mouse, and controller.

Use labeled diagrams or 1920×1080 mockups rather than prose alone. If producing code or design files,
keep the written token/component specification alongside them so the result can be reimplemented in
the project's AS3 source rather than depending on a proprietary export pipeline.

## Acceptance criteria

A successful direction:

- uses nearly the full 16:9 stage without crowding the edge-safe area;
- makes module, page, active control, live value, draft value, and available actions obvious;
- supports dense Absolute Power workflows without losing console readability;
- remains truthful about unavailable and not-yet-implemented runtime features;
- provides equivalent keyboard, mouse, and controller paths;
- makes warnings and tier meanings intelligible without color;
- preserves subscriber independence and the host/provider ownership boundary;
- can scale to hundreds of modules through bounded visible content;
- fits Starfield's menu context while giving Absolute Control a recognizable identity; and
- can be implemented deterministically in ActionScript 3 without proprietary source assets.

## Repository references

Use these documents to resolve details if the repository is available:

- `docs/NATIVE-MENU-CONTRACT.md` — authoritative UI ownership and interaction contract.
- `docs/SCALABILITY-TRANSACTIONS-AND-TEARDOWN.md` — scale, dirty-state, close, and teardown policy.
- `docs/CURRENT-STATE.md` — implemented versus pending capability truth.
- `docs/ARCHITECTURE.md` — process, rendering, snapshot, and coordinate boundaries.
- `docs/IMGUI-PARITY-MATRIX.md` — legacy workbench feature inventory.
- `../../Absolute Workbench/docs/AP-MENU-INTEGRATION-PLAN.md` — Absolute Power page and provider plan.
- `interface/src/acp/ui/PanelLayout.as` — current prototype geometry.
- `interface/src/acp/ui/PanelTheme.as` — current provisional visual tokens.
- `interface/src/acp/ui/MenuShellRenderer.as` — current source-built renderer.

When a proposal conflicts with `CURRENT-STATE.md` or the native menu contract, call out the conflict
instead of quietly designing around it.
