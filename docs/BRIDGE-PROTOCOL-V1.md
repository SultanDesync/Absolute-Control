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
  captureModuleId: String,
  capturePageId: String,
  captureControlId: String,
  error: String,
  pages: [ page ... ]
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

The registry and render model share these admission bounds: 32 modules, 32 pages, 128 controls per
page, 512 controls total, and the public fixed-capacity strings. Registration returns
`CapacityExceeded` rather than admitting an unrenderable graph. A provider read failure marks that
control unavailable without removing other controls/pages.

`generation` increments for each session snapshot/publication attempt and is the stale-command
token. `revision` describes registry/refresh changes. ActionScript calls
`modelApplied(generation)` after rebuilding and every third frame as a cheap UI-thread refresh
heartbeat. Native republishes only when the refresh cursor advanced.

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
| `invoke` | module/page/control | Require Action, call leased `invokeAction`, publish. |
| `beginBindingCapture` | module/page/control | Require writable InputBinding/device flags, enter capture, publish. |
| `apply` | module/page | Call `apply`; on success release transaction/dirty state and publish. |
| `cancel` | module/page | Call `cancel`, release transaction/dirty state, reread, and publish. |
| `close` | active session | Cancel a normal dirty session, publish, and hide only without error. |

## Refresh and slider cadence

`requestRefresh(moduleId,pageId)` validates that the page exists, advances registry and refresh
revisions, and returns. The open bridge consumes the latest revision on `modelApplied`; multiple
requests before a poll coalesce into one full replacement model. Opening a new menu reads current
values and initializes its cursor, so it does not need to replay old wakeups.

During pointer drag, `SliderWriteCoordinator` stores only the latest pointer-derived value and its
generation/page/control identity. `ENTER_FRAME` flushes at most one provider write per SWF frame.
A replacement model with a different generation clears pending drag/write state. Native also
rejects any stale command that crosses this ActionScript guard.

## Transaction and lifetime state

Dirty state is one `(moduleId,pageId)` transaction. Navigation to another page is rejected until
Apply or Cancel. Callback leases run outside host locks. Unregister returns retryable `Rejected`
while any callback or transaction is active; it never blocks the UI thread waiting for provider
code. Runtime DLL unloading is unsupported.

Normal Close performs provider Cancel. If a menu is externally destroyed without the normal close
command, session destruction performs one best-effort provider rollback while the unregister lease
is still held, then releases the lease and clears dirty/capture state. The v1 `CancelCallback`
cannot report rollback failure.

## Other bridge functions

- `ready(1)` publishes the initial model and initializes the refresh cursor.
- `close()` requests session close.
- `focus(region,actionIndex)` synchronizes ActionScript focus used by native keyboard routing.
- `modelApplied(generation)` acknowledges a model and polls refresh.
- `handlePointerDown/Move/Up`, `handlePointerWheel`, and retained `handlePointerClick` are
  native-to-movie methods. Rendered ActionScript sprites own semantic hit testing.

The synthetic provider exists only in ResearchDev and receives no special bridge behavior.
