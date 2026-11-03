# Suite refactor authority risk register

Audited 2026-09-18 against the local working trees, including existing uncommitted
work. References are source anchors at this checkpoint, not immutable release
evidence. OSF findings refer to the local `_osf-ui-reference` checkout; they do
not establish the behavior of every OSF version.

This is a planning/source audit. None of the scenarios below was reproduced in
Starfield during this audit. **Observed** means the source behavior was checked;
the described consequence is a transition risk unless stated otherwise. All
items remain open. See [the implementation tracks and gates](SUITE-REFACTOR-TRANSITION-PLAN.md).

**Execution update:** the native-backend-first plan supersedes the transitional
facade/fallback strategy referenced by the original gate labels below. Findings
remain evidence to address in the native design and subscriber adaptation;
they do not require building a legacy runtime compatibility layer.

## Gates before moving runtime authority

### A01 — peer presence can surrender authority without a ready successor

**Observed:** HOTAS detects both loaded modules and DLLs on disk, then selects
external Zero/camera ownership before starting its own hooks. Zero selects its
shared-hook path based on HOTAS presence, requires a compatible installed writer,
and remains inactive on connection failure. Its runtime initialization is
one-shot. Head Tracking similarly yields to the HOTAS compatibility contract.

Evidence: [HOTAS presence checks](../../AbsoluteHOTAS/src/Main.cpp#L15),
[owner selection](../../AbsoluteHOTAS/src/Main.cpp#L150),
[Zero initialization](../../AbsoluteZero-Ship-Control/src/Main.cpp#L56),
[Zero handshake](../../AbsoluteZero-Ship-Control/src/HotasMouseCompatibility.cpp#L42),
[Head Tracking initialization](<../../Absolute Head Tracking/src/Main.cpp#L64>).

**Risk:** an incompatible or failed peer can leave a gap where both modules
decline the same behavior. This is not proof of a normal-load-order failure:
Zero/Head Tracking defer runtime initialization to the SFSE post-load phase.

**Gate (R1/R3/R6):** distinguish present, ABI-compatible, ready, owner, and
degraded. Claim shared resources through a negotiated startup plan. Yield only
after a verified successor is ready; never automatically install a competing
hook when a peer may already own the seam. Test failed load, stale DLL on disk,
unsupported ABI, and hook failure with one owner or an explicit unavailable state.

### A02 — generic device availability depends on a flight hook

**Observed:** HOTAS starts `ThrottleController` only if its throttle hook installs;
the controller initializes InputBus and drives acquisition, publication, and
suite binding capture in one polling loop.

Evidence: [startup gate](../../AbsoluteHOTAS/src/Main.cpp#L176),
[InputBus initialization](../../AbsoluteHOTAS/src/ThrottleController.cpp#L1113),
[polling chain](../../AbsoluteHOTAS/src/ThrottleController.cpp#L1218).

**Risk:** moving only the API export preserves the hidden dependency: a failed
ship hook still disables otherwise usable device services for other domains.

**Gate (R1/R3):** devices/capture remain usable with ship injection unavailable,
HOTAS disabled, or the SWF missing. Separate service readiness by capability;
core process lifetime cannot depend on gameplay-hook or UI readiness.

### A03 — published normalization embeds HOTAS calibration and profile policy

**Observed:** InputBus publishes raw axes and normalized axes using calibration
passed from the current HOTAS configuration. Calibration lookup uses enumeration
index plus axis usage. Changing HOTAS profile cancels the global bus capture.

Evidence: [normalization](../../AbsoluteHOTAS/src/InputBus.cpp#L144),
[caller](../../AbsoluteHOTAS/src/ThrottleController.cpp#L1219),
[profile cancellation](../../AbsoluteHOTAS/src/InputBus.cpp#L620).

**Gate (R2/R3):** explicitly separate physical/raw samples, universal device
calibration, and profile/route transforms. Tag transformed data with its policy
revision. Preserve old normalized values through the legacy facade; do not apply
calibration twice or silently change subscribers' values. Map saved calibration
to stable identity, and define which profile changes invalidate which capture.

### A04 — capture has several independent entry points

**Observed:** InputBus arbitrates its own capture with `Busy`, but HOTAS also has
WizardCapture and suite command capture using DeviceManager cached samples. The
stable menu ABI delegates device recording to provider callbacks.

Evidence: [bus capture](../../AbsoluteHOTAS/src/InputBus.cpp#L496),
[wizard snapshots](../../AbsoluteHOTAS/src/WizardCapture.cpp#L110),
[suite capture](../../AbsoluteHOTAS/src/SuiteCommandBindings.cpp#L238),
[provider callback contract](../include/AbsoluteControlPanelAPI.h#L162).

**Risk:** moving InputBus alone does not establish suite-wide capture ownership.
An inactive UI path may still expose a callable legacy capture API.

**Gate (R3):** inventory every begin/poll/cancel API and route it through the same
capture lease, including legacy wizards. Distinct capture algorithms may remain,
but only the lease holder records. Preserve release-to-arm, timeout, cancellation,
and event-consumption rules. `Busy` never triggers a second recorder fallback.

### A05 — reconnect, enumeration, and edge history need separate generations

**Observed:** bus table rebuilding checks device count; producer generation
advances on initialization/table rebuild. DeviceManager reconnects individual
devices and applies recovery filtering. Digital consumers retain cumulative
counter baselines keyed by producer generation.

Evidence: [bus generation](../../AbsoluteHOTAS/src/InputBus.cpp#L588),
[device recovery](../../AbsoluteHOTAS/src/DeviceManager.cpp#L208),
[Zero baseline](../../AbsoluteZero-Ship-Control/src/InputBusClient.cpp#L399),
[Power baseline](<../../Absolute Power/src/InputBusClient.cpp#L275>).

**Risk:** a new hotplug implementation that replaces a device without changing
the count can leave stale identities. Provider transfer/reconnect can replay held
buttons or re-arm axes unexpectedly. Existing counters report “a press occurred”;
they do not preserve arbitrary edge order through a stall.

**Gate (R2/R3):** specify provider epoch, enumeration revision, device connection
epoch, sequence, timestamp, counter semantics and overflow behavior. Preserve
HOTAS recovery behavior explicitly, with raw versus gameplay-eligible samples.
Test same-count replacement, identical devices, lost/reconnected holds, changed
poll cadence, and slow consumers. Do not claim hardware edges the source API
never observed can be recovered from snapshots.

### A06 — owned output cleanup is already implemented and must survive transfer

**Observed:** ShipOutput tracks multiple owner IDs per synthetic output; it sends
release only when the last owner leaves. Cleanup also releases native owners.
HOTAS profile changes and controller shutdown invoke output cleanup.

Evidence: [hold ledger](../../AbsoluteHOTAS/src/ShipOutput.cpp#L255),
[profile switch](../../AbsoluteHOTAS/src/ThrottleController.cpp#L1080),
[shutdown](../../AbsoluteHOTAS/src/ThrottleController.cpp#L1800).

**Gate (R5/R6):** preserve that behavior across modules, not just per action.
Different actions can map to the same physical output key. Arbitration therefore
needs semantic action ownership plus an output-resource hold ledger. Revocation
must not release a surviving owner's hold. Add synthetic-feedback tests and
backend-specific release operations; neutralization cannot blindly zero shared
memory or restore state saved before a successor acquired it.

### A07 — observer, executor, and writer capabilities are different authorities

**Observed:** Zero borrows HOTAS's accumulator/writer synchronization. Head
Tracking borrows HOTAS's cockpit observation while taking camera ownership.
Power selects a HOTAS execution path using an API whose validated fields describe
command bindings; without that selection it attempts its own game-update hook.

Evidence: [Zero shared seam](../../AbsoluteZero-Ship-Control/src/HotasMouseCompatibility.cpp#L69),
[Head Tracking observer selection](<../../Absolute Head Tracking/src/Main.cpp#L77>),
[Power API probe](<../../Absolute Power/src/SuiteHost.cpp#L49>),
[Power executor selection](<../../Absolute Power/src/Main.cpp#L36>).

**Gate (R1/R4/R6):** expose observer freshness, execution readiness, and writer
ownership as separate capabilities. A compatible config/binding API is not proof
of a running game-thread executor. Preserve current context heartbeat and age
semantics; stale context becomes unknown. Draw each facade's delegation direction
to prevent core -> HOTAS -> core recursion.

### A08 — the same saved POV token has different subscriber meanings

**Observed (checkpoint 03):** HOTAS emits `@pov:0..3:<direction>` using zero-based
POV indices; Head Tracking reads that form unchanged. Zero and Power decrement
indices 1..4, so a captured `@pov:1:right` selects the second POV in Head Tracking
but the first POV in Zero/Power. Their flat direction aliases do not have this
ambiguity. This is a source-confirmed grammar conflict, not in-game reproduction.

Evidence: [producer](../../AbsoluteHOTAS/src/InputBus.cpp), `CommitCaptureLocked`;
[Head Tracking](<../../Absolute Head Tracking/src/InputBusBinding.cpp>), `Parse`;
[Zero](../../AbsoluteZero-Ship-Control/src/InputBusClient.cpp), `JoystickBindingPolicy::Parse`;
[Power](<../../Absolute Power/src/JoystickShortcut.cpp>), `JoystickBindingPolicy::Parse`.

**Gate (R2/R3):** retain original saved tokens and record their owning subscriber
dialect. Never silently reinterpret an existing Zero/Power binding as producer
syntax. Distinguish fresh capture evidence (`kind/controlId`) from saved text;
qualify all four POVs per subscriber before a versioned config migration. The
[checkpoint 03 codec/fixtures](REFACTOR-CHECKPOINT-03.md) expose ambiguous tokens
but do not fix production subscribers or establish runtime parity.

## Menu compatibility gates

### M01 — duplicate registration does not prove provider identity

**Observed:** the host returns `Duplicate` for matching string IDs. Zero accepts
that as successful registration. The new SubscriberKit helper also describes
`Duplicate` as idempotent success and only attempts rollback when it created the
module. Rollback itself can be rejected by the host's callback/transaction leases.

Evidence: [page/module duplicate checks](../src/MenuApiHost.cpp#L429),
[Zero registration](../../AbsoluteZero-Ship-Control/src/ControlPanelSubscriber.cpp#L526),
[new helper](../include/AbsoluteControlSubscriberKit.h#L239),
[unregister lifetime gate](../src/MenuApiHost.cpp#L490).

**Risk:** two versions/providers claiming the same ID can produce mixed pages or
a subscriber that thinks its callbacks were registered when they were not.

**Gate (M1/R7):** define owner-bound bundle registration, same-owner retry, foreign
duplicate rejection, and observable rollback completion. An owner cookie proves
cooperative instance identity, not a security boundary against in-process code.
Until an ownership-capable API exists, document v1's limitation and prevent broad
SDK migration from presenting this helper as a verified atomic operation.
Test foreign module/page claims, partial failure, retry, and rejected rollback.

### M02 — factory occupancy and movie invocation are weaker than readiness

**Observed:** menu registration succeeds if any factory occupies the engine menu
name. The host then marks its API ready. The SWF calls `ready(1)` but native Ready
does not validate the argument; AS ignores incompatible models, while native
acknowledges publication based on successful `applyModel` invocation.

Evidence: [factory registration](../src/ui/ControlPanelMenu.cpp#L454),
[host readiness](../src/NativeMenuProbe.cpp#L74),
[native Ready](../src/scaleform/ScaleformMenuBridge.cpp#L98),
[publication](../src/scaleform/ScaleformMenuBridge.cpp#L1159),
[AS validation](../interface/src/AbsoluteControlPanelMenu.as#L68).

**Gate (M1):** distinguish core-ready, registry-ready, factory-owned, movie-ready,
and protocol-compatible. Qualify the loaded factory and negotiate a supported
bridge version/capability set. Validate accepted generations; package provenance
does not prove which SWF a mod manager selected at runtime. Test foreign factory
preclaim, old/new DLL-SWF pairs and missing SWF, while retaining declared v1 support.

### M03 — OSF exclusion currently covers only one opening order

**Observed:** AC checks `OSFUI_FocusMenu` at hotkey/provider open. OSF's local
`menu.open` handler checks its own manifest/input readiness, and its focus menu
watchdog reopens the focus menu when desired capture persists.

Evidence: [AC open checks](../src/ui/ControlPanelMenu.cpp#L467),
[peer predicate](../src/interop/OSFUICompatibility.cpp#L16),
[OSF open](../../_osf-ui-reference/src/Bridge/RuntimeBridge.cpp#L231),
[OSF reconciliation](../../_osf-ui-reference/src/Input/RuntimeInput.cpp#L337).

**Gate (M1):** test both opening orders, capture in progress, and external hide
with a dirty draft. AC can yield/suspend its own presentation under a documented
draft policy; it must not close another host or claim universal focus ownership.
A bilateral presentation lease needs an agreed peer API. Source-only one-way
checks cannot be described as mutual exclusion.

### M04 — a later list rebuild can erase AC's PauseMenu row

**Observed:** AC preserves existing rows and uses owner marker plus action token,
then marks the insertion cycle completed after success/already-present. The
visible source label remains `CONTROL PANEL`; `ABSOLUTE CONTROL` is a target.

Evidence: [row identity](../include/ui/PauseEntryPolicy.h#L11),
[cycle completion](../src/ui/PauseMenuIntegration.cpp#L442).

**Gate (M1):** test a second list builder running after AC, repeated opens, action
collision and selection retention. Research a generation-aware/shared append
seam. A bounded verification fallback must stop rather than fight a peer's
rebuild loop. Rename the visible label independently of stable engine/export IDs.

## Cross-cutting contract decisions

- **Saved configuration:** preserve defaults/imports/custom ownership through
  migration. HOTAS's build deliberately replaces its mod-owned default INI
  ([recipe](../../AbsoluteHOTAS/xmake.lua#L75)); user custom files must remain
  untouched by packaging. A live core route cache has one authoritative saved
  owner: choose whether routes are core-owned records or subscriber-supplied
  configuration. Never dual-write the same binding into both stores without a
  versioned migration. Drafts need revision/conflict rules for concurrent frontends.
- **Shared bindings:** one channel can intentionally drive several actions or
  non-overlapping contexts. A duplicate source is not automatically a conflict;
  report overlapping resource claims and let domain policy choose reassignment.
- **Startup matrix:** old HOTAS + new core retains HOTAS device authority unless
  that HOTAS version can explicitly relinquish it. New HOTAS + absent core keeps
  its declared legacy path during the compatibility window. New core + migrated
  HOTAS negotiates once before acquisition. Unsupported pairs disable only the
  affected capability with a clear diagnostic. Late availability cannot silently
  change the selected owner during a running session.
- **Provider retirement:** current runtime DLL unloading is explicitly unsupported
  ([lifetime contract](../docs/BRIDGE-PROTOCOL-V1.md#L143)). Keep that restriction;
  save/load and menu recreation are separate test cases. Future hot unload needs
  retire/drain/acknowledge and callback epoch invalidation.
- **Output phase:** “one action frame” means coherent resolved intent. Hook
  invocation counts, game-thread execution and per-object memory lifetime still
  belong to each verified backend. Do not promise one physical write per frame.
- **Future Steam/controller support:** specify channel types and provider alias
  identity now; research acquisition separately. A physical device exposed by two
  providers must not double-route the same gesture. No current audit establishes
  a working Steam Input interception backend or complete FPS/rover action coverage.

## Recommended first implementation batch

1. Add collision, readiness, and lifecycle fixtures for M01/M02 and startup A01/A02.
2. Harden menu registration/protocol and qualify M03/M04 coexistence scenarios.
3. Draft the headless core and authority negotiation contract with A01/A02/A07.
4. Capture device trace fixtures for A03/A05; define the legacy facade mapping.
5. Migrate capture entry points together, preserving A04/A06 cleanup semantics.

Standard subscriber/grid work may proceed alongside these batches. Production
input ownership changes wait for their specific gates; mapping further offsets
is an independent research track.
