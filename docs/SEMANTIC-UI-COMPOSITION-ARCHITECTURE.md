# Absolute Control semantic UI composition architecture

> **Status:** Platform design authority for the post-scaffold subscriber phase.
> C0's headless foundation, C1's status/card/row/condition/anchor slice, and the
> C2 Flight Axes/live-association implementation are present. Later record,
> context, workflow, progress, and direct manipulation vocabulary remains
> unadvertised. C2 runtime visual/input acceptance is still pending.
>
> **Acceptance driver:** AbsoluteHOTAS Flight Axes, followed by Ship Buttons,
> Throttle, Aiming, Profiles, Macros, and Devices.

Absolute Control is not a form generator for INI fields. It is a modular UI host
for providers that own configuration, runtime behavior, live state, and workflows.
The host must be able to compose powerful task-oriented interfaces without giving
providers renderer access or forcing every module into a flat list of controls.

AbsoluteHOTAS is the first broad acceptance subscriber. Its Dear ImGui workbench
contains axis cards, graph-linked tuning, binding capture, nested macro editing,
profile context, device tables, and calibration sessions. These patterns are useful
across the Absolute suite and must become reusable Control capabilities rather than
HOTAS-specific Scaleform code.

## Architectural principles

1. **Providers own meaning; Control owns presentation.** Providers publish typed
   values, validation, relationships, workflows, and prepared live state. They do
   not publish coordinates, colors, fonts, ActionScript, or draw commands.
2. **Composition is separate from data.** Existing controls remain the transaction
   and persistence surface. A semantic composition layer arranges and relates those
   controls without duplicating their values.
3. **Every enhanced layout has a truthful fallback.** Older hosts retain the flat
   control list, bounded selectors, provider capture, and Apply/Cancel where those
   capabilities exist.
4. **The ABI stays bounded and copied.** No arbitrary JSON, provider pointers,
   unbounded trees, per-frame allocation, or synchronous gameplay traversal crosses
   the module boundary.
5. **Interaction parity is part of the contract.** Pointer, keyboard, controller,
   focus restoration, cancellation, held-input suppression, and accessibility are
   designed with the primitive, not bolted on per subscriber.
6. **A workflow is not a collection of unrelated actions.** Multi-step operations
   have explicit state, progress, allowed transitions, Commit/Cancel, and failure
   semantics.
7. **Visual relationships are declared.** A graph marker, binding, slider, status,
   or record view is associated through validated IDs. Naming conventions never
   imply layout.

## Layered model

```text
Provider-owned domain
  configuration | draft | capture | repository | workflow | telemetry
         |
Stable Control data API
  typed controls | values | actions | Apply/Cancel | records | bindings
         |
Semantic composition API
  groups | cards | rows | anchors | context | conditions | associations
         |
Host session model
  immutable page snapshot | focus | modal stack | input mode | accessibility
         |
Native/Scaleform bridge
         |
Host-owned semantic renderers
  shell | cards | tables | workflows | live visuals | fallback list
```

The data API remains usable without the composition API. Composition references
already registered module, page, control, record, and live-channel IDs; it does not
become a second configuration API.

## API boundary

The first implementation should use a separate experimental composition API rather
than extending `PageDescriptorV1` with another large tail. This provides an ABI
incubator while the stable data API continues serving existing subscribers.

Proposed header and query:

```text
include/AbsoluteControlCompositionExperimentalAPI.h
AbsoluteControlPanel_QueryCompositionApi(version)
```

The host API registers a bounded composition descriptor for an already registered
page. Registration fails if any referenced page/control/live ID does not exist or
if the fallback would be inoperable.

### Proposed fixed limits

Initial limits are policy inputs to benchmark, not an invitation to fill every page:

| Resource | Initial bound |
|---|---:|
| Composition nodes per page | 128 |
| Child/association records per page | 192 |
| Anchors per page | 16 |
| Columns in one semantic grid/table | 8 |
| Visible record rows in one repeater window | 32 |
| Workflow steps | 16 |
| Workflow progress rows | 16 |
| Module-level pinned context controls | 12 |

All structures carry `structSize`; appended tails and capabilities are size-gated.
The host copies descriptors and retains callback leases using the same unregister
and pinned-transaction rules as the data API.

## Primitive families

### 1. Semantic status

A dedicated read-only status control replaces the current read-only binding
workaround.

Status state contains:

- short value text;
- optional bounded detail;
- severity: normal, informational, waiting, warning, error, unavailable;
- optional source/owner label;
- stale flag and monotonic update sequence; and
- accessible text that never relies on color alone.

Status can be rendered as a row, badge, card summary, or workflow message without
changing provider semantics.

### 2. Composition nodes

Composition nodes form a bounded parent-index tree. Nodes reference existing
controls/components; they never own values.

Proposed semantic node kinds:

| Node | Purpose | Flat fallback |
|---|---|---|
| `Section` | Task-level heading and help | GroupHeader followed by children |
| `Card` | Keep one concept's binding, live state, tuning, and status together | Children in source order |
| `Row` | Associate mixed controls/actions/status in one semantic row | One child per ordinary row |
| `Columns` | Small comparison or paired controls | Column-major children flattened in reading order |
| `AnchorSet` / `Anchor` | Navigate a long page by semantic destination | Omitted; ordinary scrolling remains |
| `PinnedContext` | Module-wide edit target/source/dirty state | Repeated page controls or dedicated context page |
| `RecordView` | Choose popup, master/detail, table, or repeater presentation for a RecordCollection | Existing bounded record selector |
| `WorkflowView` | Present a registered workflow session | Safe actions omitted; provider retains non-menu route |
| `LiveSlot` | Place and associate a registered live component | Component remains in ordinary live area |

Nodes expose semantic density and emphasis roles, not pixels. The host chooses
responsive geometry for supported display shapes and input modes.

### 3. Explicit associations

Association records express relationships that the renderer and input router must
understand:

- control belongs to card/row;
- slider edits a live marker or band edge;
- action captures a live marker's current value;
- binding has an adjacent route/status control;
- status explains a disabled control/group;
- one shared field is summarized on another page;
- record collection drives a visible detail group;
- table column maps to a record field/action; and
- anchor targets a composition node.

Associations are validated for type compatibility. An ID match alone never creates
a visual or transactional relationship.

### 4. Conditional presentation state

Providers may publish bounded presentation state separately from values:

- visible/hidden;
- enabled/disabled;
- required/optional;
- inherited/overridden/source;
- reason text and severity; and
- current effective mode.

Conditions are provider-evaluated. The host does not execute an expression language
or infer behavior from another control's value. Hidden controls retain deterministic
focus recovery, and the fallback path exposes them disabled with a reason when
hiding would make the task incomprehensible.

### 5. Record presentations

`RecordCollection` remains the provider-owned data source. Composition selects one
of several host presentations:

- popup selector;
- persistent master/detail;
- table;
- visible repeater;
- ordered repeater; or
- compact directional/pad presentation.

Record presentations must support stable IDs, empty state, long names, disabled and
warning records, selection restoration, add/remove focus policy, and bounded window
virtualization. An ordered repeater emits semantic MoveBefore/MoveAfter operations;
it never rewrites provider arrays locally.

Nested macro editing is expressed as associated collections—macro, step, and target—
with one visible master/detail hierarchy. It is not implemented as three unrelated
popups.

### 6. Workflow sessions

Multi-step provider operations register a workflow descriptor and publish a current
workflow frame. Example flows include:

- create profile -> select activation mode -> persist sparse layer -> capture
  modifier -> Commit/Cancel;
- calibrate device -> sweep eight axes -> review accepted extrema -> Commit/Cancel;
- reassign duplicate device -> choose source/destination -> preview affected
  bindings -> Confirm/Cancel; and
- capture a throttle landmark -> show current sample -> accept into draft.

A workflow frame contains:

- stable workflow/session/generation IDs;
- current step and bounded step labels;
- instruction/status text with severity;
- progress rows with current/minimum/maximum/completion;
- allowed semantic commands: Back, Next, Retry, Commit, Cancel, custom bounded
  provider action;
- dirty/transaction participation;
- capture-active and device/stale state; and
- focus recommendation after a successful transition.

The provider owns the state machine. The host owns modal/page presentation, focus,
input routing, escape hierarchy, and generic confirmation.

### 7. Module-level pinned context

A module can register one bounded pinned context descriptor. HOTAS uses it for:

- Main/profile edit target;
- dirty indicator;
- sparse/inherited source summary; and
- Add Binding Layer workflow entry.

The host displays it consistently above every applicable module page. Context values
are read through provider callbacks and can request page refresh. The region never
scrolls away with the page body.

### 8. Live visuals and controls

The existing live API remains the telemetry lane. Composition adds general
associations for:

- RangeMeter marker <-> slider;
- RangeMeter marker <-> Capture Current action;
- band edge <-> typed draft value;
- TelemetryPlot series <-> tuning control/status;
- live component <-> card/row; and
- record/table row <-> live component scope.

Direct manipulation is optional and capability-gated. When present, dragging a
marker emits an ordinary typed draft write; it never mutates host-owned shadow
state. Keyboard/controller alternatives are mandatory.

## Host implementation responsibilities

### Native registry and session

- Validate all IDs, bounds, node trees, associations, fallback operability, and
  capability combinations at registration.
- Copy descriptors into host-owned storage.
- Query presentation/workflow frames only for the visible module/page/session.
- Pin provider callback leases and page transactions before any mutating command.
- Produce one immutable semantic page model for the bridge.
- Restore focus by stable semantic ID after refresh/mutation.
- Cancel captures/workflows in the correct modal order during route, provider, or
  host teardown.

### Bridge

- Serialize bounded semantic nodes and frames; never pass provider pointers.
- Preserve IDs and source order.
- Coalesce high-frequency live replacement separately from configuration and
  workflow transitions.
- Reject or visibly mark malformed/unavailable state rather than partially drawing
  ambiguous controls.

### Scaleform renderer

- Render semantic roles through host themes and responsive layout policy.
- Maintain identical reading and focus order.
- Virtualize long record/table windows.
- Provide pointer, keyboard, and controller operations for every semantic command.
- Make focus, selected, dirty, warning, unavailable, inherited, and stale states
  distinguishable without color alone.
- Keep provider-specific terminology in labels/data, not conditional renderer code.

## Capability negotiation and degradation

Composition capabilities should be granular enough that a provider never publishes
an unusable action:

| Capability | Enhanced behavior | Required fallback |
|---|---|---|
| semantic composition | cards/rows/columns | validated flat control order |
| semantic status | severity/detail/source | concise read-only text |
| anchors | jump navigation | ordinary scrolling |
| conditional state | hidden/disabled group with reason | visible disabled controls and reason status |
| record presentations | master/detail/table/repeater | bounded RecordCollection popup or transient Choice |
| pinned context | persistent edit target | repeated context group or Profiles page |
| workflows | multi-step session | omit operation or preserve safe legacy frontend |
| progress rows | live extrema/completion | bounded textual progress status |
| live associations | graph-linked controls | graph plus ordinary controls in flat order |
| direct live manipulation | drag/click visual edits | slider/action/controller operation |

The provider selects descriptors only after size- and capability-checking the host.
Host absence or rejection never disables provider gameplay.

## Reference vertical slices

### Flight Axes: composition foundation

This page proves the platform before other HOTAS pages are rebuilt:

- summary card;
- six semantic anchors;
- six axis cards with pictogram/text alternative;
- binding + Bind/Rebind/Clear + route status row;
- calibrated live RangeMeter with deadzone/saturation markers;
- associated invert/sensitivity/saturation/deadzone controls;
- shared lateral/vertical sensitivity source indication;
- throttle recipe graph and Capture Current actions; and
- pointer, keyboard, controller, capture, dirty, stale, and fallback behavior.

Acceptance requires task-complete in-game evidence. A screenshot of registered rows
is not acceptance.

### Ship Buttons: repeaters and conditional groups

Proves grouped binding/route rows, conditional menu reuse, shortcut repeater, preset
insertion focus, and macro page opening.

### Profiles: pinned context and workflows

Proves module context, sparse source state, dirty switching, and create-then-capture.

### Macros: nested ordered editing

Proves persistent master/detail, ordered steps, nested chord targets, polymorphic
Tap/Hold fields, and incomplete draft visibility.

### Devices: progress workflows

Proves manifest table, duplicate reassignment preview, eight-axis calibration
progress, hot-plug/device loss, Commit/Cancel, and saved-range table.

## Delivery milestones

### C0 — Model and validation

- Freeze the composition header draft and capability vocabulary.
- Implement registry validation, immutable model construction, flat fallback, and
  contract fixtures without Scaleform.
- Add malformed tree/reference/capacity/callback-lease tests.

**Implemented 2026-08-20.** The experimental declaration lives in
`include/AbsoluteControlCompositionExperimentalAPI.h`; the host-owned model and
validator live in `include/CompositionRegistry.h` and
`src/CompositionRegistry.cpp`. Registration copies bounded descriptors, validates
the semantic tree, exact control placement, record/live references, anchors,
pinned-context limits, typed associations, provider state, and page/module
capacity. Snapshots are immutable host copies and invalid or absent composition
produces a deterministic flat fallback. Provider state callbacks are leased and
unregister rejects while a callback is in flight.

This was C0's deliberate boundary. C1 now exports the query and installs the
experimental header after completing the corresponding presentation path.

### C1 — Status, card, row, condition, anchor

- Implement native model, bridge serialization, renderer, and input/focus behavior.
- Build a synthetic provider fixture independent of HOTAS.
- Benchmark a six-card page.

**Implemented 2026-08-20.** The product query advertises exactly
`kC1Capabilities`: semantic composition, semantic status, conditional state, and
anchors. Unsupported record, pinned-context, workflow, progress, live-association,
and direct-manipulation nodes/relations reject during registration.
General association records remain validated model/bridge infrastructure, but the
C1 product query rejects nonempty association arrays until a later vertical slice
implements their renderer and interaction meaning.

The active session snapshots provider state only for the selected page, computes
effective parent visibility/enabled state, recovers focus from hidden/disabled
controls by stable control ID, and preserves the flat page when composition is
absent, invalid, retired, or unavailable. The bridge serializes bounded nodes,
state, and associations into host-owned objects. The source-built movie renders
semantic card frames, grouped rows, severity/source summaries, and an eight-wide,
two-row-bounded anchor bar. Pointer, keyboard, and native controller input share
stable anchor-to-control targets and focus transitions.

`semantic_composition_integration_test` supplies a subscriber-independent six-card
fixture. It proves capability negotiation, unsupported-capability rejection,
status/condition propagation, six anchor journeys, hidden-focus recovery,
unregister fallback, and 1,000 complete native snapshots under a two-second gate
(approximately 15 ms total on the implementation workstation). Runtime visual/UX
acceptance remains required before this experimental lane is promoted to stable.

### C2 — Flight Axes vertical slice

- Publish HOTAS composition against the new primitives.
- Associate range visuals and controls.
- Complete three input-mode journeys and runtime capture/Apply/Cancel evidence.

**Implementation checkpoint 2026-08-20.** The query now advertises exactly
`kC2Capabilities`, adding live associations without advertising direct live
manipulation. `LiveSlot` nodes resolve an already registered live-channel ID on
the same module/page. Marker and series associations are checked against the
copied live descriptor rather than accepted as arbitrary strings. The movie
embeds a bounded plot or range meter in its owning semantic card, includes that
height in scrolling, and suppresses the old page-top dashboard when live slots
are present. C2 accepts at most one direct `LiveSlot` child per Card or Section,
matching the renderer's bounded embedded-component contract.

AbsoluteHOTAS publishes a 91-node Flight Axes tree containing the legacy summary,
six-axis jump bar, Thrust/Rotation/6-DOF/Fallback sections, nine cards, all 45
existing controls, and six per-axis live plots. Twenty-nine associations connect
bindings and tuning controls to the input/output series they explain. Card state
reports the bound source, required/unbound severity, aggregate readiness, reverse
strategy, digital fallback coverage, and unapplied-draft state. The ordinary
flat page remains the deterministic older-host or failed-live-registration
fallback.

Automated coverage proves descriptor bounds and exact control coverage, C2
capability negotiation, live-reference/series validation, bridge publication,
embedded renderer ownership, anchor focus, provider capture infrastructure, and
composition-aware draft/Cancel status. Physical pointer/keyboard/controller
journeys, in-game capture and Apply/read-back, final card spacing, long binding
labels, and visible-frame performance remain required before C2 is runtime
qualified. The legacy icon family and per-card deadzone/saturation band preview
are not yet expressible; labels remain authoritative and the separate Throttle
Setup page retains detailed detent/reverse/boost landmarks.

### C3 — Collections and pinned context

- Implement table/repeater/master-detail and module context.
- Port Ship Buttons and Profiles.

### C4 — Workflow and progress

- Implement provider workflow sessions and progress rows.
- Port profile create/capture, device reassignment, and calibration.

### C5 — Ordered nested editors

- Implement ordered and nested collection presentation.
- Port Macros and shortcut rows.

### C6 — Suite qualification and SDK candidate

- Exercise HOTAS, Power, Head Tracking, and AbsoluteZero together.
- Complete pointer, keyboard, controller, display-shape, hot-plug, stale, teardown,
  frame-time, and accessibility matrices.
- Promote only proven structures into the stable SDK.

## Evidence gates

Every primitive requires:

- ABI layout, size-gating, old-host, malformed input, capacity, unregister, and
  callback-lease tests;
- immutable native-model tests;
- bridge source and serialization tests;
- ActionScript architecture and input-router tests;
- pointer, keyboard, controller, focus restoration, modal hierarchy, and teardown
  journeys;
- source-built SWF provenance;
- performance evidence with the page visible and proof that polling stops when
  hidden; and
- at least two subscriber use cases before stable SDK promotion, unless the
  primitive is an intentionally experimental acceptance candidate.

## Migration policy for the current HOTAS scaffold

- Keep its provider repositories, capture service, atomic transactions, telemetry,
  and ownership boundaries; those are useful domain infrastructure.
- Do not add more flat controls to simulate compound tasks.
- Replace one page at a time behind capability negotiation.
- Keep the Dear ImGui workbench as the behavioral reference until each page passes
  its runtime matrix.
- Label non-parity pages experimental in Control.
- Remove scaffold workarounds only after their semantic replacement and fallback are
  verified.

This turns the HOTAS transition into platform development: each completed page
expands what every Absolute module can express without sacrificing ownership,
compatibility, or input parity.
