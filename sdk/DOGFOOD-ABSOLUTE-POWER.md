# Absolute Power SDK dogfood record

> **Status:** Runtime verified on 2026-08-18. This document is integration evidence and an SDK
> worklist, not a promise that the experimental interfaces are frozen.

Absolute Power was integrated as if it were an independent subscriber rather than a privileged
suite component. It uses the public Absolute Control Panel and Absolute Input Bus contracts and
keeps ownership of its configuration, actions, and runtime behavior. No Absolute Power source code
is copied into this SDK.

## What the integration proved

- Dynamic API discovery and ABI/capability validation work across separately built SFSE plugins.
- A provider can publish complete Control Panel pages while retaining draft, validation, Apply,
  Cancel, persistence, and action ownership.
- Provider-owned binding capture can enumerate a physical controller, record a HOTAS button, persist
  it in the provider, and actuate the resulting binding during gameplay.
- Input Bus snapshots and monotonic button counters provide loss-resistant edge consumption without
  consuming keyboard bindings.
- The shared pilot-state signal can replace overlapping local flight-context detection.
- Live component data can drive Absolute Power's allocation display through the shared host.

The runtime-validated Absolute Power `releasedbg` DLL used for this checkpoint had SHA-256:
`615F2444B0899DA4570F3AE6734DDB59829A16E1AADE6A7574C34F0B2E6DEBF0`.

## Friction found by dogfooding

1. **Input Bus bridge helper.** A subscriber currently repeats dynamic discovery, API validation,
   capture polling, cancellation, and device/button formatting. The next preview should provide a
   zero-dependency header that bridges Control Panel binding callbacks to the Input Bus while
   preserving optional runtime discovery.
2. **Unbound representation.** The current host/provider convention clears a binding with an empty
   string. That behavior should be named and documented immediately; a typed value alternative must
   be decided before an ABI freeze rather than inferred from strings such as `None` or `Unbound`.
3. **Structured conflicts.** Conflict identity belongs in the Control Panel binding-capture result,
   because the provider owns its profiles and bindings. A native reassign or swap dialog also needs
   an explicit provider callback; adding fields alone would not complete the transaction.
4. **Sections.** The JSON authoring schema preserves sections, but ABI v1 flattens them. Section or
   group metadata should become a semantic layout feature owned by the host.
5. **Compact action rows.** Paired actions need a responsive row-group hint, not fixed pixel widths.
   The host must remain free to stack them at narrow resolutions.
6. **Allocation-grid interaction.** Experimental grid descriptors should be able to request
   click-to-cycle states and overlaid state glyphs for colorblind accessibility.
7. **Grid sizing.** `maximumSegments` already expresses the game-specific ceiling and Absolute Power
   correctly supplies 12. Remaining empty space is a renderer/layout issue, not a missing API field.
8. **Live-channel code generation.** Extend the schema and generator only after live descriptors and
   grid interaction flags settle; generating an unstable ABI would multiply churn for authors.

## SDK branch order

1. Publish the header-only Input Bus/Control Panel binding bridge and an example subscriber.
2. Specify canonical clearing behavior and design the structured conflict/replacement handshake.
3. Add section metadata and responsive inline action grouping.
4. Stabilize the experimental segmented-grid interaction and accessibility flags.
5. Extend `menu_codegen.py` to cover the stabilized live-channel descriptors.

Each step must remain dynamically discoverable and capability-gated. A subscriber built against a
newer SDK must continue to load without the optional service and must not take a binary dependency
on another gameplay mod.
