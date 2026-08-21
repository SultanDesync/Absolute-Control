# Scalability, transactions, and menu teardown

> **Status:** Core lazy catalog, scoped refresh, transaction lease, and idempotent teardown are
> implemented and build-verified. Runtime budgets, dirty decision UX, and fault injection remain
> gates before the public SDK freezes.

This contract keeps the Control Panel a presentation and orchestration host. A subscriber remains
fully functional without it: the subscriber owns its runtime behavior, draft values, validation,
Apply/Cancel implementation, persistence, INI compatibility, and any legacy frontend. The host
stores only route, focus, capture, and transaction ownership state and invokes subscriber callbacks.

## Scalability answer

The registry is bounded at 512 modules, 2,048 pages, 32 pages per module, 128 controls per page,
512 controls per module, and 32,768 controls total. Those are admission limits, not a promise that
the full graph is suitable for one UI payload. The implemented view model therefore:

- publish a compact, virtualized module directory containing stable ID, title, availability, and
  revision metadata, with only visible sidebar rows rendered;
- publish page metadata only for the active module;
- read and publish control values only for the active page, while descriptors remain host-owned
  bounded copies;
- scope provider refreshes to their module/page and ignore inactive value refreshes until that
  route becomes active;
- renders only the fixed visible sidebar/tab/row windows, though each accepted replacement still
  redraws those visible objects;
- keep one latest deferred publication per open session, applied at a movie-frame boundary; and
- retain explicit per-module, per-page, per-control, string, and total-memory admission limits.

The automated scalability fixture registers 512 synthetic modules, three pages per module, and 16
controls per page: 1,536 pages and 24,576 controls. Its snapshot contains 512 lightweight module
summaries, three active-module page records, 16 active-page controls, and exactly 16 provider reads.
A separate concurrent stress test covers registration, snapshotting, reentrant refresh, scoped
refresh, and retryable unregister. What remains is in-game measurement of UI-frame time, bridge
allocation/payload cost, and memory at that envelope; 512 summary objects are still serialized on
an accepted replacement model.

This is an internal bridge evolution. It does not move gameplay or persistence into the host and
does not require a subscriber to abandon its headless or legacy configuration paths.

## One transaction owner

One open menu session permits at most one dirty `(moduleId, pageId)` transaction across all
subscribers. The provider owns the draft; the host owns only the transaction lease and dirty-page
identity. Multiple hidden dirty drafts were rejected because they make module switching,
unregistration, failure recovery, and Close behavior unpredictable.

Clean navigation remains immediate. A request to leave the dirty page becomes a guarded route
intent rather than an error-only dead end:

```text
Clean
  -> first successful write -> Dirty(page A)
  -> request page/module B   -> DecisionPending(page A, route B)
       -> Apply & Continue   -> provider Apply -> revalidate route B -> Clean on B
       -> Discard & Continue -> provider Cancel -> revalidate route B -> Clean on B
       -> Stay               -> Dirty(page A)
```

The same rule applies whether the destination is another page in the same subscriber or another
subscriber. The modal is host-owned and generic; Apply and Cancel still call the dirty provider.
The destination is stored as stable IDs and revalidated against the latest registry revision after
resolution. If it disappeared, the menu remains on the nearest valid route and explains why.

### Close policy

| State | User-requested Close/Back | External hide or destruction |
|---|---|---|
| Clean | Close immediately. | Tear down immediately. |
| Binding capture | Escape cancels capture and stays in the menu; another Close request may then proceed. | Cancel capture, release input ownership, then tear down. |
| Dirty | Show **Apply & Close**, **Discard & Close**, and **Stay**. Apply failure or validation failure stays open with the error. | A prompt is impossible, so call provider Cancel exactly once while its lease is held, record the abnormal rollback, and tear down. |
| Applying/discarding | Block navigation and duplicate commands until completion. | Invalidate the UI session first; complete the bounded callback/operation policy without publishing into the dead movie. |

“Apply” means the subscriber's Apply contract, including its own atomic persistence and read-back.
The host never writes the subscriber's INI. In ABI v1 Cancel has no result value, so the host can
only treat callback completion as rollback completion; a result-bearing teardown contract is an
SDK-freeze decision.

Back-stack ownership is also host-owned. A clean user Back remembers where the panel was invoked:
PauseMenu origin normally reveals the still-resident underlay after the displayed panel completes
native Hide; if another plugin removed that underlay, the host queues one recovery Show. An opt-in
standalone-hotkey origin returns to gameplay. The first queued Show origin is claimed by that session and cannot be
rewritten by a duplicate overlapping Show. External, watchdog, and fail-closed hides never
synthesize PauseMenu, so teardown cannot accidentally reopen UI after a fault. The panel's native
audio-mode lease is independent and is released idempotently for every Hide/destruction path.

Input capture and a dirty transaction are distinct. Navigation and Close cannot open a dirty
decision modal while capture owns input; capture is resolved first. Future asynchronous Apply is
represented as one explicit operation state. The host does not run concurrent provider
transactions or keep hundreds of pending drafts.

## Publication boundary

Provider callbacks and command validation run synchronously on the native UI/game path, but model
publication must not. A pointer or keyboard command may destroy and recreate Scaleform display
objects when its replacement model is applied. Doing that before the original event dispatch
unwinds can leave Scaleform holding a dead event target.

The bridge therefore keeps only the newest pending model and publishes it on the next
`ENTER_FRAME` acknowledgement. Refreshes coalesce into that pending model. Initial movie setup may
publish synchronously because no user input target exists yet. A successful Close publishes no
replacement tree: it enters terminal closing state, clears pending work, and queues Hide.

## Teardown order

Normal and abnormal paths share these invariants:

1. Mark the bridge closing so late commands are consumed without provider mutation.
2. Invalidate/drop deferred movie publications.
3. Cancel binding capture and clear host input-capture ownership.
4. On normal Close, resolve dirty state through the user decision before queuing Hide.
5. On external destruction, call provider Cancel once while the transaction lease still prevents
   unregistration, then release the lease.
6. Perform the same idempotent teardown from Hide and destruction; do not construct a replacement
   model while the movie is disappearing.
7. Release movie/bridge references before engine-owned Scaleform objects disappear.
8. Never call into a subscriber after its callback/transaction lease is released.

`unregisterModule` remains nonblocking and returns retryable `Rejected` while a callback,
transaction, or terminal operation owns a lease. SFSE plugin DLL unload remains unsupported; menu
teardown and module unregistration are supported and must not be confused with process DLL unload.

## Delivery gates

The Absolute Power integration may continue against ABI v1 because Power retains all headless
behavior and the current capacities are sufficient for its vertical slice. Before SDK release:

- implement and test the Apply/Discard/Stay route and close modal with mouse, keyboard, and
  controller semantics;
- measure the implemented lazy bridge at the 512-module envelope in game and set release budgets;
- add engine-driven external-hide tests for dirty and capture states;
- decide whether result-bearing Cancel and asynchronous Apply require provider ABI v2; and
- record repeated runtime interaction cycles with no new crash dumps and exact artifact hashes.
