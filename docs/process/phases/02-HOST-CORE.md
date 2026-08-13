# Builder phase 02: generic host model and transactions

Work only in the supplied SLOP host worktree. Do not edit ActionScript or NativeMenuProbe in this
phase. The subscriber worktree must remain unchanged.

## Required internal seam

Extend `MenuApiHost` with `Pages()`, which copies the current page vector while holding the
registry mutex and releases the mutex before returning. Provider callbacks must never execute
while that mutex is held.

Add a renderer-neutral `MenuSession` with these public concepts:

- `Model`: schema version, registry revision, active module/page/control IDs, page dirty state,
  bounded error, and ordered pages/controls with copied descriptors, typed values, and read status;
- `Command`: schema version, command kind, module/page/control identity, and one `ValueV1`;
- `Session::Snapshot()`: enumerates every registered page and reads controls without a registry
  lock; one failed callback marks only that control unavailable;
- `Session::Dispatch(command)`: implements select-page, select-control, write, invoke, apply,
  cancel, and close using [Bridge Protocol v1](../../BRIDGE-PROTOCOL-V1.md).

Use `(moduleId,pageId)` dirty ownership. A successful first write makes the page dirty. Refuse
every route—including select-control, write, invoke, apply, and cancel—that addresses a different
page while one page is dirty. Apply clears dirty state only after a successful provider callback.
Cancel and dirty Close call the provider rollback before clearing; if a dirty page has no cancel
callback, Close returns an error and preserves dirty state.
Validate identity, kind, finite numerics, and descriptor ranges before any provider callback.
Action invocation requires Action kind. Action availability depends on `invokeAction` and does not
call `readValue`. Provider reads must return the value kind required by the descriptor or the row
is unavailable. ButtonBinding uses a terminated String value. All errors are bounded and
recoverable.

## Mechanical proof

Keep the synthetic module as a headless fixture. Expand host tests to cover at least two pages,
mixed values, copied descriptors, failed reads, unknown IDs, wrong value kinds, non-finite and
out-of-range writes, action routing, apply, cancel, dirty close, navigation refusal, refresh, and
unregister revision behavior. Explicitly test dirty-page bypass attempts through select-control,
direct writes, apply, and cancel; wrong-kind reads; Action availability; ButtonBinding writes; and
dirty Close without rollback. Run the supplied host build wrapper. Write `phase-02-result.json` at
the run root with `status`, `commands`, `changedFiles`, and `blockers`. Do not commit or edit the
subscriber, NativeMenuProbe, or ActionScript.
