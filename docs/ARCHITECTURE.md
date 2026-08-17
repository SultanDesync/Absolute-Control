# Architecture and dependency boundaries

> **Status:** Current implementation map. This describes source ownership and allowed dependency
> direction; it is not a runtime- or release-readiness claim.

Absolute Control Panel is split by responsibility, version sensitivity, and release ownership.
The canonical product target owns only the native menu host. The opt-in research target composes
that host with automation and synthetic providers; subscriber gameplay remains in other projects.

## Build products

| Role | xmake target and DLL | Manifest | Package policy |
|---|---|---|---|
| Canonical host | `AbsoluteControlPanel` / `AbsoluteControlPanel.dll` | `build/artifact-manifests/AbsoluteControlPanel.artifacts.json` | Default target; `role=release`, `packageable=true`. This identifies package inputs but does not mean the product is release-ready. |
| Research host | `AbsoluteControlPanelResearchDev` / `AbsoluteControlPanelResearchDev.dll` | `build/artifact-manifests/AbsoluteControlPanelResearchDev.artifacts.json` | Explicit opt-in; `role=research-dev`, `packageable=false`; contains the synthetic provider, mailbox/SendInput automation, and DirectInput experiments. |

`xmake.lua` lists release and research sources explicitly. Adding a file beneath `src/` cannot
silently add it to the product. Research deployment consumes only the ResearchDev manifest and
refuses to coexist with a canonical or retired host. Package validation accepts only the canonical
release-role manifest and an exact DLL/INI/SWF/README file set.

The `product_version` value in `xmake.lua` supplies both the compiled API version string and
artifact-manifest product version; build and runtime identity must not be hard-coded independently.

## Native modules

```text
SFSE entry (Main)
  -> bootstrap coordinator (NativeMenuProbe)
       -> runtime compatibility + lifecycle state
       -> API readiness gate (MenuApiHost)
       -> PauseMenu integration + native menu factory
       -> process-lived input services

public C ABI -> MenuApiHost registry/leases -> MenuSession transactions
live/compound ABI -> LiveComponentsRegistry -> MenuSession page transaction
                                              -> MenuInputRouter
native menu factory -> ScaleformMenuBridge -> MenuSession / input adapter
                                         -> UI message queue
all runtime layers -> EvidenceLog -> bounded AsyncLineSink -> disk worker

ResearchDev only -> ResearchModule / ResearchSupport / ResearchInputCapture
```

Responsibilities and dependency rules:

- `AbsoluteControlPanelAPI.h` is the product ABI authority. `SlopAPI.h` may depend on it as a
  legacy source/ABI-prefix adapter; product code must not derive ABI values from the legacy header.
- `MenuApiHost` owns copied descriptors, capacity admission, readiness, provider callback leases,
  unregister policy, linearized compact catalog snapshots, and scoped refresh revisions. It knows
  no Scaleform geometry or research provider.
- `MenuSession` owns one UI-thread transaction, selection, dirty state, capture state, generation,
  and typed command validation. Scalar callbacks use `MenuApiHost` leases; compound edits attach
  that same transaction before calling the bounded visible-route component registry.
- `MenuInputRouter` and `input/NativeMenuInputAdapter` translate semantic navigation/capture input;
  they do not own provider state or render geometry.
- `scaleform/ScaleformMenuBridge` is the only native serializer/parser for the internal movie
  protocol. It depends inward on session/input services and outward on Scaleform and UI messaging.
- `ui/ControlPanelMenu` owns the engine menu object/factory and bridge attachment.
  `ui/PauseMenuIntegration` owns the version-sensitive additive vanilla entry seam and the pending
  invocation origin claimed by a displayed bridge session.
- `runtime/RuntimeCompatibility` owns exact game-version/relocation checks. Runtime offsets and
  object-layout assumptions must not spread into the API, session, or ActionScript layers.
- `EvidenceLog` exposes best-effort structured events. Only `diagnostics/AsyncLineSink` performs
  evidence-file writes, on its worker.
- `research/**`, `ResearchModule`, and `ResearchInputCapture` may depend on product seams for
  testing. The canonical product target must never depend on those research-only sources.

The remaining large native files are not automatically defects. PauseMenu composition and the
native/Scaleform adapter are version-sensitive integration boundaries with tightly related state.
Further splitting is justified only when it reduces runtime risk or enables behavioral tests; see
[the debt register](DEBT-REGISTER.md).

## ActionScript modules

```text
AbsoluteControlPanelMenu (document-class orchestration and public movie surface)
  -> BridgeCommandDispatcher (flat native commands + generation)
  -> SliderWriteCoordinator (one latest write per frame)
  -> MenuSelectionState (module/tab/row viewport and focus)
  -> PointerInteraction (rendered hit targets and drag identity)
  -> MenuShellRenderer (shell/display-list composition)
       -> ControlWidgets (semantic widgets and slider conversion)
       -> PixelTextRenderer
       -> PanelLayout + PanelTheme
```

Rendered sprites own hit regions. Geometry and theme constants flow from `PanelLayout` and
`PanelTheme`; native code only converts the OS pointer to the fixed 1920x1080 stage. The document
class retains the public methods invoked by native code, while helper classes own rendering,
selection, pointer, and write-coalescing responsibilities.

The authoritative interface source is the complete ordered `interface/src/**/*.as` tree, not only
the document class. `interface/dist/AbsoluteControlPanelMenu.build.json` records every source hash
plus `sourceTreeSha256`; artifact manifests carry the same inventory. The root-only
`sourceSha256` field is deprecated compatibility metadata and cannot substitute for the tree.

## Lifecycle and threading

- Scaleform calls, session dispatch, provider callbacks, and queued menu messages run on the
  native UI/game-task path. A `Session` is deliberately single-thread-owned. Command callbacks
  queue replacement models; the bridge coalesces and applies the newest model at the next movie
  frame boundary so it never rebuilds a display tree inside pointer/keyboard dispatch.
- Polling workers never call Scaleform or providers directly. They queue work through SFSE using a
  `CallbackGate`; stopping a cooperative service deactivates queued callbacks, requests worker
  cancellation, joins it, and waits for already-running callbacks.
- SFSE exposes no supported in-process plugin unload notification. Product hotkey/pointer services,
  research services, and evidence state are therefore intentionally process-lived so static
  destruction cannot join workers under the Windows loader lock. Explicit stop/shutdown functions
  exist for controlled tests and research restarts, not dynamic plugin unload.
- Evidence producers format and enqueue into a bounded queue; they do no evidence-file I/O and do
  not wait for disk. The newest record is dropped at capacity and counts are observable. The worker
  drains bounded outstanding work. Initialization/rotation and explicit flush/shutdown are not
  hot-path operations.

Runtime unloading of the host or subscriber DLLs remains unsupported. Normal menu close is
expected to cancel a dirty provider transaction before the menu object is destroyed. Abnormal
external Hide/destruction calls the same idempotent session teardown and has automated rollback
coverage, but still needs explicit runtime fault injection through the engine menu lifecycle. The
scale target and route/close transaction
state machine are specified in
[scalability, transactions, and teardown](SCALABILITY-TRANSACTIONS-AND-TEARDOWN.md).
