# Current implementation state

> **Authority:** This is the primary statement of what exists today. Target contracts and
> historical research records do not override it.

This snapshot describes the audited, not-yet-committed architecture hardening on top of repository
checkpoint `0c3ccb2`, plus the retained 2026-08-13 Starfield 1.16.244 evidence. Automated gates have
passed on the current tree; no new runtime or UX run has been performed for this implementation.
The project is experimental and is not a supported Nexus or SDK release.

## Evidence vocabulary

- **Runtime verified** — observed in Starfield with retained semantic/lifecycle evidence.
- **Observed** — seen in Starfield but lacking a complete current artifact/scenario record.
- **Automated verified** — current build or headless contract/process checks pass.
- **Implemented, runtime pending** — code exists; the relevant current in-game regression has not
  been completed.
- **Specified** — accepted target design only.
- **Historical** — provenance, not current operating guidance.

## Automated checkpoint

`tools/process/validate-current.cmd` passed end to end on the current tree: the canonical release
build, 8/8 native tests, 6 SDK generator tests, deterministic codegen plus generated-header compile
fixture, 25-entry catalogue, complete 10-source ActionScript provenance, artifact-tooling fixtures,
canonical release manifest, and compatibility ZIP validation all passed. Its report deliberately
records runtime and UX as `not_run`; this result does not establish release readiness.

## Product, build, and runtime boundary

| Capability | Status | Current behavior |
|---|---|---|
| Native custom menu registration | Runtime verified on pre-hardening artifact; current regression pending | Registers an independent `GameMenuBase` and dedicated source-built SWF. |
| Additive PauseMenu entry | Runtime verified on pre-hardening artifact; current regression pending | Appends one Control Panel action without replacing `pausemenu.swf`; selecting it closes PauseMenu and opens the panel. |
| F2 fallback | Runtime verified on pre-hardening artifact; current regression pending | Opens from gameplay when MainMenu and PauseMenu are not active. |
| ESM/ESP | Not required | DLL factory, runtime PauseMenu composition, and SWF path require no data plugin. |
| Runtime support | Exact-gated | Only Starfield 1.16.244 is accepted. Fifteen Address Library mappings plus the lifecycle callsite/target are checked before readiness. |
| Host readiness | Automated verified; runtime pending | The query table is discoverable while initializing. Registration and refresh return `NotReady` until runtime validation, hook installation, and factory retention succeed; terminal failure returns `Rejected`. |
| Canonical build | Automated verified | Default target `AbsoluteControlPanel` emits `AbsoluteControlPanel.dll` and a packageable release-role manifest. “Packageable” identifies safe inputs; it is not a release claim. |
| Research build | Automated boundary verified | Opt-in `AbsoluteControlPanelResearchDev` emits a non-packageable manifest and alone contains the synthetic provider, mailbox/SendInput, DirectInput, and experimental live components. |
| CommonLibSF | Internal dependency | Pinned structural scaffolding plus an exact 1.16.244 ID overlay; no CommonLibSF/SFSE type crosses the provider ABI. |

Canonical and ResearchDev manifests include workspace-relative paths, hashes, sizes, build identity,
runtime versions, and complete SWF source-tree provenance. Research deploy refuses mixed hosts;
package validation accepts only the canonical manifest and exact package contents.

## API, session, and bridge

- `include/AbsoluteControlPanelAPI.h` is the sole product ABI authority. `include/SlopAPI.h`
  aliases product types/constants/callbacks and preserves the original query/table prefix for
  experimental subscribers; it is not a second contract.
- The host copies descriptors and centrally admits at most 32 modules, 32 pages, 128 controls per
  page, and 512 controls total. String capacities remain 64-byte identifiers, 96-byte labels,
  192-byte descriptions, and 256-byte values including terminators.
- Raw ABI registrations require control IDs unique within each page. The generator intentionally
  applies a stricter module-wide rule because its generated pages share one callback/context set
  and its parser receives only `controlId`.
- Provider callbacks run outside registry/provider locks under in-flight leases. A successful
  draft write holds a transaction token. `unregisterModule` returns retryable `Rejected` while a
  callback or dirty transaction exists; Apply, Cancel, or destructor rollback releases the token.
- `requestRefresh` advances a dedicated wakeup revision. The open movie acknowledges/polls on its
  UI thread and republishes one fresh snapshot for any accumulated refreshes; a newly opened menu
  starts from current provider values.
- Every published model has a per-session generation. Current ActionScript echoes it on commands;
  stale commands are rejected before provider mutation and receive a replacement error model. The
  ten-argument, generation-free bridge form remains only for internal v1 compatibility.
- Pointer slider samples are coalesced to the latest position with at most one write per SWF frame.
  Queued native pointer moves are also coalesced to one pending game task.

See [the API contract](MODULE-API.md), [bridge protocol](BRIDGE-PROTOCOL-V1.md), and
[architecture map](ARCHITECTURE.md).

## Shell, controls, and input

| Capability | Status | Current behavior |
|---|---|---|
| Multiple subscriber modules | Observed on prior artifact | Head Tracking and AbsoluteZero populated one menu; exact paired artifact transcript was not retained. |
| Mod sidebar / module tabs | Runtime verified on prior artifact | Modules are vertical; the selected module's pages are horizontal; current limits display 13 modules, six tabs, and ten rows before scrolling. |
| Pointer clicks and footer | Observed plus automated source coverage | Rendered sprites own semantic hit targets for modules, tabs, controls, Apply, Cancel, and Close. |
| Slider click and drag | Runtime verified on prior artifact; generation/coalescing regression pending | Step, click-to-position, bidirectional drag, and release exist. Current code adds bounded per-frame coalescing/stale-write rejection. |
| Mouse wheel | Implemented, runtime pending | Native direction events route to hovered sidebar, tabs, or workspace; a clean two-direction current regression is still required. |
| Keyboard navigation | Observed plus automated coverage | Module/page/control/footer routes, adjustment, Apply, Cancel, Close, and keyboard capture exist; formal current keyboard-only regression remains. |
| Controller navigation | Not connected | Router/prompt scaffolding exists, but the native menu currently rejects gamepad events. No controller claim is made. |
| Keyboard binding capture | Runtime verified on prior artifact | One key plus Ctrl/Alt/Shift, Escape cancel, and clear round-trip through provider draft. |
| Mouse/controller/HOTAS capture | Not implemented | Flags are reserved; the current adapter does not capture these devices. |
| Toggle/action/integer/float controls | Implemented; prior runtime evidence retained | Typed callbacks, bounds, read-only behavior, action invocation, and draft/apply/cancel are covered. |
| Choice | Incomplete | Integer stepping exists; provider labels and a real dropdown do not. |
| Text/numeric editor | Not implemented | No bounded text field or direct numeric typing. |
| Dirty close | Provisional | Normal Close and abnormal `Session` destruction call provider Cancel exactly once. Confirmation UX and the engine-driven external-hide path still require runtime validation. |
| Typography/localization | Temporary/not solved | Pixel glyph rendering remains; complete glyphs, localization, accessibility, and display scaling are open. |

## Source architecture and diagnostics

Native bootstrap, runtime compatibility/state, API registry/leases, session, input routing/services,
menu integration, Scaleform bridge, and diagnostics now have explicit source owners. Release and
research source lists are explicit. ActionScript is split into document orchestration, bridge
dispatch, selection, pointer, widgets, shell, layout/theme, text, and slider coordination. The
complete ordered `interface/src/**/*.as` tree is the authoritative source identity; all ten files
are hashed into build metadata and manifests.

Hotkey, pointer, and ResearchDev pollers use cooperative stop tokens and callback gates. Their
owners are intentionally process-lived because SFSE has no supported plugin-unload notification
and static DLL teardown could join under loader lock. The evidence producer enqueues JSONL into a
bounded asynchronous sink; it performs no evidence-file I/O or disk wait on UI/render callbacks.
Dropped/I/O-failure counts are observable. Dynamic DLL unloading remains unsupported.

## Harness state

| Workflow | Status | Authority |
|---|---|---|
| Current automated product validator | Passed | `tools/process/validate-current.cmd`; product/build/contracts only, no game launch/deploy/MO2 mutation. |
| Research build/deploy | Current, explicit | ResearchDev manifest only; ignored local manifest supplies one test mod and direct shortcut. |
| Direct launch/manual run | Current | Human launches the local shortcut, loads a safe save, and validates PauseMenu/F2 plus UX. |
| Runtime mailbox / retained PauseMenu cycle | ResearchDev only | Bounded accepted/completed commands; useful for repeat lifecycle evidence, never product functionality. |
| Disposable builder v1 / magenta sentinel | Archived | `tools/process/legacy/v1`; explicit opt-in only and never current product evidence. |

Evidence logging is best effort and bounded. Screenshots/pixel signals can establish visible
presence or diagnose dialogs/layout, not provider callbacks, persistence, rollback, input
ownership, or crash freedom.

## Retained runtime evidence and lifecycle assumptions

Earlier artifacts populated the additive entry on an isolated profile and a heavily modified
600+ mod profile. The lifecycle revision completed 25 isolated PauseMenu opens with no new dump,
timeout, rejection, or observed crash. Two rarer crashes occurred in earlier builds and remain a
monitoring concern. Slider dragging was manually accepted and keyboard chords were verified.

The current architecture assumes the host and subscriber DLLs remain loaded until process exit,
Scaleform/provider calls stay on the UI/game path, provider callbacks are short, and normal Close
is the path that rolls back a dirty draft. A missing bridge root now records a runtime fault and
queues an explicit hide in the canonical host. Missing/corrupt/wrong-version movie behavior and
abnormal external teardown still require in-game failure injection.

## Remaining release gaps

The concise prioritized list is [the debt register](DEBT-REGISTER.md). Release still requires
current manual lifecycle/UX evidence, controller and non-keyboard capture implementation,
choice/text/section metadata, dirty-close/teardown policy, live-component promotion or removal,
localization/font/display/accessibility coverage, failure injection, patch-cycle proof, ABI freeze,
SDK/package/licence/version notes, and Nexus release preparation. Do not infer any of those from
the passing automated validator.
