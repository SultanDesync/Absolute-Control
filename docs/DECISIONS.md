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

## D-008 — Additive PauseMenu entry plus opt-in recovery hotkey

**Status:** Accepted for the experimental product path.

The product entry is appended to the populated PauseMenu model at runtime. Selecting it opens the
dedicated panel above the still-resident PauseMenu. PauseMenu remains the native gameplay-pause
owner; the panel's higher render/input priority covers it and prevents input from reaching it.
Starfield's audio listener recognizes a hard-coded menu-name set, so the custom panel also acquires
and releases the same ref-counted native audio mode (`2`) used by PauseMenu. Back hides only the
panel and reveals that same PauseMenu instance. If another plugin removes the underlay, the close
path queues a recovery Show instead. Replacing `pausemenu.swf` remains rejected. The standalone
hotkey is unbound by default because global function-key capture can collide with other mods and its
direct close path has produced incorrect ultrawide viewport restoration. A user can explicitly set
`OpenHotkey` to a Win32 virtual-key code (for example `0x71` for F2) as a recovery path when the
PauseMenu entry is inaccessible. MainMenu construction is not a current target.

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

## D-018 — Guarded dirty navigation and close

**Status:** Accepted target; modal implementation pending.

One menu session permits one dirty provider page. Leaving it for another page/module prompts
Apply & Continue, Discard & Continue, or Stay. User-requested Close prompts Apply & Close, Discard
& Close, or Stay. External destruction cannot prompt and performs one provider Cancel while its
transaction lease is held. The subscriber retains all draft and persistence ownership; the host
stores only dirty identity, route intent, and transaction orchestration. See
[scalability, transactions, and teardown](SCALABILITY-TRANSACTIONS-AND-TEARDOWN.md).

## D-019 — Documentation follows executable evidence

**Status:** Accepted.

Every accepted capability updates `CURRENT-STATE.md`, the component catalog, and `TEST-MATRIX.md`
in the same change. Target contracts are labeled as targets, and historical experiments remain
available without masquerading as current instructions.

## D-020 — Canonical product and quarantined ResearchDev artifacts

**Status:** Accepted.

`AbsoluteControlPanel` is the default canonical target and only release-role/packageable artifact.
`AbsoluteControlPanelResearchDev` is opt-in and non-packageable. It composes the product host with
mailbox/SendInput automation and DirectInput experiments. It registers no research-only menu
module; both artifacts use the release-safe Absolute Control host-management module.
Release/research sources are explicit, manifests are role-bound and hash-bound,
and no deploy/package tool may infer an artifact by scanning a build directory.

## D-021 — Product ABI authority and bounded legacy adapter

**Status:** Accepted for the ABI-v1 development period.

`AbsoluteControlPanelAPI.h` is the only authority for ABI values, records, and callbacks.
`SlopAPI.h` aliases those definitions and preserves the original export/table prefix for already
built experimental subscribers; it is not a parallel contract and does not expose product-only
table suffix fields. New subscribers use product discovery. Adapter removal requires an announced
migration checkpoint.

## D-022 — Readiness, callback leases, and retryable unregister

**Status:** Accepted.

Discovery is available during initialization, but registration and refresh return `NotReady` until
runtime validation, hook installation, and factory retention have succeeded. Terminal runtime
rejection returns `Rejected`. Provider calls acquire nonblocking in-flight leases and run outside
host locks. A successful draft pins its provider until Apply, Cancel, or session destruction;
unregister returns retryable `Rejected` while a call or transaction is active rather than waiting
on the UI thread or invalidating code.

## D-023 — Generational snapshots, refresh wakeups, and coalescing

**Status:** Accepted for the internal bridge.

Every published model receives a per-session generation and current ActionScript commands echo it.
Stale gestures are rejected before provider mutation. Provider `requestRefresh` advances a
separate revision that the open bridge consumes on the UI thread; multiple refreshes may coalesce
into one replacement snapshot. Slider drag keeps the latest sampled value and emits at most one
provider write per movie frame. The generation-free ten-argument bridge remains compatibility-only.

## D-024 — Explicit module ownership and process-lived services

**Status:** Accepted.

Native bootstrap, compatibility, API/session, input, menu integration, Scaleform, diagnostics, and
research sources have explicit ownership and one-way dependencies. ActionScript separates its
document-class surface from bridge, selection, pointer, widgets, shell, layout/theme, and slider
coordination. Complete source-tree provenance is required for the SWF. Polling and evidence workers
are cooperatively stoppable, but their plugin owners are intentionally process-lived because SFSE
does not support plugin unload and loader-lock destruction must not join threads. Evidence file I/O
is bounded and asynchronous from UI/render producers.

## D-025 — One product-version authority

**Status:** Accepted.

The `product_version` value in `xmake.lua` is the sole product-version authority. Xmake defines
`ACP_PRODUCT_VERSION` from it for both public and legacy API tables; artifact-manifest generation
reads the same value. Fixture, catalog, schema, and tool versions remain independently versioned.

## D-026 — Frame-boundary model publication

**Status:** Accepted and implemented for the current bridge.

Commands and provider callbacks may synchronously mutate session/provider state, but replacement
Scaleform models are coalesced and applied only from the next movie-frame acknowledgement. The
initial no-input snapshot is the sole synchronous publication. A successful Close enters a
terminal state and queues Hide without rebuilding the display tree. This prevents display-object
destruction while pointer/keyboard dispatch still owns an event target.

## D-027 — Lazy module directory before hundreds-scale capacity

**Status:** Core representation implemented and build-verified; runtime budgets pending.

The internal bridge carries a compact module directory, page metadata for the active module, and
values/controls only for the active page. The 512-module × 3-page × 16-control fixture proves 16
reads and 16 serialized controls rather than full-graph traversal. Registry limits are explicit;
the 512-summary serialization and visible redraw path still require in-game frame/memory budgets.
Provider headless ownership and the public C ABI remain independent of this view model.

## D-028 — Terminal, fail-safe menu teardown

**Status:** Accepted; core bridge/session behavior implemented, runtime fault injection pending.

Accepted Close makes the bridge terminal: late commands cannot mutate providers, pending movie
publications are dropped, capture ownership is cleared, and Hide is queued. External destruction
also clears capture and relies on the session's exactly-once dirty rollback under its transaction
lease. Dynamic plugin DLL unload remains unsupported and is separate from safe menu teardown and
retryable subscriber unregistration.

## D-029 — Linearized registry and session-scoped lifecycle

**Status:** Accepted and build-verified; engine fault injection pending.

Registry mutations and terminal readiness transitions recheck lifecycle under the registry lock;
catalog graph and revision are captured at one linearized point. Provider callbacks run outside
host locks under leases, and unregister is nonblocking/retryable while callbacks or a transaction
are active. Refresh wakes are page-scoped except for directory mutations. Menu session state and
Scaleform calls remain deliberately UI-thread-confined. Back-stack origin belongs to the displayed
menu instance, and Hide/destruction share one idempotent capture/transaction teardown.

## Open decisions before SDK freeze

- ABI-v1 freeze and the exact removal window for the legacy query/table prefix.
- Whether labeled choices, strings, sections, and presentation hints extend ABI v1 or require v2.
- Numeric editor interaction and formatting model.
- Whether the experimental live/compound ABI is promoted, redesigned, or removed.
- Whether eliminating CommonLibSF materially improves maintenance after the native seam stabilizes.
