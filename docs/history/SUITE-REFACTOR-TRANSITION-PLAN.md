# Absolute suite refactor: native backend first

> **Current direction, 2026-09-18:** Build Absolute Control for the final desired
> architecture first, then adapt subscribers individually. This supersedes the
> transitional facade/shadow rollout and the proposed subscriber-first sequence.

The [archived transition proposal](SUITE-REFACTOR-TRANSITION-PLAN-ARCHIVE.md)
retains the original research, invariants and historical checkpoints. Its
compatibility-window, legacy fallback and intermediate facade requirements are
not implementation requirements for this build. The
[authority risk register](SUITE-REFACTOR-AUTHORITY-RISKS.md) remains useful source
research; its old gate names must be read against this revised plan.

## Target architecture

Absolute Control owns the headless runtime and presents its services through a
separate menu shell. It owns device providers, device/channel identity, native
binding capture, context evidence, native route/profile records, transforms,
action arbitration and generic output services. Gameplay subscribers own their
domain configuration, semantics and persistence, and register with AC directly.
One component writes each record. The menu edits drafts and issues commands.

The runtime lives inside the AC plugin initially. Its lifetime is independent
of menu configuration, native menu hooks, registration success and SWF presence.
Moving it into another DLL is unnecessary unless deployment requirements demand
that later. Device workers do not depend on gameplay injection hooks.

```text
AC device providers -> native copied frames -> routes + context + arbitration
                                                 |
subscriber domain intents/actions ---------------+
                                                 |
                                        resolved action frames
                                                 |
                                  AC output backends at verified hooks

subscriber page declarations -> AC menu -> commands -> owning service/module
```

## Build sequence

### B1 - Native runtime and device data plane

- Start AC runtime services before presentation initialization.
- Own DirectInput acquisition and polling directly, without loading or calling
  HOTAS. Keep provider-specific constants behind the acquisition boundary.
- Separate physical identity, channel identity, provider epoch, enumeration
  revision and connection epoch. Never persist runtime connection handles.
- Publish coherent copied frames with timestamps, sequences and edge counters;
  bound device counts and history. Reads never invoke provider code or acquire
  hardware. Slow readers can detect sequence gaps; snapshots do not promise
  arbitrary event ordering or edges hardware polling never observed.
- Handle disconnect/reconnect, identical devices and same-count replacements.
- Refuse an unsupported old-suite pairing rather than enabling competing pollers
  or selecting a legacy fallback. Only one AC host can run in a process.

Current implementation is recorded in [checkpoint 04](REFACTOR-CHECKPOINT-04.md).
The bounded native lifecycle stream and coherent inventory reconciliation are
recorded in [checkpoint 06](REFACTOR-CHECKPOINT-06.md); sample frames stay on the
separate data plane. The notification contract remains internal, not a public ABI.
Hardware and game qualification remain separate from mock and build validation.

### B2 - Native calibration, capture and configuration

- Own device calibration by stable physical/channel identity. Distinguish driver
  ranges from user calibration and profile transforms.
- Implement one capture state machine with owner/session generation, release to
  arm, timeout, cancellation and disconnect handling. Presentation only observes
  and controls the session; it does not poll devices.
- Preserve Zero's unified recorder: one binding control listens for any allowed
  keyboard, mouse or controller input in the same session. Subscribers declare
  accepted input kinds; users do not select separate recording modes.
- Define versioned AC-owned binding, route and profile records and persistence.
  Subscriber-specific domain settings remain owned by subscribers.
- Design profile inheritance, explicit unbinding, activation and switching
  directly. Switching invalidates affected routes and releases owned outputs.
- Support clean native setup and rebinding. Preserve old files. Optional one-time
  import can reuse unambiguous assignments later; it is not on the critical path
  and the new runtime must not interpret old HOTAS profile files.

### B3 - Context, routing and action authority

- Run the [game-state research program](GAME-STATE-RESEARCH.md) independently of
  output-route implementation. Qualify named menu, construction, dialogue,
  player/ship-role and lifecycle observations using community source research
  followed by controlled runtime evidence. Source declarations alone are not
  qualified flags. State observation can become ready before its action paths.
- Publish named context evidence with validity, source, priority and freshness;
  unknown remains distinct from false. Device providers do not decide gameplay
  policy. Observe game state through AC-owned, version-gated services.
- Separate player role, camera, UI state and action permission. Publish copied
  current-state snapshots plus generation-linked change events, with startup
  reconciliation and invalidation on load/revert or source loss. AC owns the
  built-in version-specific observation hooks, memory layouts and supported
  injection backends. Extension providers can supply additional observations
  and executors under the same service contracts; subscribers consume semantic
  state and retain gameplay policy.
- Register semantic action IDs, shapes, reducers, lease policy and release rules.
- Resolve a coherent action frame with source/context generations. Support
  simultaneous axes/buttons, layers, chords, dead zones, curves and rate limits.
- Make route/context loss, stale input and subscriber loss explicit revocations.
  Observers, game-thread executors and output writers have separate readiness.

### B4 - Output backends and subscriber-facing SDK

- Own game-binding resolution as well as input acquisition. An action routed to
  SendInput resolves its semantic game action plus context to the current bound
  keyboard/mouse output in AC. Subscribers must not independently read the game
  binding file or translate actions into keys. Explicit raw-key outputs remain
  distinct from semantic actions; direct native routes do not pass through key
  resolution or silently fall back to SendInput.
- Resolve the base game bindings together with custom overrides, primary and
  alternate slots, explicit unbinding and keyboard layout. Distinguish missing
  overrides from malformed/unknown data. Do not turn an explicitly unbound or
  unresolvable action into a guessed default key. Expose resolution status,
  provenance and generation for diagnostics and capability checks.
- Load/reload binding data outside polling and rendering, publishing a coherent
  versioned snapshot. On a game rebind or output-method change, invalidate the
  affected routes and release the exact outputs acquired under the old snapshot
  before admitting new output. Key-up must use the acquired output identity,
  never a newly resolved key. Define re-arming behavior for still-held sources.
- Own synthetic holds across actions sharing a physical key/button. Release only
  the last owned hold; prevent self-generated input feedback.
- Integrate proven native injection at verified hooks/phases, with game-version,
  object-lifetime and capability checks. Unsupported actions remain unavailable.
- Keep cleanup generation-bound so a revoked writer cannot erase a successor.
  Repeated hook invocation must not replay edges.
- Publish a capability/version/lifecycle-checked native subscriber SDK after
  contracts have executable tests. No new HOTAS runtime facades or fallback
  clients are required. Retain existing menu identifiers where practical; a
  runtime redesign does not itself require changing presentation identity.

Implementation reference: HOTAS currently parses `ControlMap_Custom.txt` in
`ControlMapReader` and resolves outputs in `ShipOutput`. Its retained SendInput
path combines catalog defaults, custom bindings and optional manual overrides;
direct native and fixed-context routes have different rules. The existing parser
documents that base-only alternate bindings are invisible to its override-only
view. Reuse verified format knowledge and tests, not that limitation or its
unknown-token fallback policy. This service belongs to AC even while HOTAS's
subscriber/profile redesign remains deferred.

Qualification includes a game rebind, mouse-bound action, primary unbound with a
usable alternate, fully unbound action, malformed data, layout change, context
separation, and rebinding while an output is held. AP keyboard bindings must be
qualified through the new input/action service too; keyboard support cannot be
assumed to bypass the old InputBus dependency.

### Backend extension providers

Modders must be able to contribute new state/action providers through a PR or
ship an independent SFSE plugin without waiting for an AC release. The public
provider API is a supported architecture boundary. Built-in providers should
use the same registration and lifecycle contracts, so upstreaming an extension
does not require rewriting its consumers.

- An installed provider registers with AC when both are ready. AC validates and
  enumerates its typed state, event and action descriptors. Consumers discover
  capabilities and declare required versions instead of assuming load order.
- Use stable namespaced identifiers, explicit schema/ABI versions and ownership
  tokens. Reject duplicate ownership rather than replacing another provider.
  Shared semantic contracts allow modules to coordinate without depending on
  each other's private memory layout or requiring AC to hard-code their names.
- Providers own their additional game-version checks, observation hooks and
  specialized executors. AC owns publication, subscription, scheduling contracts,
  routing, arbitration and shared output ownership. In-process plugins are not
  sandboxed; this API contract cannot prevent arbitrary code bypassing it.
- Missing, incompatible or failed providers expose unavailable capabilities.
  Invalidate their published state and revoke owned actions using the normal
  lifecycle rules. Independent plugin delivery does not imply runtime DLL hot
  unloading, which remains unsupported.
- Menu declarations are optional and separate from backend registration. A
  provider can work headlessly or supply both services and configuration pages.
- Ship a minimal standalone provider example, contract tests and documentation
  for discovery, snapshots/events, action execution and failure cleanup alongside
  the SDK. Adding a state or action supported by the contract must not require
  editing AC core source files or obtaining a centrally assigned identifier.

This is a planned capability, not an implemented extension ABI. Unknown value
shapes or execution requirements may still require an SDK/core revision. An
external provider also carries its own patch-maintenance responsibility; AC
centralizes the built-in suite's engine integration, not all future mod code.

### Deferred research: native DLL / Papyrus bridge

Reopen research into exposing AC's event bus to Papyrus mods after the native
contracts are established. A scripted mailbox ESM is a proposed transport,
not a selected or validated implementation. Compare it with native Papyrus
bindings and available event mechanisms before choosing a design; their current
Starfield support, scheduling behavior and measured throughput remain research
questions. No transport bandwidth or delivery guarantee is established yet.

The bridge should use the same namespaced state/action contracts as native
providers. Target state snapshots, selected change notifications and bounded
commands first. Keep device sample streams, per-frame control and precise output
timing on the native side. A bridge can ship as an optional extension without
making Papyrus or an ESM a dependency of AC's headless native runtime.

Research must distinguish replaceable state updates from events or commands
that require ordered delivery. Define bounded queues, subscription filters,
batching/coalescing, overflow reporting, sequence numbers and acknowledgements
where needed. Use serializable values and validated identifiers across the
boundary; never expose raw native pointers. Discover supported value types and
report unsupported operations explicitly.

Save/load and VM lifetime are central qualification cases: determine which
mailbox data persists, invalidate stale sessions, resynchronize current state,
and prevent a restored mailbox from replaying an old action. Test bidirectional
latency, bursts, stalled consumers, absent scripts/plugins, load/revert and
duplicate delivery before claiming reliable orchestration. A notification
delivery acknowledgement is distinct from successful completion of an action.

This is deferred exploration, not a prerequisite for native subscriber migration
and not a commitment to implementing the mailbox ESM specifically.

### B5 - Adapt subscribers one at a time

At the user's request, menu-facing adaptation has started while native services
remain incomplete. [Checkpoint 05](REFACTOR-CHECKPOINT-05.md) covers shared
registration and an in-game loading pass for all nine Zero, Power and Head
Tracking pages. This is an early presentation checkpoint, not completion of
their native runtime migration or a replacement compatibility phase.

Adapt Zero, Power and Head Tracking to the completed native services, then
redesign HOTAS's remaining gameplay/profile integration with a focused review.
Consult all modules' requirements during B1-B4, especially HOTAS's calibration,
simultaneous controls and profile switching; postponing adaptation must not mean
ignoring those requirements. No subscriber migration is a prerequisite for
starting AC's services, and unimplemented backend capabilities must not be
advertised as ready.

For each subscriber, remove generic polling/capture/output ownership as its AC
path becomes functional. Qualify controls, Apply/Discard, persistence, gameplay
without the menu, disconnects, context loss and shutdown. Validate the relevant
specialized hooks before removing their old implementation. Use coherent native
build cohorts, not partially interoperable public releases.

## Authority and release rules

1. One acquisition owner per provider, one suite capture session and one final
   arbiter per action/output resource.
2. No output without a ready capability, valid context and current ownership.
3. High-frequency samples stay out of lifecycle events and Scaleform.
4. Callback/context pointers cannot outlive their registration. Workers stop and
   drain before their owned provider objects are destroyed; runtime DLL hot
   unload remains unsupported.
5. Menu failure cannot disable healthy device/core services. Gameplay behavior
   cannot rely on a menu frame callback.
6. Preserve foreign PauseMenu rows and use explicit presentation ownership.
7. Save/load, focus loss, disconnect and profile changes have tested release
   behavior. Cleanup must not blindly zero game memory or restore stale state.
8. No automatic switch to old providers on failure. Unsupported mixed old/new
   combinations report an unavailable capability. Rollback is a coherent prior
   suite plus its preserved configuration, not runtime fallback.
9. Native profile/device redesign may require rebinding. Never guess an identity
   or silently reinterpret a saved POV token with conflicting dialects.

Before release, qualify clean setup, native upgrades, missing menu/SWF, missing
subscribers, unsupported old DLL combinations, hotplug/reconnect, identical
devices, frame stalls, simultaneous actions, held-output loss, profile switching,
version rejection and shutdown. Menu compatibility/transaction scenarios from
checkpoint 01 still apply. Automated tests do not establish in-game behavior.

## Evidence and immediate work

- [Checkpoint 01](REFACTOR-CHECKPOINT-01.md): registration/presentation hardening.
- [Checkpoint 02](REFACTOR-CHECKPOINT-02.md): executable authority model; a design
  fixture, not the production device worker.
- [Checkpoint 03](REFACTOR-CHECKPOINT-03.md): provider-neutral record draft and
  legacy-token evidence. Legacy codecs remain outside the production runtime.
- [Checkpoint 04](REFACTOR-CHECKPOINT-04.md): AC-owned device worker and data store.

Finish B1 device qualification and observability, then implement B2 capture and
native configuration on the same data plane. Build context, routes and output
services before adapting subscribers. This plan replaces the previously proposed
copied-HOTAS shadow adapter as the next implementation step.
