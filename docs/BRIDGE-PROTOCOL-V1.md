# SLOP native/ActionScript bridge protocol v1

> **Status:** Current internal prototype protocol with historical SLOP type names. It is separate
> from the product-named provider API and may change before release. The target future bridge in
> `NATIVE-MENU-CONTRACT.md` is not implemented.

This protocol is internal to the SLOP host. It is separate from the cross-DLL `SlopAPI.h` ABI.
Its purpose is to remove implementation choices from builder-agent prompts and make generic
rendering mechanically testable.

## Native-to-ActionScript model

Native code calls `_root.applyModel(model)` with this bounded object graph:

```text
model = {
  schemaVersion: 1,
  revision: uint,
  activePage: int,
  selectedControl: int,
  dirty: Boolean,
  error: String,
  pages: [ page ... ]
}

page = {
  moduleId: String,
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

The numeric `kind`, `flags`, and `valueKind` values are the corresponding `SlopAPI.h` enum values.
Only the field selected by `valueKind` is authoritative. Integer values must remain within the
exact integer range representable by Scaleform Number for any control exposed to the SWF.

Bounds for v1:

- 32 pages;
- 128 controls per page;
- 512 controls total;
- existing fixed-capacity API string limits;
- one bounded error per model and per control.

If bounds are exceeded, include what fits, set `model.error`, keep Close operational, and log the
rejection. Never publish a partially constructed object without `schemaVersion` and `pages`.

The host builds arrays with Scaleform movie-root array/object creation APIs. It publishes a new
immutable model after registration revision changes and after every accepted command. AS calls
`modelApplied(revision)` after successfully rebuilding its display list.

## ActionScript-to-native commands

Map one native function named `dispatch`. Its flat signature is:

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
  stringValue:String)
```

Every argument is present for every call. Unused values carry false, zero, zero, and empty string.
Flat arguments are deliberate: they avoid ambiguous ActionScript object parsing at the recovered
native boundary.

Supported commands:

| Command | Required identity/value | Native behavior |
|---|---|---|
| `selectPage` | module/page | Select page, clear row selection, publish model |
| `selectControl` | module/page/control | Update focus and publish model |
| `write` | module/page/control and typed value | Validate descriptor/value, call `writeDraft`, mark page dirty, publish |
| `invoke` | module/page/control | Require Action kind, call `invokeAction`, publish |
| `apply` | module/page | Call page `apply`; on success clear dirty snapshot and publish |
| `cancel` | module/page | Call page `cancel`, clear dirty state, reread and publish |
| `close` | active page | If dirty, call `cancel`; hide only after cancel returns; keep a native Escape route |

Unknown versions, commands, pages, controls, kinds, non-finite numbers, or range violations are
rejected without invoking provider code. Publish the unchanged model with a bounded error so the
UI can recover.

## Transaction state

Dirty state is tracked by `(moduleId, pageId)`, not globally by control. The first successful
`write` makes the active page dirty. Navigation to another dirty page is disallowed in v1 and
sets an explanatory model error until Apply or Cancel. This avoids hidden multi-provider
transactions in the first implementation.

Provider callbacks execute without the registry mutex. Runtime plugin unloading is unsupported;
`unregisterModule` must wait until no callback is in flight or return `Rejected`. A minimal v1
host may serialize registry mutation and UI commands on the game thread.

## Synthetic compatibility

The research provider must register through `SLOP_QueryApi` like any other module and appears in
the same model. It may remain useful for headless tests, but the bridge and SWF must not identify
it specially. Existing smoke events may be renamed to generic model/command events; the runner
must be updated in the same change so evidence remains meaningful.
