# Absolute Control Panel menu-definition SDK

> **Status:** Experimental SDK candidate, not a supported release. Use
> [`CURRENT-STATE.md`](../docs/CURRENT-STATE.md) and
> [`SDK-STATUS.md`](../docs/SDK-STATUS.md) to distinguish implemented ABI-v1 behavior from the
> target menu language.

Subscriber developers can either handwrite ABI v1 descriptors or generate the same descriptors
from strict JSON. Both routes use `AbsoluteControlPanelAPI.h`; neither transfers configuration
ownership to the host.

The SDK branch also carries first-party integration records that are intentionally kept outside the
stable host contract:

- [`examples/ABSOLUTE-HEAD-TRACKING.md`](examples/ABSOLUTE-HEAD-TRACKING.md) explains the reference
  menu definition, generated descriptors, optional host discovery, and Input Bus binding boundary.
- [`DOGFOOD-ABSOLUTE-POWER.md`](DOGFOOD-ABSOLUTE-POWER.md) records an independent-provider-style
  integration and the API changes it motivated.

Named examples published here must be first-party, contributed by their author, or covered by an
explicit license or permission for SDK use. Unapproved third-party compatibility experiments remain
private research artifacts.

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
- `ProviderCallbacks`, which accepts the provider's existing ABI callbacks and context; and
- `MakePages`, which wires those callbacks into `PageDescriptorV1` values for registration.

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

Sections preserve authoring order, but ABI v1 has no section descriptor, so section headings are not
emitted. Direct ABI-v1 providers can publish labeled choices through the appended page callback and
can use bounded `TextInput`; the generator does not emit either feature yet and rejects them rather
than silently degrading the definition. Presentation hints and advanced live components remain
outside this schema profile. Control IDs must be unique across the whole module because ABI v1
callbacks receive `controlId`, not a page-qualified key.

## Tests

```powershell
python -m unittest discover -s sdk/tests -v
xmake -P sdk/tests/compile
xmake run -P sdk/tests/compile generated_menu_compile_test
```

Negative fixtures cover forbidden presentation fields, duplicate callback IDs, and invalid ranges.
