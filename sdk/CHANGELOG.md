# SDK change notes

## Next build — structured layout, accessible grids, and binding conflicts

- Added `ControlKind::GroupHeader`, `kControlLayoutInline`, and
  `kCapabilityStructuredLayout`. Generated definitions accept the queried host capability mask;
  older hosts receive header-free, non-inline fallback arrays.
- JSON sections now emit group headers and accept an optional `description`. Action options may
  use the `layoutInline` flag.
- `ProviderCallbacks` and generated pages now carry the optional labeled-choice,
  provider-binding-capture, and `reassignBinding` callback tails.
- Added `kCapabilityBindingConflictResolution`. Providers implement reassignment atomically in
  their existing draft and retain the normal Apply/Cancel lifecycle.
- Added the size-gated segmented-grid `flags` tail, `kSegmentedGridCycleOnClick`, and
  `CompoundOperationKind::SetSegmentTier`. Older live-channel descriptors remain accepted with
  zero interaction flags.

All additions retain ABI version 1. Consumers must validate `ApiV1::structSize`, pass
`ApiV1::capabilities` to generated `MakePages`, and feature-detect optional behavior.
