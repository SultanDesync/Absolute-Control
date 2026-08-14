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

A provider registers one or more `PageDescriptorV1` records. The host copies page/control
descriptors before `registerPage` returns; the provider keeps context/callbacks alive until
`unregisterModule` succeeds or Starfield exits. Raw ABI control IDs need only be unique within a
page because every page may supply distinct context/callbacks. The SDK generator uses one shared
`ProviderCallbacks` set and module-wide ID parser, so generated definitions deliberately require
module-wide unique IDs. The pair `(moduleId, pageId)` is globally unique.

The host admits at most 32 modules, 32 pages, 128 controls per page, and 512 controls total.
Identifiers, labels, descriptions, and string values have capacities 64, 96, 192, and 256 bytes
including the terminator. Registration rejects a graph it cannot render completely.

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
changed outside a menu command. Multiple refreshes may coalesce; the open UI consumes a refresh
revision on its UI-thread heartbeat and rereads a replacement model.

## Discovery pattern

An independent plugin should locate the already-loaded host module and resolve the query by
name rather than linking against a host import library.  This keeps the gameplay plugin usable
when Absolute Control Panel is absent. Provider initialization follows this order:

1. Look for `AbsoluteControlPanel.dll` after SFSE has loaded plugins.
2. Resolve `AbsoluteControlPanel_QueryApi` with the Windows loader.
3. Query exactly the ABI version compiled by the provider.
4. Validate `structSize`, `abiVersion`, and every function pointer the provider needs.
5. Register the module/pages. The query table may already exist while runtime setup is incomplete:
   treat `NotReady` as retryable, `Rejected` as terminal for that host process, and every other
   non-`Ok` result as a logged, non-fatal configuration-menu failure.
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
- Every callback executes outside host locks under an in-flight lease. A successful draft write
  also holds a transaction lease until Apply, Cancel, or session destruction.
- `unregisterModule` is nonblocking. It returns `Rejected` while any callback/transaction lease is
  active; the provider retries later and cannot invalidate callback code until unregister returns
  `Ok`.
- Runtime host/subscriber DLL unloading is not supported by version 1. Process-exit lifetime is the
  normal contract.

## Current executable proof

ResearchDev-only `src/ResearchModule.cpp` is a synthetic subscriber. It registers a toggle, integer slider, and
input binding through the same public ABI available to another DLL. The
native menu reads and writes those values only through copied descriptors and provider
callbacks.  The provider persists them to an ignored research-only configuration file.

The contract test proves product and legacy queries, initialization/rejection, descriptor copying,
capacity admission, duplicate rejection, refresh consumption, generation/stale-command behavior,
callback leases, dirty unregister, and retry without launching Starfield. A prior complete
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

The product `ApiV1` suffix exposes `isOpen` and `isInputCaptureActive`; the legacy table ends before
those fields and is statically checked as the exact original prefix. The product header is the
single ABI authority; no reinterpret-cast adapter exists.
