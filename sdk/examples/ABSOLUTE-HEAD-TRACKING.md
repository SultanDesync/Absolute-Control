# Absolute Head Tracking first-party example

Absolute Head Tracking is the primary named SDK example because its menu and integration code are
part of the Absolute suite. The example can therefore evolve with the SDK without implying a third
party's endorsement or publishing a derivative integration surface before its author approves it.

The example is intentionally split into two layers:

- [`absolute-head-tracking.menu.json`](absolute-head-tracking.menu.json) is the declarative source
  for ordinary settings and actions.
- [`generated/AbsoluteHeadTrackingMenu.generated.h`](generated/AbsoluteHeadTrackingMenu.generated.h)
  is the checked-in ABI-v1 descriptor output used by compile and drift tests.

The camera hook, OpenTrack client, configuration parser, and gameplay policy remain in Absolute Head
Tracking. They are not copied into the SDK because they are provider implementation details rather
than menu-host responsibilities.

## What it demonstrates

- Stable module, page, section, and control IDs.
- Toggle, integer-slider, float-slider, keyboard-binding, and provider-owned action descriptors.
- Provider callbacks for reads, draft writes, Apply, Cancel, and actions.
- Deterministic generation with a checked-in header and CI drift detection.
- A provider that remains functional when Absolute Control is absent or incompatible.
- Optional `HeadPose` telemetry with profile/top wireframes, an artificial horizon, asymmetric
  limits, configurable center, direct enable/invert/tuning controls, and manual recenter routed
  through the ordinary Axes-page transaction. A shared center-aligned deadzone slider below the
  graphs drives a proportionally sized red inner gate before the yellow mapped-output indicator
  takes over.

Generate and verify the descriptors with:

```powershell
python sdk/tools/menu_codegen.py validate sdk/examples/absolute-head-tracking.menu.json
python sdk/tools/menu_codegen.py generate `
    sdk/examples/absolute-head-tracking.menu.json `
    sdk/examples/generated/AbsoluteHeadTrackingMenu.generated.h --check
```

## Runtime integration boundary

Absolute Head Tracking discovers `AbsoluteControlPanel_QueryApi` dynamically after peer SFSE
plugins have loaded. It validates the ABI version, structure size, module identity, capabilities,
and every function pointer it needs before registering the generated module and pages. A missing,
not-ready, or incompatible host leaves the head-tracking runtime operational and its fallback menu
available.

When the separately queried live API advertises `kLiveCapabilityHeadPose`, the provider registers
one `head-pose` channel on the Axes page. The descriptor references the existing yaw, pitch, and roll
enable/invert/sensitivity/minimum/center/maximum controls plus the provider-owned recenter action;
it also references the shared deadzone slider. Its wait-free callback copies only prepared
raw/output degrees. Missing capability, rejected
registration, stale telemetry, or an older host leaves all ordinary controls and head-tracking
behavior intact.

Physical flight-control bindings add one provider-owned layer that the declarative ABI-v1 generator
does not yet emit:

1. The Bindings page advertises controller-capable input controls.
2. Its capture callbacks ask the optional Absolute Input Bus to begin, poll, or cancel recording.
3. The completed canonical binding string is returned to Absolute Control in `BindingCaptureV1`.
4. Absolute Control sends that value through the normal provider draft callback.
5. Absolute Head Tracking validates and persists the binding only when the user applies the page.

At runtime, Absolute Head Tracking consumes the same Input Bus snapshots to actuate its recenter and
tracking-toggle actions. When AbsoluteHOTAS publishes authoritative pilot context, the module also
uses that signal instead of running overlapping flight detection. Neither service transfers camera
ownership or configuration ownership to the host.

The planned header-only bridge will reduce the handwritten capture/discovery glue. This example is
the acceptance case for that helper: the helper is complete only when Absolute Head Tracking can use
it without acquiring a hard binary dependency on AbsoluteHOTAS or Absolute Control.
