# Absolute Control Integration SDK

> **Status:** `0.1.0-beta.1` coordinated private beta. The public SDK release is coming soon.
> Mod authors can message the Absolute Control author through Nexus Mods to apply. Use
> [`CURRENT-STATE.md`](../docs/CURRENT-STATE.md) and
> [`SDK-STATUS.md`](../docs/SDK-STATUS.md) to distinguish implemented ABI-v1 behavior from the
> target menu language.

This beta targets the Absolute Control host shipped in **Absolute Suite 0.2.0-beta.1**. It packages
the current ABI-v1 configuration header, the separately negotiated experimental live and C2
composition headers, the menu-definition generator, examples, tests, and integration guidance.
The SDK package version and each API's ABI version are independent.

## Beta access and integration identity

Read [`BETA-ACCESS.md`](BETA-ACCESS.md) before starting an integration. Every accepted beta
consumer receives a specific package version and records its module ID, header identities, host
baseline, fallback behavior, and deployment state in
[`integration-registry.json`](integration-registry.json). Do not replace a working vendored header
in an existing released module merely to make its hash match the newest package: ABI-v1 additions
are size- and capability-gated, and stable consumers may intentionally remain on an older prefix.

The package's `AbsoluteControlSDK.manifest.json` is the distribution authority for exact file
hashes and its source commit. Report that SDK version together with the Absolute Control plugin
version when requesting integration support.

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
- `ProviderCallbacks`, including the optional choice, selected-record, device-capture, and
  binding-reassignment callback tails; and
- `MakePages`, which wires those callbacks into `PageDescriptorV1` values for registration and
  accepts the queried host capability mask; and
- `SupportsPageOpen` / `RequestOpen`, which size- and capability-gate the optional asynchronous
  host command for opening one of the generated module's registered pages.

Each JSON section emits a non-focusable `GroupHeader` when the host reports
`kCapabilityStructuredLayout`. After size-checking the optional `ApiV1::capabilities` tail, pass
that mask—or `kCapabilityNone` when absent—to `MakePages`. Its compatibility path omits section
headers and strips `layoutInline` when connected to an older host. Two or three consecutive actions
may use the `layoutInline` flag to share one row.

The schema also emits `recordCollection` as a string-valued transient selected-record control.
Providers fill the appended `readRecordItems` callback with at most 64 stable-ID records; the host
owns its bounded list/detail modal. Action options may use `requiresConfirmation`; the host shows
the Action label and description before invoking provider code. Use these options only after
feature-detecting `kCapabilityRecordCollections` and `kCapabilityActionConfirmation`, or construct
an older-host fallback definition.

The provider still implements reads, draft writes, validation, Apply, Cancel, actions, and config
persistence. Generate descriptors once during the build; JSON is not parsed in Starfield.

## Handwrite descriptors

Existing integrations may continue constructing `ModuleDescriptorV1`, `ControlDescriptorV1`, and
`PageDescriptorV1` directly. The generator is optional and does not create a second runtime API.
Handwritten and generated integrations must obey the same capacity, stable-ID, and callback rules.

## Experimental semantic composition (C2)

After registering the stable module and page descriptors, an advanced provider may
dynamically resolve `AbsoluteControlPanel_QueryCompositionApi` and request ABI 1 from
`AbsoluteControlCompositionExperimentalAPI.h`. Check `ApiV1::structSize` and every capability
bit before registering composition. The current host advertises only `kC2Capabilities`:
semantic cards/rows/columns, semantic status, provider-evaluated visible/enabled state, and
bounded anchors, plus same-page live slots and validated live-series/marker associations.

Composition references existing control IDs and changes presentation only; it does not own
values or persistence. Array order is reading/focus order, every parent precedes its children,
and every non-header stable control must be placed exactly once. Keep the ordinary page order
task-complete because missing, older, rejected, or invalid composition deterministically falls
back to that page. Register referenced live channels before composition. Record views, pinned
context, workflows, progress, and direct manipulation are vocabulary reserved for later milestones
and are not currently negotiable. C2 association IDs are validated against copied live channel
markers or series and do not permit provider drawing or direct graph mutation.

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
`kSegmentedGridCycleOnClick` and handle `SetSegmentTier`. A provider that size-checks
`ExperimentalApiV1::capabilities` and finds `kLiveCapabilityGridControlAssociations` may append
bounded `GridControlAssociationV1` records linking grid rows to same-page `Choice` controls. The
host renders those Choices on their rows, preserves keyboard/controller activation, and leaves the
ordinary controls untouched when the capability is absent. Control IDs must be unique across the
whole module because ABI v1 callbacks receive `controlId`, not a page-qualified key.

Hosts may also advertise `kLiveCapabilityPresentationFlags` and
`kLiveCapabilityDynamicRangeFrames`. The former allows pinned tuning meters and host-owned
collapsed secondary diagnostics. The latter allows a full-size `LiveFrameV1` to replace static
range bands and markers from the provider's current draft.

See [CHANGELOG.md](CHANGELOG.md) for additive capability and migration notes.

## Tests

```powershell
python -m unittest discover -s sdk/tests -v
xmake -P sdk/tests/compile
xmake run -P sdk/tests/compile generated_menu_compile_test
```

The same installed-kit compile fixture is available through CMake:

```powershell
cmake -S sdk/tests/compile -B build/sdk-consumer
cmake --build build/sdk-consumer --config Release
```

Negative fixtures cover forbidden presentation fields, duplicate callback IDs, and invalid ranges.
