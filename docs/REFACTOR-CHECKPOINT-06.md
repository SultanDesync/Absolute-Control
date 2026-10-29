# Checkpoint 06: native device lifecycle notifications

Date: 2026-09-29 (America/Vancouver).

This continues B1 of the [native-backend-first plan](SUITE-REFACTOR-TRANSITION-PLAN.md)
after the [subscriber menu checkpoint](REFACTOR-CHECKPOINT-05.md). The production
device store now has a bounded native change stream and coherent inventory
snapshots. This is an internal foundation for AC's event bus, not the universal
provider/subscriber ABI or completion of gameplay migration.

## Implemented

`NativeBackend().Devices().Snapshot()` copies availability, a store session,
the last notification cursor, and all device/channel descriptors under the same
lock. It works before startup and after shutdown; a stopped snapshot has no
devices. Returned data belongs to the reader and does not change underneath it.

`CopyChanges(after, changes)` returns retained notifications strictly after the
reader's cursor. The store keeps 256 records in a fixed ring. Each record has a
monotonic sequence, session, and `Started`, `InventoryChanged`, or `Stopped`
kind. Readers keep their own cursors; there are no registered callbacks, retained
consumer pointers, hardware calls from reads, or menu dependencies.

Successful inventory changes notify for connection/validity transitions,
provider/enumeration/connection generations, device replacement or removal,
channel descriptors, and display metadata. Repeated identical inventories do
not notify. Sample publication remains exclusively on the copied frame data
plane; 1,000 sample publications produce no lifecycle notifications. Rejected
inventories publish neither partial state nor notifications. Metadata-only
changes preserve compatible sample history.

Start/Stop are idempotent. Each new store lifetime increments its session;
notification cursors and retained history survive Stop/Start. This avoids
mistaking a prior lifetime's cursor for current activity. Provider polling
failure clears the store and publishes Stop after provider cleanup. Failure
before the store starts creates no fictitious device-service lifetime.

The snapshot's `ready` means the data store accepts publication. It does not
advertise capture, routing, context, output capabilities, or even completion of
the backend's first poll. `BackendRuntime::Lifecycle()` and `Failure()` retain
their separate worker-health meaning.

## Reader reconciliation

1. Copy a snapshot and adopt its inventory, availability, session and cursor.
2. Read changes after that cursor. If there are changes, copy a new snapshot
   and adopt its cursor; notifications signal reconciliation rather than replay
   of intermediate inventories.
3. If `dropped` is nonzero, events were overwritten before this reader consumed
   them. It counts exactly the missing events after the requested cursor.
   Reconcile from a new snapshot, rather than guessing the missing transitions.

An empty successful read returns the current cursor. A future cursor is rejected
as `InvalidArgument` without altering the caller's result. Inventories and
notifications commit under one lock, so copying a snapshot and then reading
after its cursor cannot miss a subsequently committed change. The journal is
bounded invalidation history, not a reliable command queue; it must never carry
press edges, output commands or actions requiring acknowledgement.

## Validation

- Both product and ResearchDev plugins built with MSVC in `releasedbg` and `release`.
- All 19 native xmake tests passed, including the new device lifecycle suite.
- Fixtures cover initial/stopped snapshots, copied descriptor isolation,
  reconnect and same-count replacement, metadata edits, sample separation,
  rejected publications, exact overflow reporting and concurrent readers.
- Production worker fixtures exercise no-menu startup, provider failure,
  restart and cleanup; notifications come from the real store used by the worker.
- The maintained `tools/process/validate-current.ps1` workflow passed all 11
  automated gates, including the 19 native tests, nine SDK generator tests,
  generated-code/consumer fixtures, catalog, SWF provenance, artifact manifests
  and package integrity. The report is
  `artifacts/process/refactor-checkpoint-06-validation.json`.
- `git diff --check` passed.

No DLL deployment, game launch or hardware qualification was performed.

## Next increment

Use these snapshots/notifications for one AC-owned capture state machine with
owner/session generations, release-to-arm, cancellation, timeout, disconnect
and stale-sample invalidation. Preserve the unified keyboard/mouse/controller
recorder requirement; this checkpoint only supplies controller device evidence.
Native keyboard/mouse acquisition and suppression of self-generated input still
need their own implementation. Then connect a subscriber to tested native
services and remove its redundant input ownership when parity is demonstrated.

Capture/calibration/persistence, semantic context, action arbitration, output
backends and the public extension ABI remain unfinished. The existing nine
subscriber pages have the earlier loading evidence; this change does not add
transaction or gameplay qualification to that evidence.
