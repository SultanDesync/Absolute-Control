# Absolute Control Panel native/ActionScript bridge protocol v1

> **Status:** Current internal protocol, not the cross-DLL provider ABI and not frozen. Product
> ActionScript uses generation-aware commands; a ten-argument generation-free form remains for
> internal prototype compatibility.

## Native-to-ActionScript model

Native calls `applyModel(model)` with a bounded object graph:

```text
model = {
  schemaVersion: 1,
  generation: Number,
  revision: Number,
  activePage: int,
  selectedControl: int,
  focusRegion: uint,
  focusedAction: uint,
  dirty: Boolean,
  bindingCaptureActive: Boolean,
  textCaptureActive: Boolean,
  captureModuleId: String,
  capturePageId: String,
  captureControlId: String,
  error: String,
  modules: [ module ... ],
  pages: [ page ... ]
}

module = {
  moduleId: String,
  moduleTitle: String,
  pageId: String  // first registered page, used as the module navigation target
}

page = {
  moduleId: String,
  moduleTitle: String,
  pageId: String,
  title: String,
  description: String,
  controls: [ control ... ]
}

control = {
  controlId: String,
  kind: uint,
  flags: uint,
  label: String,
  description: String,
  minimum: Number,
  maximum: Number,
  step: Number,
  available: Boolean,
  valueKind: uint,
  booleanValue: Boolean,
  integerValue: Number,
  floatValue: Number,
  stringValue: String,
  error: String
}
```

`kind`, `flags`, and `valueKind` use `AbsoluteControlPanelAPI.h` values (the legacy header aliases
them). Only the field selected by `valueKind` is authoritative. Integer/generation values crossing
Scaleform must be exactly representable as `Number`.

Registry admission is bounded at 512 modules, 2,048 pages, 32 pages per module, 128 controls per
page, 512 controls per module, and 32,768 controls total, plus the public fixed-capacity strings.
Each model contains the compact module directory, page metadata only for the active module, and
controls/values only for the active page. Registration returns `CapacityExceeded` rather than
admitting a graph outside those bounds. A provider read failure marks only that control unavailable.

`generation` increments for each session snapshot/publication attempt. The bridge acknowledges a
generation only after `applyModel` succeeds, and commands are compared with that visible generation
rather than a newer deferred snapshot. `revision` describes registry/refresh changes. ActionScript calls
`modelApplied(generation)` after rebuilding and every frame as a UI-thread publication/refresh
heartbeat. Native stays idle when no command model or refresh is pending.

## ActionScript-to-native commands

The current flat function is:

```text
dispatch(
  schemaVersion:uint,
  command:String,
  moduleId:String,
  pageId:String,
  controlId:String,
  valueKind:uint,
  booleanValue:Boolean,
  integerValue:Number,
  floatValue:Number,
  stringValue:String,
  expectedGeneration:Number)
```

Current ActionScript always supplies all eleven arguments and echoes `model.generation`.
Native also accepts the original ten-argument form; it sets generation to zero, the explicit
compatibility opt-out. Unknown versions/commands/identity, non-finite or inexact numbers, stale
generations, mismatched kinds, and range violations are rejected before provider mutation. A new
model carries the bounded error so the UI can recover.

| Command | Identity/value | Behavior |
|---|---|---|
| `selectPage` | module/page | Select page and publish. |
| `selectControl` | module/page/control | Update selection and publish. |
| `write` | module/page/control + typed value | Validate, call leased `writeDraft`, pin transaction, mark page dirty, publish. |
| `invoke` | module/page/control | Require Action. `kControlMutatesDraft` pins the page transaction and marks success dirty. `kControlAppliesDraftBeforeInvoke` applies an existing draft first, suppresses invocation on apply failure, and invokes only after successful persistence. Other actions invoke immediately. Publish. |
| `beginBindingCapture` | module/page/control | Require writable InputBinding/device flags, enter capture, publish. |
| `beginTextCapture` | module/page/control | Require writable TextInput, read its opening string, enter bounded native text capture, and publish. Enter writes the edited string through the ordinary draft transaction; Escape cancels capture without a write. |
| `apply` | dirty module/page | Call `apply`; on success release transaction/dirty state and publish. Clean calls are rejected without a provider callback. |
| `cancel` | dirty module/page | Call `cancel`, release transaction/dirty state, reread, and publish. Clean calls are rejected without a provider callback. |
| `close` | active session | Clean Close requests Hide. Dirty Close publishes the guarded decision and retains the provider transaction. |
| `dirtyApply` | pending dirty decision | Apply the pinned provider draft, then complete the requested route/Close. Failure retains the draft and decision. |
| `dirtyDiscard` | pending dirty decision | Cancel the provider draft, then complete the requested route/Close. |
| `dirtyStay` | pending dirty decision | Dismiss the decision, preserve the draft, and restore the previously published route/focus. |

## Refresh and slider cadence

`requestRefresh(moduleId,pageId)` validates that the page exists, advances registry and refresh
revisions, and returns. The open bridge consumes the latest revision on `modelApplied`; inactive
page-only refreshes do not rebuild the active view, while registry mutations wake it. Commands and
multiple relevant refreshes before the next frame coalesce into one replacement model. Dynamic
replacement publication never occurs inside the pointer/keyboard command callback. Opening a new
menu reads current values and initializes its cursor, so it does not need to replay old wakeups.

During pointer drag, `SliderWriteCoordinator` stores only the latest pointer-derived value and its
generation/page/control identity. `ENTER_FRAME` flushes at most one provider write per SWF frame.
A replacement model with a different generation clears pending drag/write state. Native also
rejects any stale command that crosses this ActionScript guard.

## Transaction and lifetime state

Dirty state is one `(moduleId,pageId)` transaction. Navigation to another page or Close opens one
guarded Apply/Discard/Stay decision; unrelated commands remain rejected until it is resolved.
Callback leases run outside host locks. Unregister returns retryable `Rejected`
while any callback or transaction is active; it never blocks the UI thread waiting for provider
code. Runtime DLL unloading is unsupported.

Normal Close now presents the guarded dirty decision documented in
`SCALABILITY-TRANSACTIONS-AND-TEARDOWN.md`. A successfully resolved Close enters a terminal bridge state,
drops deferred publications, and queues Hide without applying another display tree. If a menu is
externally hidden or destroyed without the normal close command, the idempotent Hide/destructor
teardown performs one best-effort provider rollback while the unregister lease is still held, then
releases the lease and clears dirty/capture state without building another movie model. The v1
`CancelCallback` cannot report rollback failure.

## Other bridge functions

- `ready(1)` publishes the initial model and initializes the refresh cursor.
- `close()` requests session close.
- `focus(region,actionIndex)` synchronizes ActionScript focus used by native keyboard routing.
- `modelApplied(generation)` acknowledges a model and polls refresh.
- `handlePointerDown/Move/Up`, `handlePointerWheel`, and retained `handlePointerClick` are
  native-to-movie methods. Rendered ActionScript sprites own semantic hit testing.

The synthetic provider exists only in ResearchDev and receives no special bridge behavior.
