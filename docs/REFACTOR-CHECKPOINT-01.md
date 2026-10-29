# Refactor checkpoint 01: registration and presentation readiness

2026-09-18. First implementation slice from M1/R7 of the
[transition plan](SUITE-REFACTOR-TRANSITION-PLAN.md).

## Changes

- SubscriberKit preflights matching module/page IDs and unique page IDs before
  mutation. A duplicate module cannot acquire pages or trigger unregister.
  Duplicate pages are failures. Detailed registration results expose attempted
  rollback and whether partial registration may remain.
- Zero rejects foreign module/page duplicates and rejected rollback terminally.
  It returns `Rejected` to discovery so fallback frontend selection cannot
  mistake a collision for a successful registration. Its own successful
  registration remains idempotent; initial `NotReady` remains retryable.
- The native host verifies its factory pointer both before and after engine
  registration. A foreign factory occupying the stable menu name is rejected.
- The bridge validates the exact `ready(1)` payload and waits for the existing
  synchronous `modelApplied(generation)` callback. Invocation alone does not
  acknowledge a published model. Initial heartbeat zero, repeated accepted
  generation heartbeats and publication acknowledgements have distinct rules.
- Failed movie/root/handshake initialization cannot activate Show bookkeeping.
  Protocol rejection queues Hide once, disarms input and stops subsequent live
  polling; normal Hide owns transaction rollback.
- The visible Pause Menu row is `ABSOLUTE CONTROL`. Existing engine menu, export,
  action and row-owner identities remain compatible.

## Boundaries and remaining work

Stable v1 registration still lacks owner tokens and atomic bundle installation.
The guards prevent ordinary duplicate-as-success errors but cannot provide
ownership-safe rollback against arbitrary concurrent foreign registration under
the same ID. Host-owned registration handles/bundles remain a later contract.
Other subscribers still need the same collision review before their migration.

The current SWF already provides the required v1 Ready and synchronous
acknowledgement, so this checkpoint does not change its protocol or source.
Build provenance remains required for the deployed DLL/SWF pair. Runtime build-ID
negotiation, continuous OSF overlap handling and late PauseMenu mutation handling
remain open; this checkpoint does not claim full OSF coexistence.

Device polling, capture ownership, action routing and injection have not moved.

## Validation

- Maintained Control validator: release/ResearchDev build, native tests including
  executable protocol-state negative cases, SDK generator/consumer fixtures,
  catalogue, SWF provenance and package validation.
- Zero: release build and all three CTest tests; debug subscriber test exercises
  foreign duplicates, retry, rollback rejection and frontend fallback selection.
- Source integration guards check that the tested protocol policy is used by the
  real bridge and that factory/Show gates remain wired in.
- In-game and UX qualification: pending, using Control + Zero + OSF first.

Automated result: Control's maintained validator passed (14 native tests,
9 SDK generator tests, consumer fixtures and artifact/package gates); Zero's
release CTest suite passed 3/3. The validated release ResearchDev DLL/SWF pair
and Zero DLL were staged with hash-verified backups. No game run or MO2 profile
change was performed. Standalone must be disabled and Research enabled before
using the scripted startup runner; never enable both hosts together.

The ignored checkpoint-local runner manifest restores the existing startup/load
sequence and retains the game for manual qualification. It deliberately uses
`skipBuild: false`: the runner currently deploys its default `releasedbg`
configuration, whereas the pre-staged automated-validation artifacts are
`release`. A runner rebuild is a separate artifact requiring its own runtime
evidence, not a qualification of the pre-staged binary.

The runtime pass must open through the real Pause Menu row, exercise Apply and
Discard, close/reopen, and test OSF opening both before and after AC. The startup
runner's direct research-open success is only initial handshake evidence, not
proof of Pause Menu or two-host compatibility. Restore HOTAS, Power and Head
Tracking for the later multi-subscriber and input-authority checkpoints.

### First supervised runtime attempt

The cold-start runner built and deployed the `releasedbg` ResearchDev pair, then
stopped at the first general input acknowledgement deadline. Evidence shows
mailbox registration and successful key release at that boundary: title advance
was available before the normal runtime mailbox. The runner now explicitly waits
for mailbox registration before issuing its first navigation command. It also
loads the Windows PowerShell utility module explicitly, treats a zero sentinel
threshold as disabled rather than visual success, and preserves evidence before
returning a manual-ready session. These script changes pass parsing/diff checks;
a fresh cold-start run is still required to validate the timing fix.

The retained session was continued manually. Evidence confirms activation from
the owned PauseMenu row, retention of the PauseMenu underlay, movie load, Ready,
Show, and accepted model publication with two modules. This is partial runtime
evidence, not completed transaction/UX qualification.

The user observed no OSF entry. The deployed OSF settings describe `MOD MENUS`
(different from the older reference audit's `MOD SETTINGS`) and enable it by
default. OSF DLL presence is confirmed; initialization/entry operation is not.
OSF's standalone baseline and two-host coexistence therefore remain unqualified.
