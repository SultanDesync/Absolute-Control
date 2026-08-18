# Absolute Control Panel menu-definition SDK

> **Status:** Experimental SDK candidate, not a supported release. Use
> [`CURRENT-STATE.md`](../docs/CURRENT-STATE.md) and
> [`SDK-STATUS.md`](../docs/SDK-STATUS.md) to distinguish implemented ABI-v1 behavior from the
> target menu language.

Subscriber developers can either handwrite ABI v1 descriptors or generate the same descriptors
from strict JSON. Both routes use `AbsoluteControlPanelAPI.h`; neither transfers configuration
ownership to the host.

## Generate descriptors

Start from `examples/absolute-head-tracking.menu.json`, then validate and generate:

```powershell
python sdk/tools/menu_codegen.py validate sdk/examples/my-mod.menu.json
python sdk/tools/menu_codegen.py generate sdk/examples/my-mod.menu.json include/MyModMenu.generated.h
```

Check generated files in with the subscriber source. CI can detect drift without rewriting files:

```powershell
python sdk/tools/menu_codegen.py generate sdk/examples/my-mod.menu.json include/MyModMenu.generated.h --check
```

The generated header provides:

- immutable `ModuleDescriptorV1` and per-page `ControlDescriptorV1` tables;
- a `ControlId` enum and `ParseControlId` helper for provider callback dispatch;
- `ProviderCallbacks`, including the optional choice, device-capture, and binding-reassignment
  callback tails; and
- `MakePages`, which wires those callbacks into `PageDescriptorV1` values for registration and
  accepts the queried host capability mask.

Each JSON section emits a non-focusable `GroupHeader` when the host reports
`kCapabilityStructuredLayout`. After size-checking the optional `ApiV1::capabilities` tail, pass
that mask—or `kCapabilityNone` when absent—to `MakePages`. Its compatibility path omits section
headers and strips `layoutInline` when connected to an older host. Two or three consecutive actions
may use the `layoutInline` flag to share one row.

The provider still implements reads, draft writes, validation, Apply, Cancel, actions, and config
persistence. Generate descriptors once during the build; JSON is not parsed in Starfield.

## Handwrite descriptors

Existing integrations may continue constructing `ModuleDescriptorV1`, `ControlDescriptorV1`, and
`PageDescriptorV1` directly. The generator is optional and does not create a second runtime API.
Handwritten and generated integrations must obey the same capacity, stable-ID, and callback rules.

## Deliberate ABI v1 limits

The schema is closed: unknown properties fail. Coordinates, dimensions, colors, fonts, textures,
CSS, ActionScript, and native drawing callbacks are therefore not expressible. The host owns layout
and style.

Sections preserve authoring order and emit presentation-only group-header controls. The only
schema-level presentation hint is `layoutInline` on Action controls; other geometry remains
host-owned. Direct ABI-v1 providers can publish labeled choices and bounded `TextInput`; those
option kinds remain outside this schema profile, although their optional page callbacks are now
wired by `ProviderCallbacks`. Advanced live components remain handwritten against
`LiveComponentsExperimentalAPI.h`; segmented grids may opt into direct pip cycling with
`kSegmentedGridCycleOnClick` and handle `SetSegmentTier`. Control IDs must be unique across the
whole module because ABI v1 callbacks receive `controlId`, not a page-qualified key.

See [CHANGELOG.md](CHANGELOG.md) for additive capability and migration notes.

## Tests

```powershell
python -m unittest discover -s sdk/tests -v
xmake -P sdk/tests/compile
xmake run -P sdk/tests/compile generated_menu_compile_test
```

Negative fixtures cover forbidden presentation fields, duplicate callback IDs, and invalid ranges.
