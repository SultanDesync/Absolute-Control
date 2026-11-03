# Live and compound components

> **Status:** Product-integrated ABI candidate. The registry, MenuSession transaction lane, native
> bridge, and Scaleform renderer consume range meters, telemetry plots, segmented grids, and
> radial response labs. The
> export retains its experimental name until ABI freeze and in-game performance/accessibility
> acceptance.

## Feasibility conclusion

The advanced Absolute suite interfaces are feasible in the native Scaleform host. They do not
require an ImGui/DXGI overlay. ActionScript graphics can draw bounded rectangles, bands, lines,
markers, curves, masks, labels, and interactive sprites, while native providers supply copied
state through the existing game/UI-thread bridge.

The release implementation must not rebuild the complete configuration model for every hardware
sample. Static configuration and live presentation use separate bounded lanes.

## Standard advanced components

### Range meter

A one-dimensional meter contains:

- a bounded numeric domain;
- a current/live marker;
- colored bands such as dead, active, cruise, reverse, and boost zones;
- fixed or draft-derived markers such as center, saturation, and detents;
- optional labels and formatted current value;
- stale/unavailable state; and
- optional editable markers associated with typed control IDs.

Providers select bounded semantic visual roles; the host theme resolves their actual colors and
states. Raw provider-supplied ARGB values are not part of the shared component contract.

This covers the AbsoluteHOTAS throttle graph, bipolar-axis graph, and Absolute Head Tracking live
output bars. Ordinary sliders adjust the overlay values and cause an immediate redraw. A marker
with a non-empty `controlId` is directly editable on pointer-capable hosts: clicking its handle or
bounded non-active zone selects it and dragging emits the same coalesced typed draft writes as the
ordinary slider. Provider validation, generation checks, and Apply/Cancel remain authoritative;
the graph never owns a second draft. Widths and other transforms that are not a one-to-one marker
value remain ordinary controls until the contract declares an explicit mapping.

Range colours are semantic rather than a provider-selected palette. The host deliberately keeps
idle/dead, zero thrust, active travel, cruise, full thrust, reverse, and boost distinguishable;
the physical live marker uses a separate high-contrast treatment. Editable marker handles and the
active-drag state are host-owned presentation.

### Telemetry plot

A telemetry plot contains a small bounded set of scalar series plus optional bands and markers.
The component owns a fixed-size history ring and downsampling. Providers publish current samples;
they do not allocate or transmit an ever-growing history. This component is reserved for cases
where change over time is useful, rather than replacing the cheaper range meter.

### Presentation hints

The additive descriptor `flags` tail also carries renderer-owned presentation hints. A pinned
component remains fixed above the scrolling control region. A secondary component may start
collapsed and is disclosed by the host without adding a provider setting. Collapsed diagnostics
continue to receive only their latest bounded sample; they do not force a full page redraw.

### Radial response lab

A radial response lab is a bounded two-axis tuning surface for systems whose behavior depends on
distance from a center point. Its descriptor names ordinary provider controls for enablement,
activation radius, idle delay, exponential decay rate, and polling rate. The host never owns a
second configuration draft.

The demo runs automatically while its page is visible. Relative mouse movement displaces the
reticle inside the outer envelope; inactivity then evaluates the provider's current activation
radius, idle delay, poll rate, and the same discrete `exp(-rate * dt)` response without ending the
demo. Movement outside the activation ring leaves the reticle parked, making the radius boundary
immediately visible. An explicit Start/Stop Demo button lets the user opt out without consuming
ordinary menu clicks. The three primary tuning sliders are mirrored beside the graph and route
through the ordinary typed, coalesced draft-write lane.

`kLiveCapabilityRadialResponse` and full descriptor/frame size checks gate the additive C3 tail.
Older live providers keep their original descriptor and dynamic-range sizes; the host continues
to accept both without interpreting former tail padding.

### Head-pose calibration

The additive `HeadPose` surface is a bounded three-axis angular instrument for head tracking and
other camera-pose providers. Each semantic axis chooses a host view—top, profile, or artificial
horizon—and references ordinary same-page sensitivity, minimum, center, and maximum controls. The
axis descriptors also name their enable and inversion toggles; the surface may name one same-page
recenter action and one shared deadzone slider. The live callback copies only raw tracker degrees,
shaped output degrees, and availability.

The host renders sparse pilot-helmet line symbols, blue negative arc, green positive arc,
yellow live marker, zero and configured-center ticks, raw/output text, compact sliders, enable and
inversion toggles, an optional manual recenter button, and a shared center-aligned deadzone slider
below the graphs. The dim inner semicircle is the deadzone scale; its centered red fraction grows
linearly from zero to the slider's declared maximum. A red probe follows sensitivity-scaled/inverted
tracker motion inside that fraction until the gate is exceeded, when the yellow outer marker takes
over as mapped camera output. Color is reinforced by labels and numerical values.
These embedded controls issue the same typed drafts or provider action as their
ordinary counterparts. A capable host suppresses the redundant flat rows while retaining those
controls in its transaction and keyboard/controller focus model; an older host renders the ordinary
rows as the deterministic fallback. No provider geometry, palette, ActionScript, SWF, or drawing
callback crosses the ABI.

`kLiveCapabilityHeadPose`, `kLiveChannelDescriptorV1HeadPoseSize`, and
`kLiveFrameV1HeadPoseSize` exact-gate the new tails. The preceding radial-response size constants
preserve compatibility with providers compiled against the former full structures.

### Segmented allocation grid

A segmented grid contains bounded labeled columns and bounded segments per column. Each segment
has a semantic state, preview state, live state, and optional interaction. This covers Absolute
Power's six ship systems and its current 12-pip allocation bars while retaining a bounded maximum
of 32 segments per provider column:

- hollow, green, yellow, and red allocation tiers;
- explicit Green-first, Yellow-after-Green, Red-last semantics;
- cyan-outline live powered/current indication;
- gold-tick target-preview indication;
- per-column current, maximum, and target labels;
- direct Hollow→Green→Yellow→Red→Hollow pip cycling with 1/2/3 glyphs;
- compact +G/+Y/+R/− quick-step operations; and
- pointer, keyboard, and controller operations to add, trim, or change tier.

The renderer sizes each bar from `maximumSegments` instead of reserving 32 visual slots. A channel
opts into direct cycling through the size-gated `flags` tail and
`kSegmentedGridCycleOnClick`; older descriptors normalize to zero flags. Power registered the
earlier channel as ready and published its route in Starfield; the next-build interaction,
persistence, controller, and frame-time acceptance pass remains qualification work.

#### Grid-row Choice associations

`kLiveCapabilityGridControlAssociations` advertises an additive, size-gated live-channel tail.
`GridControlAssociationV1` explicitly links one segmented-grid `columnId` to one `Choice`
`controlId` declared on the same page. The mapping is deliberately outside
`GridColumnDescriptorV1`: changing a record embedded in the fixed column array would change its
stride and break already-compiled ABI-v1 providers.

- The association tail is one-to-one, bounded by `kMaximumGridControlAssociations`, and validated
  for identifiers, declared columns, duplicate columns, and duplicate controls.
- Menu snapshot construction retains only targets that exist on the same page and are `Choice`
  controls. Invalid or unavailable targets remain ordinary controls rather than disappearing.
- A retained Choice renders at the end of its grid row and is suppressed from the lower scrolling
  list. A section containing only associated Choices is pruned with them.
- Pointer click or keyboard/controller activation on the focused grid row opens the same bounded
  host-owned choice popup and routes the write through the ordinary provider transaction.
- If the queried API lacks the capability, the provider publishes no associations. The grid keeps
  its normal `LIVE / TARGET` summary and the Choices remain in the ordinary list.

This v1 association supports Choice only. Toggle, action, status, and arbitrary renderer slots are
not inferred from IDs and require a future explicit capability if accepted into the SDK.

## Live-data lane

A provider registers a live channel associated with a page/component. The host polls only while
that page is visible and the menu is active. A callback copies the latest fixed-capacity POD frame
without blocking, allocating, accessing disk, or traversing unsafe gameplay objects.

The host polls only the visible route. Full page descriptors and ordinary configuration values
remain on the configuration model lane. After the initial snapshot, live updates use an
active-page-only patch: range values/bands/markers replace their live fields, while telemetry
plots append one newest sample to a movie-owned bounded history. The patch redraws only registered
live placements and never rebuilds controls, tabs, help, focus, or provider transactions.

`LiveFrameV1::dynamicRange` is an append-only, size-gated tail. It lets a provider publish bands
and markers from its current draft, including during a tuning gesture, while older providers and
hosts continue to use the static descriptor bands. `kLiveCapabilityPresentationFlags` and
`kLiveCapabilityDynamicRangeFrames` make these additions discoverable.

Initial policy targets are:

- host-selected cadence aligned to the visible movie frame for axis/head-tracking meters;
- lower cadence for ship power snapshots, with immediate refresh after an accepted edit;
- sequence number and monotonic sample timestamp on every frame;
- visible stale state when the source stops advancing;
- hard caps on channels, series, samples, bands, markers, columns, and segments; and
- automatic suspension when the menu/page is hidden.

The exact cadence remains a benchmarked host policy, not a provider demand. Visual telemetry does
not need to run at the input/controller thread's full sampling frequency.

## Interaction lane

The current scalar `write` and `invoke` commands remain correct for toggles, sliders, editable
range markers, and actions. Compound editors additionally require bounded semantic events. A
segmented grid should emit operations such as:

```text
set-segment-count(controlId, columnId, tierId, count)
trim-column(controlId, columnId, count)
set-tier(controlId, tierId)
set-segment-tier(controlId, columnId, segmentIndex, tierId)
```

The provider validates the operation, mutates its draft, computes any allocation preview, and
returns a replacement compound snapshot. The host never edits a provider-owned preset structure
directly.

These events and snapshots now use the sized v1 live-component ABI. A successful compound edit
first attaches the ordinary page transaction token, so unregister is pinned and the same page
Apply/Cancel/teardown callbacks commit or roll back both scalar and compound drafts. Packing an
allocation grid into hundreds of unrelated scalar controls or an opaque unvalidated string
remains explicitly rejected.

## Performance proof

Promotion requires an in-game benchmark that records UI frame time and update latency for:

- six live axis meters with bands and markers at the host cadence;
- at least one 120-sample scrolling plot;
- a six-by-32 interactive allocation grid;
- simultaneous draft edits and telemetry updates;
- pointer, keyboard, and controller operation; and
- stale-source, hidden-page, close, save, and provider-failure transitions.

The success criterion is stable menu presentation without polling or rendering work continuing
after the page is hidden.
