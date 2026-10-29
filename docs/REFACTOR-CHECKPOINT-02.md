# Refactor checkpoint 02: headless authority harness

2026-09-18. First internal R0/R1 executable model from the
[transition plan](SUITE-REFACTOR-TRANSITION-PLAN.md). This is a harness milestone;
neither R0 nor R1 is complete in the shipping plugin.

## Implemented boundary

`include/runtime/HeadlessCore.h` and `src/runtime/HeadlessCore.cpp` implement a
standard C++ registry and exclusive-resource ledger. Only `headless_core_test`
links the implementation. Neither plugin target starts it or acquires resources
through it. Stable menu and HOTAS ABIs remain untouched.

Services register with an identity, exact internal contract version, capability
bits, role and fresh instance token. Discovery distinguishes absent,
incompatible and present-but-unready. Registry readiness does not imply that a
device provider, observer, executor or writer is ready. These should register as
separate services when their health differs. Registration can precede or follow
core startup; consumers retry discovery or inspect the lifecycle journal.

Before startup, the caller supplies an explicit owner selection for each named
exclusive resource. The selection is immutable for that run. A shadow service
cannot be selected or acquire a lease. Degradation, expiry and missing readiness
never authorize a replacement or fallback. This models old HOTAS retaining
authority when a new core cannot negotiate its retirement. It does not yet
implement suite discovery, a readiness barrier or a production authority plan.

Leases have unique tokens and deadlines. Revocation keeps the resource reserved
until the selected backend acknowledges its own release operation. Shutdown
waits in `Stopping` for all acknowledgements; restart cannot bypass cleanup.
An acknowledgement for an old lease cannot clear a successor's claim. A mock
held-output writer verifies that changing a registry state does not itself
release an output. There are no game-memory writes, `SendInput`, device handles
or provider callbacks in the core.

## Internal operating rules

- **Command plane:** registration, startup selection, health updates and lease
  commands run synchronously on one caller-owned thread. The caller serializes
  every operation, including reads; this class is not thread-safe. No callbacks
  are invoked or retained, so callback-duration rules are not yet applicable.
- **Event plane:** the copied journal contains registration, owner selection,
  health, grant, renewal, revocation, neutralization and lifecycle records with
  monotonic event sequence and caller time. It retains 256 records and explicitly
  reports how many were dropped. Drain/export integration is pending; this is
  not a complete session log after truncation.
- **Data plane:** service and authority snapshots are copies with no borrowed
  pointers. At most 64 services and 128 resources are retained, with identifiers
  bounded to 127 bytes. Device sample frames/history are deferred to R2 and will
  not use the lifecycle journal or Scaleform.
- **Clock:** callers invoke `AdvanceTo` before processing commands at a new time.
  Time and lease durations share one caller-selected monotonic unit. Backward
  time, zero duration and overflowing deadlines are rejected. Expiry occurs at
  the deadline, and cannot be undone by renewal. No wall-clock worker runs here.
- **Lifetime:** tokens are scoped to one core object and never reused across its
  restarts. The object cannot be copied. Selected services cannot retire until
  shutdown completes. Stop before destruction and complete backend cleanup;
  destruction cannot perform a backend release. Runtime DLL unload is still
  unsupported.

## Validation

The executable checks all six provider/subscriber/menu registration orders,
late discovery, no-menu startup, menu failure while devices remain available,
version/capability rejection, duplicate identities, shadow restrictions,
exclusive capture, startup owner selection, blocked fallback, health-loss
revocation, cleanup acknowledgement, delayed shutdown, stale tokens across
restart, deadline renewal/expiry, bounded storage and copied snapshots.

Run from the repository root:

```powershell
xmake build -y headless_core_test
xmake run headless_core_test
xmake test 'headless_core_test/*'
```

Checks use an always-active failure macro, including in optimized builds.
Current validation: MSVC `releasedbg` build and executable passed, together with
the existing native-decomposition and release/research-boundary tests (3/3
selected xmake tests). This is mock evidence only; there is no claim of actual
hardware, SWF or in-game coexistence qualification.

## Next gates

Wire production diagnostic events only after inventorying existing owner and
connection generations. Decide core deployment and thread ownership before
activating it. Define normalized frame storage and legacy token fixtures next,
preserving existing HOTAS calibration and edge-counter semantics. The M1 OSF,
late PauseMenu mutation and transaction runtime cases from checkpoint 01 remain
open and independent of this harness. Resource-specific reducers, multi-owner
physical output holds and native neutralization require R5/R6 contracts; this
exclusive-resource model is not a replacement for those policies.
