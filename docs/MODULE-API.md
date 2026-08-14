# Absolute Control Panel module API

> **Status:** Executable ABI-v1 candidate, not frozen. Product-named discovery is preferred;
> research aliases and missing presentation metadata are documented in
> [current state](CURRENT-STATE.md) and [SDK status](SDK-STATUS.md).

Absolute Control Panel is a native configuration-menu host. It is intended to
play the role commonly filled by a Mod Configuration Menu: one native Starfield menu renders
configuration pages supplied by independently installed SFSE plugins.

The host is not an AbsoluteHOTAS settings implementation.  It owns menu registration,
rendering, navigation, input routing, draft/apply/cancel orchestration, accessibility, and safe
close behavior.  A provider owns its settings, validation, persistence, live behavior, and the
callbacks that translate a menu value into module state.

## Version 1 boundary

Providers include `include/AbsoluteControlPanelAPI.h` and resolve
`AbsoluteControlPanel_QueryApi` dynamically
from the host DLL.  The exported query returns `nullptr` for an unsupported ABI version.  The
contract uses only fixed-width integers, fixed-capacity character arrays, standard-layout
records, raw function pointers, and an opaque provider context.  No STL type, exception, RTTI
object, allocator ownership, or CommonLibSF type crosses the DLL boundary.

A provider registers one or more `PageDescriptorV1` records.  The host copies the page and
control descriptors before `registerPage` returns; the provider must keep its context and
callbacks alive until it calls `unregisterModule` or Starfield exits.  Control IDs need only be
unique within a page.  The pair `(moduleId, pageId)` is globally unique.

The presentation schema is fixed: each registered module appears once in the vertical mod
sidebar, and that module's registered pages appear as horizontal tabs across the top. Page
registration order defines tab order. Providers should therefore give the module one concise
display name, give each page a short tab title, and split larger configuration surfaces into
coherent pages instead of repeating the module name in every page title.

Version 1 describes these typed controls:

- Boolean toggle;
- integer and floating-point slider;
- choice;
- action button; and
- button binding.

The prototype renderer currently proves these semantic types but does not yet expose enough
descriptor data for the release widget set. Before the public SDK is frozen, the contract must
add provider-owned choice labels, bounded string editing, and section/layout metadata. The host
chooses the standard widget and geometry from semantic type plus bounded presentation hints;
providers do not submit coordinates, sprites, ActionScript, or arbitrary drawing callbacks.

Every value read or written carries a `ValueKind`.  Providers must reject a mismatched kind,
an unknown control ID, and out-of-range data.  A successful write changes provider-owned draft
state.  The host then invokes the page's `apply` callback when the user applies a change, or
`cancel` when the user discards it.  `requestRefresh` tells the host that values or availability
changed outside a menu command.

## Discovery pattern

An independent plugin should locate the already-loaded host module and resolve the query by
name rather than linking against a host import library.  This keeps the gameplay plugin usable
when Absolute Control Panel is absent. Provider initialization follows this order:

1. Look for `AbsoluteControlPanel.dll` after SFSE has loaded plugins.
2. Resolve `AbsoluteControlPanel_QueryApi` with the Windows loader.
3. Query exactly the ABI version compiled by the provider.
4. Validate `structSize`, `abiVersion`, and every function pointer the provider needs.
5. Register pages.  Treat `NotReady` as retryable and every other non-`Ok` result as a logged,
   non-fatal configuration-menu failure.
6. Continue loading the gameplay module even when the host is absent or rejects registration.

The earlier `SLOP_QueryApi` export remains available for research subscribers compiled against
ABI v1. New integrations use the Absolute Control Panel name exclusively.

## Ownership rules

- Absolute Control Panel may copy presentation metadata, but never caches a pointer to a provider descriptor.
- Providers never call Starfield UI functions through this API.
- The host never reads or writes a provider's configuration file directly.
- The host may implement generic input capture; it submits the resulting binding string through
  the provider's normal draft callback.
- Providers return errors instead of throwing across the boundary.
- Provider callbacks must be short and must not block the game thread on filesystem or network
  work.
- A provider unregisters before any callback code can become invalid.  Runtime plugin unloading
  is not supported by version 1.

## Current executable proof

`src/ResearchModule.cpp` is a synthetic subscriber. It registers a toggle, integer slider, and
input binding through the same public ABI available to another DLL. The
native menu reads and writes those values only through copied descriptors and provider
callbacks.  The provider persists them to an ignored research-only configuration file.

The contract test proves version rejection, successful registration, descriptor copying,
duplicate rejection, refresh, and unregistration without launching Starfield. A complete
isolated game run additionally proves that the subscriber's values cross the native/Scaleform
bridge and persist through its callback. Keyboard chord capture now has its own bounded host
transaction and provider round-trip tests.

Absolute Head Tracking is the first product-named external subscriber and registers General,
Axes, and Bindings through `AbsoluteControlPanel_QueryApi`. AbsoluteZero is the preserved legacy
ABI fixture and currently resolves `SLOP_QueryApi`. Both remain fail-optional and retain their
previous configuration frontends. Exact confidence and checkpoint commits are recorded in
[current implementation state](CURRENT-STATE.md).

The ActionScript movie constructs mods, page tabs, and controls from the registry. Choice labels,
broader input-device capture, richer typography, accessibility, and provider availability changes
remain explicit development gates.
