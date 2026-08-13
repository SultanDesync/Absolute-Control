# SLOP module API

SLOP is the **Starfield Local Options Panel** configuration-menu host.  It is intended to
play the role commonly filled by a Mod Configuration Menu: one native Starfield menu renders
configuration pages supplied by independently installed SFSE plugins.

The host is not an AbsoluteHOTAS settings implementation.  It owns menu registration,
rendering, navigation, input routing, draft/apply/cancel orchestration, accessibility, and safe
close behavior.  A provider owns its settings, validation, persistence, live behavior, and the
callbacks that translate a menu value into module state.

## Version 1 boundary

Providers include `include/SlopAPI.h` and resolve `SLOP_QueryApi` dynamically
from the host DLL.  The exported query returns `nullptr` for an unsupported ABI version.  The
contract uses only fixed-width integers, fixed-capacity character arrays, standard-layout
records, raw function pointers, and an opaque provider context.  No STL type, exception, RTTI
object, allocator ownership, or CommonLibSF type crosses the DLL boundary.

A provider registers one or more `PageDescriptorV1` records.  The host copies the page and
control descriptors before `registerPage` returns; the provider must keep its context and
callbacks alive until it calls `unregisterModule` or Starfield exits.  Control IDs need only be
unique within a page.  The pair `(moduleId, pageId)` is globally unique.

Version 1 describes these typed controls:

- Boolean toggle;
- integer and floating-point slider;
- choice;
- action button; and
- button binding.

Every value read or written carries a `ValueKind`.  Providers must reject a mismatched kind,
an unknown control ID, and out-of-range data.  A successful write changes provider-owned draft
state.  The host then invokes the page's `apply` callback when the user applies a change, or
`cancel` when the user discards it.  `requestRefresh` tells the host that values or availability
changed outside a menu command.

## Discovery pattern

An independent plugin should locate the already-loaded host module and resolve the query by
name rather than linking against a host import library.  This keeps the gameplay plugin usable
when SLOP is absent.  Provider initialization follows this order:

1. Look for the SLOP host DLL after SFSE has loaded plugins.
2. Resolve `SLOP_QueryApi` with the Windows loader.
3. Query exactly the ABI version compiled by the provider.
4. Validate `structSize`, `abiVersion`, and every function pointer the provider needs.
5. Register pages.  Treat `NotReady` as retryable and every other non-`Ok` result as a logged,
   non-fatal configuration-menu failure.
6. Continue loading the gameplay module even when the host is absent or rejects registration.

The current research DLL is named `AbsoluteControlPanelResearch.dll`; a promoted host may use a
SLOP-specific binary name.  Consumers should therefore make the host module name configurable
during research.  The exported API name and ABI records are the compatibility boundary.

## Ownership rules

- SLOP may copy presentation metadata, but never caches a pointer to a provider descriptor.
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
button binding through the same public `SLOP_QueryApi` surface available to another DLL.  The
native menu reads and writes those values only through copied descriptors and provider
callbacks.  The provider persists them to an ignored research-only configuration file.

The contract test proves version rejection, successful registration, descriptor copying,
duplicate rejection, refresh, and unregistration without launching Starfield. A complete
isolated game run additionally proves that the subscriber's values cross the native/Scaleform
bridge, persist through its callback, and capture vJoy button input while SLOP is exclusive.

The ActionScript movie is still a deliberately fixed three-control renderer. Dynamic page and
control construction from the registry, two independently compiled providers, choice/action
semantics, dirty-state UI, and provider availability changes remain explicit research gates.
