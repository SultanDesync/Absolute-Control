# SDK change notes

## Next build — structured layout, accessible grids, and binding conflicts

- Added the independently negotiated experimental semantic-composition query and
  installed `AbsoluteControlCompositionExperimentalAPI.h`. C2 advertises
  cards/rows/columns, semantic status, provider-evaluated visibility/enabled state,
  bounded anchors, and same-page live slots with validated series/marker
  associations. Providers register stable data and referenced live channels first
  and retain their ordinary flat order as fallback. Record presentations, pinned
  context, workflows, progress, and direct manipulation remain unadvertised.

- Appended the size-gated `ApiV1::requestOpenPage` callback and
  `kCapabilityPageOpenRequests`. A provider can request one of its registered routes without
  touching Starfield UI from the caller thread; generated headers expose `SupportsPageOpen` and
  `RequestOpen`. Older hosts and provider-owned fallback menus remain unchanged.
- Added `kControlPinnedContext` and `kCapabilityPinnedContextControls`. A capable host keeps up to
  three Choice/RecordCollection/InputBinding editing-context controls above the scrolling page;
  providers register ordinary unflagged pages when the capability is absent.
- Added the size-gated `PageDescriptorV1::readRecordItems` tail,
  `ControlKind::RecordCollection`, and `kCapabilityRecordCollections`. A collection publishes at
  most 64 stable-ID list/detail records and reuses transient string `writeDraft` selection.
- Added `kControlRequiresConfirmation` and `kCapabilityActionConfirmation`. The host owns the
  Confirm/Cancel modal and invokes the existing Action callback only after affirmative resolution.
- The JSON generator accepts `recordCollection` and Action `requiresConfirmation`, and wires the
  selected-record callback tail through generated `ProviderCallbacks`.

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

- Added the size-gated `ExperimentalApiV1::capabilities` tail and
  `kLiveCapabilityGridControlAssociations`. Full-size live-channel descriptors may append bounded
  `GridControlAssociationV1` records linking rows to same-page Choice controls. The earlier
  `GridColumnDescriptorV1` layout and flags-only live descriptor remain byte-compatible.
- Added `kLiveCapabilityPresentationFlags`, pinned/secondary/collapsed channel hints, and
  `kLiveCapabilityDynamicRangeFrames`. Dynamic range bands/markers live in an append-only frame
  tail; older hosts and providers keep the original v1 payload and static descriptor ranges.

All additions retain ABI version 1. Consumers must validate `ApiV1::structSize`, pass
`ApiV1::capabilities` to generated `MakePages`, and feature-detect optional behavior.
