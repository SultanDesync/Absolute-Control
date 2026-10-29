# Absolute suite refactor transition plan

> **Archived:** superseded by the [native-backend-first plan](SUITE-REFACTOR-TRANSITION-PLAN.md)
> on 2026-09-18. Transitional rollout steps below are historical, not current requirements.

> **Status:** Planning baseline, 2026-09-18. This document turns the target
> ownership model into checkpoints and records authority conflicts that must be
> eliminated before implementation moves between DLLs.

Source-confirmed findings and their release gates are recorded in
[the authority risk register](SUITE-REFACTOR-AUTHORITY-RISKS.md). The safeguards
below are proposed contracts, not claims of completed implementation or runtime
qualification.

Implementation evidence is tracked separately: [checkpoint 01](REFACTOR-CHECKPOINT-01.md)
covers initial menu hardening, and [checkpoint 02](REFACTOR-CHECKPOINT-02.md)
adds the internal headless registry/authority harness. Their remaining gates are
still open; neither checkpoint transfers production input or output authority.
[Checkpoint 03](REFACTOR-CHECKPOINT-03.md) adds a DeviceService contract draft and
legacy token fixtures, including subscriber-specific POV dialect conflicts.

## Outcome

Absolute Control becomes the suite's universal runtime and presentation host.
Gameplay modules remain subscribers that own only their distinct domain.

The runtime core and menu shell are separate responsibilities even when they
ship in the same plugin:

```text
device providers -> normalized source frames -> route/action authority
                                                    |
gameplay subscribers -> domain intents/config ------+
                                                    |
                                            resolved action frame
                                                    |
                                             output backends

menu subscribers -> declarative pages -> menu shell -> draft commands
                                               |
                                      subscriber-owned persistence
```

The runtime service lifetime must be independent of the menu. Opening or closing
a menu legitimately changes focus, capture, and gameplay eligibility; those
changes flow through explicit context/lease policy while device acquisition and
core services continue running.

### Native device and profile redesign direction — 2026-09-18

The target uses native Absolute Control device/channel references and a
redesigned profile model. Existing HOTAS serialization must not dictate that
model or require an enduring runtime profile-translation layer. Rebinding is
an acceptable migration outcome where identity or behavior cannot be established
reliably; universal automatic conversion is not a release requirement.

Keep the following concerns separate in the design:

- Physical device identity and calibration belong to the device configuration
  model; enumeration indices and display names are not saved canonical identity.
- Binding assignments reference native device/channel IDs and semantic actions.
  Runtime connection handles are resolved separately and are never persisted.
- Profiles select assignments, overrides and domain settings. Define inheritance,
  explicit unbinding, activation triggers and switching behavior deliberately,
  rather than reproducing HOTAS's INI overlay mechanism by accident.
- Define one persistence owner for each record before implementation. The exact
  split between suite route/profile records and subscriber domain profiles is
  still a design decision; the menu remains a draft editor, not a second writer.

A one-time, versioned importer may reuse unambiguous legacy assignments. It must
write the new native representation through its owning component, preserve the
old files for rollback, and report unresolved devices/bindings. Never guess
between identical devices or silently reinterpret POV dialects. The new runtime
must work with only its native records after import. No importer is promised
until the new schema and import feasibility have been reviewed.

This direction changes the saved-configuration strategy. Transitional DLL/API
facades and old-suite startup selection remain separate, bounded compatibility
decisions; they do not require the new profile engine to execute legacy files.

## Transition vocabulary

- **Authority**: the one component allowed to make a decision or mutate a
  resource.
- **Provider**: a replaceable implementation that acquires data or performs a
  resolved output operation.
- **Subscriber**: a domain module that declares configuration, policy,
  telemetry, menu pages, and semantic intents.
- **Facade**: a compatibility ABI that preserves an old caller contract while
  delegating to the new authority.
- **Shadow**: a non-authoritative implementation that receives copied input and
  reports comparisons but cannot capture, persist, acquire a device, synthesize
  input, or write game state.
- **Lease**: time-bounded, revocable permission to produce or consume an
  exclusive resource.

Moving code does not itself transfer authority. A checkpoint transfers
authority only when its cutover gate is met.

## Non-negotiable transition invariants

1. One polling owner per physical input provider.
2. One active binding-capture session suite-wide.
3. One canonical identity for each device, channel, binding, context, and game
   action; display labels are never identities.
4. One final arbiter per semantic game action and backend consumption boundary.
5. One owner for every held synthetic key/button. The same owner must release
   it on route loss, context loss, backend failure, shutdown, or revocation.
6. One configuration writer per subscriber record. Absolute Control may hold a
   draft but never persists another module's domain configuration.
7. One input-owning menu presentation at a time. Pause Menu entries from other
   mods remain foreign rows and are preserved.
8. Provider callbacks are never retained beyond their registered lifetime.
9. High-frequency samples do not travel through the lifecycle event bus or the
   Scaleform bridge.
10. No legacy fallback may run concurrently with the replacement authority.
11. Failure releases only outputs the suite owns, using the action's documented
    release operation. Neutralization must not erase a successor's writes or
    restore stale native state. Unowned game memory is never blindly zeroed.
12. A subscriber continues its gameplay behavior when the menu/SWF is absent.

## Authority ledger

| Resource or decision | Current authority | Transitional authority | Target authority | Cutover evidence |
|---|---|---|---|---|
| DirectInput enumeration/acquisition | AbsoluteHOTAS `DeviceManager` | HOTAS remains sole poller; core consumes mirrored frames | Absolute Control DirectInput provider | Stable identity, hotplug, reconnect, axis/button/POV parity |
| Device/channel identity | HOTAS Input Bus token and device GUID conventions | Shadow records legacy identity; optional import resolves known assignments | Absolute Control native device registry | Native identity, explicit unresolved/rebind outcomes, and import fixtures |
| High-rate device snapshots | HOTAS Input Bus | Legacy facade reads core snapshots after device cutover | Absolute Control data plane | Sequence/timestamp/edge parity under frame stalls |
| Binding capture | HOTAS Input Bus plus provider-specific menu callbacks | Core owns lease; legacy capture API delegates | Absolute Control capture service | Exclusivity, cancellation, timeout, disconnect, menu-close tests |
| Binding conflict policy | Individual subscriber callbacks | Subscriber validates domain assignment; core detects canonical channel conflicts | Split: core detects, subscriber decides domain reassignment | Cross-module and same-module conflict tests |
| Runtime context | HOTAS publishes pilot/gameplay/targeting state | Named context producers publish evidence; core resolves | Absolute Control context service | Proven source precedence, freshness, unknown state, transitions |
| Routes/transforms/layers | HOTAS and each subscriber independently | Existing gameplay paths remain authoritative; core shadows results | Absolute Control route engine | Recorded trace parity and simultaneous-input tests |
| Semantic action identity | Implicit per module/API | Registry exists without owning production writes | Absolute Control action registry | Stable IDs, shape metadata, capability discovery |
| Ship/camera writer ownership | Pair-specific HOTAS APIs | Existing leases adapt to core resource leases | Absolute Control arbiter | Contention, pre-emption, revocation, neutralization tests |
| Native game writes | HOTAS/Zero/Power/Head Tracking specialized hooks | Existing writer remains authoritative per action | Absolute Control injection backends where proven | Per-action runtime evidence and rollback qualification |
| `SendInput` | AbsoluteHOTAS ship output and module-specific compatibility paths | Existing code wrapped as an exclusive backend | Absolute Control compatibility output backend | Held-state ledger and no-feedback/no-double-write tests |
| Subscriber draft and persistence | Each subscriber | Unchanged | Each subscriber | Apply/Discard/failed-save/reload tests |
| Menu registration and rendering | Absolute Control Panel host | Stable v1 plus SDK helpers; experimental contracts remain optional | Absolute Control menu shell | ABI fixtures and multi-subscriber runtime qualification |
| Pause Menu contribution | Absolute Control and other mods independently | Owned row identity, collision checks, fail-closed coexistence | Cooperative Absolute Control adapter | Vanilla, OSF UI, reordered list, reopen and reload matrix |

## Checkpoint plan

These checkpoints form dependency tracks, not one long serial queue:

- **Compatibility first:** R0 diagnostics plus menu hardening M1 below.
- **Core:** R1 -> R2 -> R3, with R4 context contracts designed before R3 cutover.
- **Configuration:** design native device, binding and profile records alongside
  R2, before production profile integration or any automatic migration. R3 waits
  for native activation/switching, calibration and rebinding qualification.
- **Output:** R4 + R5 -> R6, one action family at a time.
- **Subscriber/UI:** R7 common controls, SDK, and grid work can start alongside
  R1/R2; only core-backed binding recording waits for R3. FPS/rover research and
  Steam/controller provider research do not block grid or compatibility work.
- **Retirement:** R8 follows a declared supported-version compatibility window.

### M1 — compatibility gates before broad subscriber migration

- Prove registration ownership; plain `Duplicate` must not be advertised as
  verified idempotence. Include the new subscriber-kit helper in this gate.
- Validate the loaded menu factory and native/SWF protocol before declaring the
  presentation ready. A protocol extension must preserve declared v1 support.
- Qualify OSF opening before and after AC, and a later PauseMenu list mutator.
  Current one-way open checks do not establish mutual exclusion.
- Change the visible row to `ABSOLUTE CONTROL` in its own implementation slice;
  keep existing export/action/menu identities until a separate migration.
- Keep existing callback leases, generation guards, and foreign-row preservation.

Gate: ownership-collision, partial-registration, stale-SWF, late-list-mutation,
and two-host open-order scenarios have explicit passing outcomes. Source tests
alone cannot qualify the in-game coexistence cases.

Rollback: retain the previous coherent DLL/SWF pair and standalone activation
route; do not replace or close another mod's presentation.

### R0 — freeze and observe

Purpose: make current authority visible before moving it.

- Name every mutable resource in an authority ledger in diagnostics.
- Give every capture, route writer, menu session, and output hold a unique
  generation or lease ID.
- Inventory existing timestamps and counters; add missing authority/connection
  generations through internal diagnostics or an optional versioned contract.
  Do not change the frozen v1 layout for instrumentation.
- Record start, renew, revoke, neutralize, and fallback events.
- Freeze the existing HOTAS Input Bus ABI and its binding-token grammar.
- Freeze stable Absolute Control menu ABI v1; do not promote the experimental
  composition/live contracts yet.

Gate: a runtime log can answer who owned a device, capture, context signal,
action, and synthetic hold at any point in a session.

Rollback: diagnostic-only; no behavior changes.

### R1 — establish the headless core boundary

Purpose: prevent the menu DLL structure from defining the runtime architecture.

- Add an internal service registry with capability and version discovery.
- Define lifecycle states: unavailable, starting, ready, degraded, stopping.
- Define separate data, event, and command planes.
- Make the core start independently of Scaleform registration and Pause Menu
  injection.
- Define thread-affinity and callback-duration rules for every service.
- Add bounded snapshot/history storage; subscribers never retain provider-owned
  pointers.

Gate: a headless harness starts and stops the core, registers mock providers and
subscribers in arbitrary order, and proves that menu failure has no effect.

Rollback: core stays dormant; all legacy paths remain authoritative.

### R2 — normalized device contract and DirectInput shadow

Purpose: define the general device vocabulary before adding another backend.

- Model button, scalar, vector, relative delta, touch/contact, POV, and pose
  channels without assuming DirectInput layout.
- Separate persistent physical identity, runtime connection instance, channel
  identity, and user-facing name.
- Specify sample sequence, timestamp, freshness, disconnect, reconnect, and
  hotplug behavior.
- Mirror HOTAS frames into a shadow core provider. Do not open or acquire the
  same DirectInput devices twice.
- Build trace comparison tools for axes, buttons, POVs, edges, and reconnects.

Gate: recorded and live shadow comparisons demonstrate lossless representation
of all existing HOTAS inputs and stable native identities. Legacy assignments
have explicit importable, unresolved or rebind-required outcomes.

Rollback: stop mirroring; HOTAS remains unchanged.

### R3 — transfer device and capture authority

Purpose: make Absolute Control the one hardware acquisition and recording host.

- Move DirectInput implementation behind the core provider contract.
- Convert `AbsoluteHOTAS_QueryInputBusApi` into a compatibility facade over core
  snapshots and core capture leases.
- Provide one capture state machine independent of menu lifetime.
- Let the menu shell present capture while the core owns device recording.
- Preserve subscriber-owned validation and reassignment policy.
- Select authority at startup after a bounded readiness barrier. Old HOTAS
  versions cannot be ordered to stop acquiring through an API they do not
  implement: an old HOTAS remains the provider or the core rejects that pairing.
- Qualify native profile activation, switching, calibration and held-output
  cleanup before disabling the HOTAS polling loop. Report deliberate behavior
  changes and bindings needing reassignment. Preserve legacy context delivery
  for supported facade consumers; R4's context contract is a prerequisite.
- Update subscribers to prefer the core service. Select HOTAS fallback only when
  that service is absent or the startup authority plan explicitly selects HOTAS
  and establishes that the core will not acquire the resource. A present but
  unready/failed core is not evidence of non-ownership.

Gate: HOTAS, Zero, Power, and Head Tracking pass device, edge, capture, timeout,
disconnect, and native saved-binding tests. Supported legacy API consumers pass
their declared facade matrix; legacy profile execution in the new runtime is
not required. Native clean setup, rebinding and any offered import are qualified.

Rollback: a startup switch selects the legacy HOTAS owner before either side
acquires devices. It cannot change owners mid-session.

### R4 — separate and centralize context evidence

Purpose: remove pilot/gameplay state from the device ABI.

- Define named context signals with source, confidence/validity, timestamp, and
  freshness.
- Treat unknown as distinct from false.
- Establish deterministic precedence when multiple producers know a signal.
- Adapt the current HOTAS runtime-context fields into a legacy context provider.
- Keep context observation separate from action policy.

Gate: boarding, leaving a seat, pause/menu transitions, loading, targeting,
death, focus loss, and stale-source traces resolve deterministically.

Rollback: consumers can read the legacy context facade until migrated.

### R5 — semantic actions, routes, and arbitration in shadow

Purpose: create the universal orchestration model without moving proven hooks.

- Register stable semantic action IDs and shapes: momentary, held, scalar,
  vector, delta, and state request.
- Declare combination policy per action: exclusive, priority, additive,
  maximum-magnitude, latest-valid, or domain-defined reducer.
- Define transforms, layers/chords, gates, dead zones, curves, and rate limits.
- Resolve source frames into one action frame with an explicit tick and context
  generation.
- Shadow existing HOTAS and subscriber decisions against recorded traces.
- Ensure simultaneous roll, thrust, look, and button actions do not accidentally
  serialize into mutually exclusive lanes.

Gate: route-engine results match existing behavior where parity is required and
pass new simultaneous-input/conflict tests.

Rollback: discard shadow action frames; existing modules continue writing.

### R6 — output backend contract and action-by-action cutover

Purpose: centralize writes only when the replacement is safer than the current
specialized path.

- Define backend capability discovery by semantic action, game version, context,
  and value shape.
- Wrap `SendInput` as a compatibility backend with an owned held-state ledger.
- Add researched native injection backends independently of device providers.
- Resolve a coherent action frame and let each backend consume it at its
  verified hook/phase. Do not assume all game writes occur once per frame:
  per-hook or per-object writers may need repeated application of the same
  resolved intent with edge deduplication and object-generation checks.
- Require neutralization on lease loss, context loss, stale input, exception,
  version mismatch, and shutdown.
- Prevent synthetic output from being re-consumed as physical input.
- Cut over one action family at a time; never enable legacy and core writers for
  the same action.

Gate: each action has offset/hook evidence, lifetime tests, conflict tests,
neutralization proof, version gates, and a startup-selectable rollback backend.

Rollback: select the prior writer on the next startup. Never fail over during a
held action unless both backends share a proven neutralization handshake.

### R7 — subscriber and menu simplification

Purpose: leave subscribers with domain code and inexpensive menu declarations.

- Migrate common host discovery, ABI checks, registration rollback, value
  codecs, and transaction mechanics to the subscriber kit.
- Make standard float sliders, boolean toggles, choices, actions, and binding
  recorders declarative.
- Make grid placement authoritative so a composed control is rendered once;
  retain flat pages only as an explicit compatibility fallback.
- Keep provider configuration validation, Apply/Discard, save errors, telemetry,
  and advanced instruments provider-owned.
- Replace provider-specific device capture callbacks with the core recorder when
  the binding targets a core device channel.

Gate: at least Zero and one device-heavy module use the standard path without
duplicated host/capture plumbing, and menu behavior is unchanged for old hosts.

Rollback: subscriber retains its old registration/capture adapter for one
compatibility release.

### R8 — remove inversions and retire facades

Purpose: finish the ownership transfer after the compatibility period.

- Remove generic DirectInput and suite capture implementation from HOTAS.
- Remove new feature work from pair-specific camera/mouse/writer APIs.
- Remove legacy discovery only after telemetry/log evidence shows no supported
  subscriber still requires it.
- Keep token/config migrations idempotent and versioned.
- Publish the supported provider/subscriber/injection SDK as one provenance-
  checked package.

Gate: clean-install, upgrade, downgrade, partial-suite, missing-menu, and mixed-
version matrices all have declared outcomes and pass.

Rollback: reinstall the previous coherent suite package; individual DLL
downgrades are not promised across a retired ABI boundary.

## Hidden authority conflicts and traps

### Provider-owned capture versus core-owned capture

The stable menu ABI currently says the provider owns polling, capture policy,
and binding syntax while the host owns presentation and navigation lock. That is
correct for the current architecture but conflicts with the target universal
device recorder. Do not silently reinterpret those callbacks. Add a distinct
core recorder capability; adapt old callbacks until subscribers migrate.

### Discovery order is not dependency resolution

Subscribers currently discover other loaded DLLs using module/export lookup.
If discovery occurs once before the provider is ready, an optional dependency
can remain absent for the whole session. The core needs readiness events or
idempotent retry, plus validation of ABI size, version, capabilities, and
lifecycle on every retained handle.

### A fallback can become a second authority

Fallback requires service absence or explicit negotiated non-ownership. Busy,
degraded, stale, rejected, or unsupported-action results must not cause a second
provider to poll, capture, or write concurrently. Fallback selection is a
startup decision or an explicit lease transfer. A generic `unavailable` status
describes capability health, not permission for another provider to take over.

### DirectInput acquisition needs one suite authority

A shadow DirectInput implementation must not create/acquire the same devices as
the current HOTAS owner. Shadowing operates on copied frames until cutover.
Foreground/background cooperative levels, reconnect loops, and focus changes
must be owned by the selected provider. This is a suite ownership rule, not a
claim that the current DirectInput cooperative mode is OS-exclusive.

### Edge counts cannot be reconstructed casually

Subscribers use press/release counters rather than only current button state.
Moving polling cadence or thread ownership can lose, duplicate, or reorder
edges. The normalized contract needs monotonically increasing source sequence
and digital edge counters with defined wrap/reconnect behavior.

### Device GUID is not the whole identity problem

Physical identity, connection instance, product identity, and duplicated
identical devices are different concepts. Tokens must survive ordinary
reconnects without silently rebinding to the wrong identical controller.

### Context is evidence, not policy

“Piloting,” “on foot,” “gameplay active,” and “targeting” may come from different
hooks and age differently. The core resolves named evidence; it does not let a
device provider decide gameplay policy. Unknown/stale must not collapse to
false.

### Arbitration requires action-specific algebra

There is no universal “last writer wins.” Analog thrust may be exclusive, look
deltas additive, toggles edge-triggered, and state requests priority-based.
Every action declares its reducer, neutral value, lease behavior, and stale
timeout.

### `SendInput` has both hold and feedback hazards

Key-down ownership must survive route changes and always produce a matching
key-up. Synthetic keys can also re-enter game or plugin input paths. Tagging is
limited, so routing must suppress self-generated feedback by design and never
run two SendInput writers for one action.

### Game tick, polling tick, and UI tick are different clocks

Device sampling may run faster than game updates; Scaleform may stop while
paused; hooks can run on different threads. The action frame needs an explicit
target tick and immutable source snapshot. No gameplay writer may depend on a
menu frame callback.

### DLL callback lifetime crosses ownership boundaries

Registered function pointers and context pointers pin subscriber lifetime.
Dirty drafts, active captures, live telemetry, queued UI commands, and worker
callbacks must all drain before unregistration succeeds. A generation check is
required after asynchronous hops.

### Configuration preview can masquerade as persistence

The menu host owns a draft/session, but only the subscriber may validate and
save its configuration. Runtime preview must be reversible on Discard, menu
destruction, provider loss, or failed Apply. Core route configuration and module
domain configuration need separate records even when edited on one page.

### Menu identity and action identity are not labels

`ABSOLUTE CONTROL`, `MOD SETTINGS`, translated labels, menu names, action
numbers, and owner markers have different purposes. Coexistence logic must use
stable owner identity plus action token and preserve foreign rows.

### Experimental menu APIs can accidentally become permanent

Composition and live-component contracts expose layout and high-rate behavior
that are still evolving. Standard controls should remain on the stable ABI;
experimental contracts require explicit capability checks and fallback content.

### Packaging can recreate old ownership accidentally

Vendored headers, duplicated DLL names, stale SWFs, and partial suite upgrades
can pair a new subscriber with an old host. Packages need an API manifest,
provenance hashes, capability matrix, and mixed-version tests. The player-facing
rename must not rename exports, menu IDs, or configuration files without a
separate compatibility migration.

## Required test matrix

Every authority-changing checkpoint must cover:

- clean install and upgrade from the last public suite release;
- Absolute Control present/absent and menu SWF present/absent;
- HOTAS present/absent and mixed supported versions;
- Zero, Power, and Head Tracking independently present/absent;
- vanilla Pause Menu, OSF UI, reordered rows, repeated open/close, save/load;
- device connected at startup, hotplugged, disconnected during capture, and
  reconnected;
- two identical devices and reordered device enumeration;
- focus loss, pause, loading, seat/context transition, and stale producer;
- simultaneous analog axes, chords, opposing writers, and capture contention;
- held `SendInput` output during route/context/backend loss;
- provider registration before/after host readiness and failed registration
  rollback;
- dirty draft, Apply failure, Discard, forced menu close, and subscriber loss;
- supported and unsupported game versions with injection disabled fail-closed;
- explicit shutdown and save/load harnesses; runtime DLL hot unload stays
  unsupported until a separate retire/drain contract is implemented.

Select the cases relevant to each checkpoint, then run the combined suite matrix
before release. Include hostile registration collisions, an old HOTAS alongside
a new core, SWF/native mismatch, OSF opening second, and a late list rebuilder.

## Decisions required before freezing the runtime ABI

1. Whether the core is a service inside the Absolute Control plugin or a
   separately named runtime DLL. This is a deployment decision, not permission
   for the menu to own runtime lifetime.
2. Canonical identifier formats for provider, physical device, connection,
   channel, context, action, route, and subscriber.
3. Snapshot ownership and threading model: copy-out structs, immutable handles,
   or bounded ring views.
4. Action shapes, combination policies, lease priorities, and neutralization
   guarantees.
5. Native binding/profile schema and versioning, one-time import feasibility,
   unresolved-binding UX and rollback to preserved legacy files. An enduring
   runtime translator for HOTAS profiles is excluded from the target.
6. Supported late-discovery and mixed-version behavior.
7. Which APIs are stable public SDK surface and which remain research-only.
8. Saved route/configuration ownership and whether future migrated input features
   require the core. Preserve current standalone behavior during the compatibility
   window; retiring that fallback is an explicit release/product decision.

## Delegation plan for implementation

Use lower-tier agents for bounded source inventories, pure helper/codecs, fixture
construction, and documentation consistency. Give each agent one resource/contract
and an acceptance criterion, with exclusive file ownership for edits. The main
agent reviews cross-module ownership, ABI promises, hook lifetime, and release
gates. Escalate difficult runtime arbitration or reverse engineering only after
a focused task produces a concrete unresolved issue. Do not duplicate broad suite
audits across agents or run full builds for documentation-only work.

## Immediate next artifacts

- A native device/binding/profile design defining record ownership, inheritance,
  explicit unbinding, activation and calibration, plus import/rebinding outcomes.
- An executable authority-ledger test harness with mock providers and writers.
- A draft `DeviceService` ABI containing no DirectInput-specific constants.
- A legacy HOTAS-token codec and round-trip fixture corpus.
- Recorded HOTAS input/context/action traces for shadow comparison.
- A semantic action catalogue seeded from current ship, camera, FPS, and rover
  research, with confidence and backend-support fields.
- A suite compatibility manifest consumed by packaging validation.
