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

The host admits at most 512 modules, 2,048 pages, 32 pages per module, 128 controls per page, 512
controls per module, and 32,768 controls total. Identifiers, labels, descriptions, and string values
have capacities 64, 96, 192, and 256 bytes including the terminator. A visible snapshot is smaller:
all module summaries, only the active module's page metadata, and only the active page's controls.

The presentation schema is fixed: each registered module appears once in the vertical mod
sidebar, and that module's registered pages appear as horizontal tabs across the top. Page
registration order defines tab order. Providers should therefore give the module one concise
display name, give each page a short tab title, and split larger configuration surfaces into
coherent pages instead of repeating the module name in every page title.

Version 1 describes these typed controls:

- Boolean toggle;
- integer and floating-point slider;
- choice;
- action button;
- button binding; and
- bounded text input; and
- presentation-only group header.

`GroupHeader` creates a non-focusable section divider from its label and description; the host
never calls value or mutation callbacks for it. Consecutive Action controls carrying
`kControlLayoutInline` may share one physical row in groups of two or three. The flag is rejected
on other control kinds. Providers feature-detect `kCapabilityStructuredLayout`; generated SDK
pages accept the queried capability mask and fall back to header-free, non-inline arrays for older
hosts. The host chooses standard widget geometry from semantic type plus these bounded presentation
hints; providers do not submit coordinates, sprites, ActionScript, or arbitrary drawing callbacks.

Every value read or written carries a `ValueKind`.  Providers must reject a mismatched kind,
an unknown control ID, and out-of-range data.  A successful write changes provider-owned draft
state.  The host then invokes the page's `apply` callback when the user applies a change, or
`cancel` when the user discards it.  `requestRefresh` tells the host that values or availability
changed outside a menu command. Multiple refreshes may coalesce; the open UI consumes a refresh
revision on its UI-thread heartbeat and rereads a replacement model.

Actions are immediate by default. An Action descriptor may set `kControlMutatesDraft` when the
callback performs a reversible library edit such as create, delete, revert, or reorder. The page
must then provide the full Apply/Cancel contract. Before invoking it, the host pins the same page
transaction used by ordinary writes and compound edits; success marks the page dirty. A failed
first action is cancelled and releases the pin, while a failed action inside an existing draft
preserves that draft. The flag is rejected on every non-Action kind.

An Action may instead set `kControlAppliesDraftBeforeInvoke` for an ordered operation such as
Save & Activate. If the page is dirty, the host first calls `apply` under its existing transaction
pin. Failure suppresses the action and preserves the dirty draft. Success releases the transaction
and calls `invokeAction` against the just-saved state. The apply-before and mutates-draft flags are
mutually exclusive and are both rejected on non-Action controls.

`TextInput` reads and writes a string value. Its descriptor uses `minimumValue = 0`,
`stepValue = 1`, and an integral `maximumValue` from 1 through 255 as the character bound. The
current native editor accepts printable ASCII, Backspace, Enter, and Escape; the provider remains
responsible for its own content policy and final validation.

`Choice` reads and writes an integer value. A provider may append `readChoiceOptions` to its
`PageDescriptorV1`; the callback fills at most 256 `ChoiceOptionV1` value/label records. Lists are
snapshotted only for the active page and may change after `requestRefresh`. When the callback is
absent, the host synthesizes numeric labels for a bounded integral range. Every published option
must be unique, within the descriptor range, and include the current value.

Set `kControlTransientChoice` when a Choice selects provider-owned workbench state rather than
configuration. Its successful `writeDraft` callback rebuilds the model but does not attach a
transaction or mark the page dirty. The flag is rejected on other control kinds. Providers must
feature-detect `kCapabilityLabeledChoices` from the appended `ApiV1::capabilities` field before
registering this flag or callback; older providers and base-size page descriptors remain valid.

An `InputBinding` row may request provider-owned mouse/controller recording by setting
`kBindingMouse` or `kBindingController` and appending the complete
`beginBindingCapture` / `pollBindingCapture` / `cancelBindingCapture` callback set to its page.
Providers must first feature-detect `kCapabilityProviderBindingCapture`. The host owns the visible
capture session, navigation lock, Escape/Back cancellation, clear action, and ordinary draft write;
the provider owns device enumeration, debounce/timeout policy, and the stable binding string. Poll
returns `Idle` or `Capturing` until a terminal `Captured`, `Cancelled`, `TimedOut`, or `Error` state.
Callbacks run on the native UI thread and must return promptly. A captured non-empty string is
submitted through the row's normal `writeDraft` callback, so Apply/Cancel and persistence remain
provider-owned. All three callbacks must be present together; older page descriptors and
keyboard-only providers remain valid.

For a duplicate captured binding, a provider may return `BindingCaptureState::Error` with the
captured binding and conflict detail, or return `Result::Duplicate` from the ordinary binding
write. The host presents Reassign/Cancel and, after Reassign, calls the optional appended
`reassignBinding` callback. Providers feature-detect
`kCapabilityBindingConflictResolution`, atomically remove the binding from its previous owner and
assign it to the selected record's draft, then rely on the normal page Apply/Cancel lifecycle.
Older descriptors without this tail remain valid.

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
- The host implements keyboard capture. Optional device providers may implement mouse/controller
  capture through the appended callback set; either path submits the result through the provider's
  normal draft callback.
- Providers return errors instead of throwing across the boundary.
- Provider callbacks must be short and must not block the game thread on filesystem or network
  work. ABI v1 callbacks are synchronous and cooperative; the host cannot safely preempt a hung
  subscriber callback.
- Every callback executes outside host locks under an in-flight lease. A successful draft write
  also holds a transaction lease until Apply, Cancel, or session destruction.
- `unregisterModule` is nonblocking. It returns `Rejected` while any callback/transaction lease is
  active; the provider retries later and cannot invalidate callback code until unregister returns
  `Ok`.
- Runtime host/subscriber DLL unloading is not supported by version 1. Process-exit lifetime is the
  normal contract.
- A session is single-UI-thread-owned. Registry discovery, refresh, and unregister are synchronized;
  providers must keep callback context/code alive until unregister returns `Ok`.

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

Provider-owned controller capture has a deterministic host contract test covering callback-set
validation, capture polling, stable device binding round-trip, user cancellation, timeout detail,
and transaction rollback. Absolute Head Tracking is the first consumer: its provider invokes the
optional AbsoluteHOTAS Input Bus while this host continues to own presentation and draft semantics.

Absolute Head Tracking is the first product-named external subscriber and registers General,
Axes, and Bindings through `AbsoluteControlPanel_QueryApi`. AbsoluteZero is the preserved legacy
ABI fixture and currently resolves `SLOP_QueryApi`. Both remain fail-optional and retain their
previous configuration frontends. Exact confidence and checkpoint commits are recorded in
[current implementation state](CURRENT-STATE.md).

Absolute Power is the first subscriber being developed in parallel with the host. It registers
Presets, Automation / Cheats (Coming Soon), and Diagnostics through the product query. Power's
Presets route is a fixed 35-control labeled-choice selected-record workbench plus the bounded
six-by-32 segmented grid; it does not grow controls with preset count. Power owns source-aware
lifecycle actions, bindings, exact tier values, ordering, preview, activation lifecycle, sparse
atomic persistence, and every gameplay effect. A ResearchDev run of the earlier full editor
confirmed registration, ready grid, movie/bridge publication of Power's three-page route, a fresh
ship snapshot, 22 one-pip settlement steps through convergence, normal Hide/exit, and no new dump.
Cross-weapon testing then invalidated promotion of the Automation editor. The current early-release
route publishes only three controls: Coming Soon status, the policy limitation, and a persisted
Disable All safety action. Its 18-control Diagnostics route groups provider-owned compatibility,
runtime, live-ship, configuration, frontend, support, and path state. Runtime evidence for the
reduced Automation surface and human interaction/persistence qualification remain open.

The ActionScript movie constructs mods, page tabs, and controls from the registry. Dynamic choice
labels and a virtualized dropdown are implemented. Broader input-device capture, accessibility,
and provider availability changes
remain explicit development gates.

The product `ApiV1` suffix exposes `isOpen` and `isInputCaptureActive`; the legacy table ends before
those fields and is statically checked as the exact original prefix. The product header is the
single ABI authority; no reinterpret-cast adapter exists.
