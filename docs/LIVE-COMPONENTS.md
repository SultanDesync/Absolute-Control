# Live and compound components

> **Status:** Product-integrated ABI candidate. The registry, MenuSession transaction lane, native
> bridge, and Scaleform renderer consume the segmented allocation grid for Absolute Power. Range
> meters and telemetry plots remain headless/tested contracts without product renderers. The export
> retains its experimental name until ABI freeze and in-game performance/accessibility acceptance.

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
output bars. Ordinary sliders can adjust the overlay values and cause an immediate redraw. A later
direct-manipulation pass may allow a marker or band edge to be dragged; that interaction emits a
normal typed draft write to the associated control rather than bypassing provider validation.

### Telemetry plot

A telemetry plot contains a small bounded set of scalar series plus optional bands and markers.
The component owns a fixed-size history ring and downsampling. Providers publish current samples;
they do not allocate or transmit an ever-growing history. This component is reserved for cases
where change over time is useful, rather than replacing the cheaper range meter.

### Segmented allocation grid

A segmented grid contains bounded labeled columns and bounded segments per column. Each segment
has a semantic state, preview state, live state, and optional interaction. This covers Absolute
Power's six ship systems and up to 32 pips per system:

- hollow, green, yellow, and red allocation tiers;
- explicit Green-first, Yellow-after-Green, Red-last semantics;
- cyan-outline live powered/current indication;
- gold-tick target-preview indication;
- per-column current, maximum, and target labels;
- per-system G/Y/R requested-count labels and add-tier selection; and
- click/keyboard/controller operations to add, trim, or change tier.

The current renderer rebuilds the visible six-by-32 display from a bounded copied model at a
low-rate Power cadence. Selecting a hollow pip fills through that position with the chosen tier;
selecting a filled pip trims that position and everything above it. Power's companion 18 integer
sliders expose the same exact tier counts to keyboard navigation. A current Starfield run registered
the channel as ready and published the Power route; pointer behavior, persistence, controller
routing, and frame-time measurement remain qualification work. Pool reuse remains a performance
optimization required before the public SDK freezes.

## Live-data lane

A provider registers a live channel associated with a page/component. The host polls only while
that page is visible and the menu is active. A callback copies the latest fixed-capacity POD frame
without blocking, allocating, accessing disk, or traversing unsafe gameplay objects.

The host polls only the visible route, copies the latest frame into the immutable page model, and
coalesces replacement publication at the existing ActionScript frame boundary. Full page
descriptors and ordinary configuration values remain on the configuration model lane.

Initial policy targets are:

- host-selected cadence capped at 30 Hz for axis/head-tracking meters;
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
