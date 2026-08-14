# Design decisions

> **Status:** Current architectural record. Entries distinguish accepted product decisions from
> provisional implementation choices and historical names.

This file records why the project is shaped as it is. It prevents a future maintainer or builder
agent from rediscovering the same tradeoffs from conversation history.

## D-001 — Product name and AI-forward method

**Status:** Accepted.

The product is **Absolute Control Panel**. “SLOP” and “Starfield Local Options Panel” are historical
research names retained only where required to reproduce the first ABI experiment. AI-assisted
research and implementation are an explicit project method; observable behavior, reviewability,
privacy, and maintenance evidence determine acceptance.

## D-002 — One native shared configuration host

**Status:** Accepted.

The Absolute suite should use one native Starfield/Scaleform configuration menu rather than one
Dear ImGui overlay per module. A shared host gives consistent focus, input, layout, accessibility,
and compatibility behavior while subscriber DLLs remain independent.

## D-003 — Providers retain gameplay and configuration ownership

**Status:** Accepted.

The host owns menu registration, rendering, navigation, generic capture, and transaction
orchestration. A subscriber owns schema semantics, validation, draft state, live preview,
persistence, and gameplay hooks. The host never parses another module's INI. No STL, allocator,
exception, ImGui, CommonLibSF, or SFSE object crosses the DLL ABI.

## D-004 — Fail-optional runtime discovery

**Status:** Accepted.

Subscribers discover the already-loaded host and query a versioned C ABI dynamically. The Control
Panel is not a loader dependency. Missing, incompatible, or rejected hosts must leave gameplay,
INI editing, hotkeys, Workbench, and any standalone UI usable.

## D-005 — Persistent mod sidebar and horizontal module tabs

**Status:** Accepted.

Mods occupy a vertical sidebar. Pages belonging to the selected mod occupy a horizontal tab row
above the workspace. This borrows Skyrim MCM's proven information architecture while retaining the
mod list for faster switching and avoiding a second nested navigation screen.

## D-006 — Host-owned components and constrained definitions

**Status:** Accepted.

Subscribers request semantic controls; the host owns geometry, focus, pointer regions, styling,
and input behavior. The authoring language may describe types, ranges, labels, sections, and
bounded hints, but cannot inject arbitrary ActionScript, coordinates, CSS, textures, or native
drawing callbacks. This keeps different mods coherent and preserves safe evolution.

## D-007 — Existing ImGui panels are feature references

**Status:** Accepted.

The project should translate the useful structure and capability of existing Absolute ImGui
panels rather than discard their successful workflows. It does not copy renderer-specific code or
force a one-to-one window layout. Provider inventories and parity matrices preserve features while
the native component library determines presentation.

## D-008 — Additive PauseMenu entry plus F2 fallback

**Status:** Accepted for the experimental product path.

The product entry is appended to the populated PauseMenu model at runtime. Selecting it closes
PauseMenu and opens the dedicated panel. F2 remains a recovery/development path. Replacing or
covering PauseMenu was rejected because it increases conflict risk and breaks menu ownership.
MainMenu construction is not a current target.

## D-009 — No ESM/ESP for the current launch path

**Status:** Accepted.

The SFSE DLL can register the menu factory, load the SWF, and compose the PauseMenu entry without a
data plugin. An ESM/ESP becomes justified only if future work needs forms, quests, Papyrus, or a
data-defined entry; it must not be added merely to carry the SWF.

## D-010 — Runtime mappings are exact and independently verified

**Status:** Accepted; implementation remains runtime-specific.

CommonLibSF types are useful structural scaffolding but unresolved or stale menu IDs are not ABI
authority. The host currently uses a pinned CommonLibSF snapshot, Address Library, and an exact
1.16.244 overlay recovered from the executable. These dependencies are internal and must be
revalidated for every Starfield patch. Removing CommonLibSF remains possible future work, not a
completed decision.

## D-011 — Input originates in the native menu/game path

**Status:** Accepted.

Menu navigation and capture consume Starfield/SFSE input and schedule UI work on the appropriate
game/UI path. Startup automation may request bounded game-thread key pulses, but the product must
not continuously drive vJoy, Steam Input, or a virtual controller. Earlier controller automation
caused a frozen/phantom-axis test condition and is not part of the product.

## D-012 — Mechanical automation plus human UX judgment

**Status:** Accepted.

Scripts and structured evidence should prove registration, lifecycle, pointer phases, writes,
rollback, persistence, and absence of crashes. A human validates feel-dependent questions such as
slider response, visual hierarchy, readability, and navigation comfort. Vision models are most
valuable for diagnosing failed screenshots, not routine binary success.

## D-013 — Direct launcher shortcut is the baseline launch interface

**Status:** Accepted for local research.

The ignored local manifest supplies the exact SFSE launcher shortcut. Automation launches that
shortcut directly; it does not navigate MO2 or require general desktop/computer control. MO2 profile
preparation is a separate explicit step.

## D-014 — Source-built SWF and no redistributed Bethesda assets

**Status:** Accepted.

The interface is built from checked-in ActionScript using a pinned toolchain. A manually edited
opaque SWF cannot be the sole source of truth. Extracted vanilla SWFs may inform local research but
are ignored and never redistributed.

## D-015 — Privacy is a release and contribution gate

**Status:** Accepted.

Local paths, account names, mod-list locations, logs, screenshots, dumps, process IDs, device
identifiers, and credentials remain ignored. Documentation and PRs use placeholders and semantic
results. Every commit/PR scans the exact staged payload.

## D-016 — Legacy frontends remain during migration

**Status:** Accepted for the experimental period.

Head Tracking and AbsoluteZero retain Workbench, standalone ImGui, INI, and hotkey paths. Control
Panel integration is additive and reversible until the native host and SDK earn release status.

## D-017 — Advanced components use a bounded separate experiment

**Status:** Provisional.

Live graphs, range/detent meters, and power-pip grids require bounded snapshots, rate limits,
visibility suspension, stale-state handling, and compound operations. They currently live behind
a separate experimental API so ordinary ABI v1 cannot be destabilized. Promotion, redesign, or
removal remains open.

## D-018 — Dirty close currently cancels; release UX remains open

**Status:** Provisional.

The current session safely invokes provider Cancel when a dirty page closes. This prevents silent
persistence but discards the draft without a confirmation screen. The target design calls for an
input-complete Apply/Discard/Return modal before release.

## D-019 — Documentation follows executable evidence

**Status:** Accepted.

Every accepted capability updates `CURRENT-STATE.md`, the component catalog, and `TEST-MATRIX.md`
in the same change. Target contracts are labeled as targets, and historical experiments remain
available without masquerading as current instructions.

## Open decisions before SDK freeze

- Permanent DLL filename, public query name, namespace, and legacy-alias lifetime.
- Whether labeled choices, strings, sections, and presentation hints extend ABI v1 or require v2.
- Final dirty-close workflow and modal behavior.
- Numeric editor interaction and formatting model.
- Whether the experimental live/compound ABI is promoted, redesigned, or removed.
- Whether eliminating CommonLibSF materially improves maintenance after the native seam stabilizes.
