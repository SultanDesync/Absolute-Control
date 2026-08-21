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
- bounded text input;
- presentation-only group header; and
- bounded selected-record/list-detail collection.

`GroupHeader` creates a non-focusable section divider from its label and description; the host
never calls value or mutation callbacks for it. Consecutive Action controls carrying
`kControlLayoutInline` may share one physical row in groups of two or three. The flag is rejected
on other control kinds. Providers feature-detect `kCapabilityStructuredLayout`; generated SDK
pages accept the queried capability mask and fall back to header-free, non-inline arrays for older
hosts. The host chooses standard widget geometry from semantic type plus these bounded presentation
hints; providers do not submit coordinates, sprites, ActionScript, or arbitrary drawing callbacks.

### Experimental semantic composition C2

Providers with task surfaces that cannot be expressed truthfully as a flat form may separately
resolve `AbsoluteControlPanel_QueryCompositionApi` from
`AbsoluteControlCompositionExperimentalAPI.h`. Register the stable data page first. Composition
then references those copied control IDs to describe bounded sections, cards, rows, columns,
status roles, provider-evaluated visibility/enabled state, and anchors. It never becomes a second
value, transaction, validation, or persistence API.

The current query advertises exactly `kC2Capabilities`: the C1 semantic/status/condition/anchor
surface plus live slots and validated live-series/marker associations. Nodes requiring record
presentations, pinned context, workflows, progress, or direct manipulation reject even though
their reserved vocabulary is present in the experimental header. Providers must validate
the API size and capability mask and retain a task-complete stable-page order. Missing or invalid
composition produces that deterministic flat fallback. The host copies descriptors, invokes
state callbacks only for the selected page under an unregister lease, and recovers focus by the
stable control target when a condition hides or disables the current branch. See
[Semantic UI Composition Architecture](SEMANTIC-UI-COMPOSITION-ARCHITECTURE.md) for bounds,
degradation rules, and milestone evidence. A `LiveSlot` must reference a live channel already
registered on the same module/page. Association semantic IDs must identify a copied marker or
telemetry series; they cannot be arbitrary renderer hints.

Every value read or written carries a `ValueKind`.  Providers must reject a mismatched kind,
an unknown control ID, and out-of-range data.  A successful write changes provider-owned draft
state.  The host then invokes the page's `apply` callback when the user applies a change, or
`cancel` when the user discards it.  `requestRefresh` tells the host that values or availability
changed outside a menu command. Multiple refreshes may coalesce; the open UI consumes a refresh
revision on its UI-thread heartbeat and rereads a replacement model.

Providers may optionally request that the host open at one of their registered pages through the
appended `ApiV1::requestOpenPage` callback. Before reading or calling that tail, check both
`structSize >= kApiV1RequestOpenPageSize` and `kCapabilityPageOpenRequests`. `Ok` means the route
was validated and queued, not that a movie opened synchronously. The callback is safe from a
provider worker: it coalesces a bounded route request and schedules a host task; the Scaleform
bridge performs selection on its UI-thread heartbeat. Missing hosts, older tables, absent
capability, rejected lifecycle state, or a missing route must fall back to the provider's existing
configuration UI. This command never synthesizes keyboard, mouse, or controller input.

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

An Action may set `kControlRequiresConfirmation`. The host then presents the Action label and
description in a generic Confirm/Cancel modal and does not enter provider code until Confirm is
chosen. Escape/Back/Cancel dismisses the modal without invoking the callback. After confirmation,
the existing immediate, draft-mutating, or apply-before-invoke semantics run unchanged. Providers
feature-detect `kCapabilityActionConfirmation`; the flag is rejected on non-Action controls.

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
transaction or mark the page dirty. The bit is also used by `RecordCollection` through the clearer
`kControlTransientSelection` alias and is rejected on all other control kinds. Providers must
feature-detect `kCapabilityLabeledChoices` from the appended `ApiV1::capabilities` field before
registering this flag or callback; older providers and base-size page descriptors remain valid.

`RecordCollection` is the bounded dynamic primitive for profiles, layers, macros, devices, and
other selected-record workflows. `readValue` returns the selected stable `recordId` as a string,
and the appended `readRecordItems` callback publishes at most 64 `RecordItemV1` records containing
ID, label, summary, detail, and bounded disabled/warning flags. Empty collections are valid. The
host snapshots records only for the active page, rejects duplicate/malformed IDs, and renders an
eight-row visible list with a selected-item detail pane. A selectable collection carries
`kControlTransientSelection` (an alias of the established transient Choice bit); selecting a record
calls `writeDraft` with its string ID without pinning a transaction or marking the page dirty.
Providers feature-detect `kCapabilityRecordCollections`. `PageDescriptorV1::readRecordItems` is an
appended, size-gated tail, so descriptors ending before it retain their prior behavior.

A page may mark up to three editing-context controls with `kControlPinnedContext`. Supporting
hosts keep those controls in one compact strip above live visuals and the scrolling page body, so
the selected profile/layer and its activation remain visible on every applicable page. The flag is
valid only on `Choice`, `RecordCollection`, and `InputBinding`; the controls retain their ordinary
provider-owned values, capture, transient-selection, and Apply/Cancel semantics. A provider must
feature-detect `kCapabilityPinnedContextControls` and register an unflagged fallback page set for
older hosts. Pinned controls are not duplicated in the body and remain first in keyboard/controller
focus order. Enhanced experimental compositions that predate the strip are completed by the host
with a synthesized pinned-context container, bounded by the ordinary composition node ceiling.

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

The release-safe `ControlPanelModule` dogfoods the same public ABI as daughter plugins. It registers
only host-owned module ordering and activation settings plus read-only records for registered
modules, configuration pages, live channels, and semantic compositions. Both release and
ResearchDev therefore show the same meaningful Absolute Control module; synthetic research
settings are no longer part of either runtime.

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
current Presets route declares 19 descriptors on a fully capable host: four group headers, a
transient selected-record Choice, rename/startup controls, keyboard and Input Bus bindings, four
inline lifecycle actions, and six priority Choices associated with a six-row 12-pip segmented
grid. It does not grow controls with preset count. Missing capabilities remove presentation-only
headers/inline layout, substitute bounded selection navigation, omit unavailable provider capture,
or leave the priority Choices in the ordinary list. Power owns source-aware lifecycle actions,
bindings, ordering, sparse atomic persistence, and every gameplay effect. A ResearchDev run of the
earlier full editor
confirmed registration, ready grid, movie/bridge publication of Power's three-page route, a fresh
ship snapshot, 22 one-pip settlement steps through convergence, normal Hide/exit, and no new dump.
Cross-weapon testing then invalidated promotion of the Automation editor. The current early-release
route publishes only three controls: Coming Soon status, the policy limitation, and a persisted
Disable All safety action. Its 19-control Diagnostics route reports provider-owned compatibility,
runtime, live-ship, configuration, frontend/Input Bus, support, and path state. Runtime evidence for
the compact Presets surface, reduced Automation surface, and human interaction/persistence
qualification remains open.

The ActionScript movie constructs mods, page tabs, and controls from the registry. Dynamic choice
labels and a virtualized dropdown are implemented. Broader input-device capture, accessibility,
and provider availability changes
remain explicit development gates.

The product `ApiV1` suffix exposes `isOpen` and `isInputCaptureActive`; the legacy table ends before
those fields and is statically checked as the exact original prefix. The product header is the
single ABI authority; no reinterpret-cast adapter exists.
