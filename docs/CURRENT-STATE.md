# Current implementation state

> **Authority:** This is the primary statement of what exists today. Target contracts and
> historical research records do not override it.

This snapshot describes the architecture hardening accumulated after repository checkpoint
`0c3ccb2`, plus the retained 2026-08-13 Starfield 1.16.244 evidence and the current
Absolute Power subscriber checkpoint. Automated gates pass on the current tree. The 2026-08-15
ResearchDev runs established the earlier 35-preset/21-automation/18-diagnostic labeled-choice
registration, three-page bridge publication, native snapshot readiness, multi-frame activation
convergence, and clean menu/process teardown. Later cross-weapon testing invalidated promotion of
the Automation editor: only the Weapon 1 happy path worked. The current early-release source keeps
the page ID but publishes a three-control **Coming Soon** safety preview instead. Runtime evidence
for that reduced surface is pending. AbsoluteHOTAS H1, standalone Absolute Head Tracking, and
standalone AbsoluteZero are now runtime-qualified configuration subscribers. Head Tracking,
Absolute Power, and AbsoluteZero also enumerated together in the current native menu without a
module-list conflict. The current HOTAS/Zero compatibility slice is runtime verified:
Zero's presence selects native mouse pitch/yaw, HOTAS retains the single writer hook and other
flight lanes, and Zero uses bounded accumulator operations without a competing trampoline.
The supervised paired smoke confirmed that AbsoluteZero owns mouse auto-centering. Head Tracking
remains deliberately incompatible with HOTAS while the latter still embeds legacy head tracking;
that state is not a host-registration failure.
The project is experimental and is not a supported
Nexus or SDK release.

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
build, 9/9 native tests, 8 SDK generator tests, deterministic codegen plus generated-header compile
fixture, 26-entry catalogue, complete 15-source ActionScript provenance, artifact-tooling fixtures,
canonical release manifest, and compatibility ZIP validation all passed. Its report deliberately
records runtime and UX as `not_run`; this result does not establish release readiness.

## Product, build, and runtime boundary

| Capability | Status | Current behavior |
|---|---|---|
| Native custom menu registration | Runtime verified on current hardened DLL | Registers an independent `GameMenuBase` and dedicated source-built SWF; two Pause-origin opens in one process completed. |
| Additive PauseMenu entry | Revised label/native styling, resident-underlay transition, and native audio lease runtime verified | Appends a short `MOD OPTIONS` action without replacing `pausemenu.swf`; the row copies the native entry data shape so PauseMenu retains its typography and scale. Selecting it opens Absolute Control above the resident PauseMenu so gameplay is never exposed between menus. The panel takes a balanced native PauseMenu audio-mode lease because Starfield's audio handler ignores custom menu names. Runtime validation confirmed the gameplay ambience is suppressed in the panel and restored after Hide. Back reveals the same PauseMenu instance; a missing underlay uses a recovery Show. |
| Standalone recovery hotkey | Opt-in; unbound by default | `OpenHotkey=0x00` installs no global listener. A user may assign a Win32 virtual-key code if the canonical PauseMenu entry is inaccessible; the direct close path is not the supported ultrawide lifecycle. |
| ESM/ESP | Not required | DLL factory, runtime PauseMenu composition, and SWF path require no data plugin. |
| Runtime support | Exact-gated | Only Starfield 1.16.244 is accepted. Twenty Address Library mappings plus the lifecycle callsite/target are checked before readiness. |
| Host readiness | Runtime verified on current hardened DLL | The query table is discoverable while initializing. Registration and refresh return `NotReady` until runtime validation, hook installation, and factory retention succeed; Head Tracking and AbsoluteZero discover the current ResearchDev artifact at data-load and have a bounded post-post-data-load retry. Terminal failure returns `Rejected`. |
| Canonical build | Automated verified | Default target `AbsoluteControlPanel` emits `AbsoluteControlPanel.dll` and a packageable release-role manifest. “Packageable” identifies safe inputs; it is not a release claim. |
| Research build | Automated boundary verified | Opt-in `AbsoluteControlPanelResearchDev` emits a non-packageable manifest and alone contains the synthetic provider, mailbox/SendInput, and DirectInput research tools. The bounded live/compound registry is now shared product code. |
| CommonLibSF | Internal dependency | Pinned structural scaffolding plus an exact 1.16.244 ID overlay; no CommonLibSF/SFSE type crosses the provider ABI. |

Canonical and ResearchDev manifests include workspace-relative paths, hashes, sizes, build identity,
runtime versions, and complete SWF source-tree provenance. Research deploy refuses mixed hosts;
package validation accepts only the canonical manifest and exact package contents.

## API, session, and bridge

- `include/AbsoluteControlPanelAPI.h` is the sole product ABI authority. `include/SlopAPI.h`
  aliases product types/constants/callbacks and preserves the original query/table prefix for
  experimental subscribers; it is not a second contract.
- The host copies descriptors and centrally admits at most 512 modules, 2,048 pages, 32 pages per
  module, 128 controls per page, 512 controls per module, and 32,768 controls total. String
  capacities remain 64-byte identifiers, 96-byte labels, 192-byte descriptions, and 256-byte
  values including terminators.
- A model publishes the compact module directory, metadata for the active module's pages, and
  values/controls only for the active page. The 512-module × 3-page × 16-control fixture performs
  16 provider reads and carries 16 controls, rather than traversing all 24,576 controls.
- Raw ABI registrations require control IDs unique within each page. The generator intentionally
  applies a stricter module-wide rule because its generated pages share one callback/context set
  and its parser receives only `controlId`.
- Provider callbacks run outside registry/provider locks under in-flight leases. A successful
  draft write holds a transaction token. `unregisterModule` returns retryable `Rejected` while a
  callback or dirty transaction exists; Apply, Cancel, or destructor rollback releases the token.
- `requestRefresh` advances a page-scoped wakeup revision. The open movie acknowledges/polls on its
  UI thread; inactive value refreshes are consumed without rebuilding the active route, registry
  mutations wake it, and a newly opened menu starts from current provider values.
- Every published model has a per-session generation. Current ActionScript echoes it on commands;
  stale commands are rejected before provider mutation and receive a replacement error model. The
  ten-argument, generation-free bridge form remains only for internal v1 compatibility.
- Dynamic command/input/refresh models are coalesced and published from the next ActionScript
  frame heartbeat. They are not applied inside the native callback that is still dispatching a
  pointer or keyboard event. Accepted Close is terminal, drops pending publications, clears input
  capture ownership, and queues Hide without rebuilding the display tree.
- Pointer slider samples are coalesced to the latest position with at most one write per SWF frame.
  Queued native pointer moves are also coalesced to one pending game task.
- The product host now copies and polls bounded live-component frames only for the visible route.
  Compound operations attach the ordinary page transaction before provider mutation, and their
  replacement snapshots publish through the same deferred generation-aware bridge.

See [the API contract](MODULE-API.md), [bridge protocol](BRIDGE-PROTOCOL-V1.md), and
[architecture map](ARCHITECTURE.md).

## Shell, controls, and input

| Capability | Status | Current behavior |
|---|---|---|
| Multiple subscriber modules | Runtime and visual interaction verified on current artifact | ResearchDev enumerated alongside AbsoluteHOTAS and, in separate current runs, standalone Absolute Head Tracking and AbsoluteZero. A combined run displayed Head Tracking, Absolute Power, and AbsoluteZero together without module-list conflict. HOTAS published three pages; Head Tracking published General, Axes, and Bindings; Zero published Mouse Alignment. |
| Mod sidebar / module tabs | Runtime verified on current artifact | Modules are vertical; the selected module's pages are horizontal; current screenshots cover HOTAS, Head Tracking, and AbsoluteZero module/page selection. Current limits display 15 modules, six tabs, and 12 ordinary rows before scrolling. |
| Pointer clicks and footer | Observed plus automated source coverage | Rendered sprites own semantic hit targets for modules, tabs, controls, Apply, Cancel, and Close. |
| Slider click and drag | Runtime verified on prior artifact; generation/coalescing regression pending | Step, click-to-position, bidirectional drag, and release exist. Current code adds bounded per-frame coalescing/stale-write rejection. |
| Mouse wheel | Implemented, runtime pending | Native direction events route to hovered sidebar, tabs, or workspace; a clean two-direction current regression is still required. |
| Keyboard navigation | Observed plus automated coverage | Module/page/control/footer routes, adjustment, Apply, Cancel, Close, and keyboard capture exist; formal current keyboard-only regression remains. |
| Controller navigation | A/B/D-pad and live-grid focus implemented; device regression pending | The native adapter routes Xbox A, B, and the four D-pad directions through the shared menu router with per-button edge suppression. On a segmented grid, Up/Down selects a system and Left/Right removes/adds a request. B cancels capture or a binding-conflict modal; A activates the focused modal/action/control. In-game traversal and repeat-cadence acceptance remain pending. |
| Keyboard binding capture | Runtime verified on current subscriber set | Head Tracking round-tripped single keys and Ctrl/Alt/Shift chords; AbsoluteZero uses the same current native capture surface for its clearable, modifier-free suppression key. Escape cancel and clear behavior have automated coverage. |
| Provider-owned controller/HOTAS capture | Runtime verified with two Input Bus consumers | The appended capability lets a provider own device enumeration/capture while Control owns the recording session, cancellation, clear action, and draft write. Head Tracking recorded and persisted a physical HOTAS binding and actuated it in gameplay. Absolute Power independently used the preliminary SDK to record Input Bus preset bindings and consume the shared pilot signal. Keyboard-only and older providers remain compatible. |
| Host-owned mouse capture | Not implemented | The mouse flag remains reserved; no generic mouse recorder is currently published. |
| Toggle/action/integer/float controls | Runtime verified on current artifact | Head Tracking exercised the current toggle, action, integer-slider, and float-slider presentation with provider draft/apply/cancel ownership. Typed callbacks, bounds, and read-only behavior remain automated. |
| Choice | Implemented; runtime interaction pending | Providers may publish up to 256 dynamic value/label pairs. The virtualized dropdown supports pointer, wheel, keyboard, paging, selection, dismissal, and readable fallback labels for bounded integral choices. Transient choices change provider-owned view state without dirtying or pinning a transaction. |
| Segmented allocation grid | Next-build interaction implementation complete; in-game acceptance pending | The product bridge and SWF size each row from provider `maximumSegments` (12 for Power), overlay 1/2/3 on colored pips, provide direct Hollow→Green→Yellow→Red→Hollow cycling, and expose +G/+Y/+R/− quick steps without the old request-count text column. Long labels use the reclaimed space; Up/Down grid focus plus Left/Right adjustment covers keyboard/controller routing. All edits retain shared Apply/Cancel ownership. Runtime persistence, layout, and frame-time acceptance remain open. |
| Text/numeric editor | Bounded text implemented; direct numeric typing pending | TextInput provides provider-bounded printable-ASCII entry with Backspace, Enter-to-draft, Escape cancel, capture teardown, and ordinary Apply/Cancel ownership. Exact integer slider stepping is available; direct numeric typing is not. |
| Dirty route/close | HOTAS H1 and Head Tracking runtime interaction verified; full input matrix pending | Page/module switching and Close open a host-owned Apply/Discard/Stay modal. Apply failure retains the provider draft and transaction lease, Discard calls provider Cancel, Stay restores the previously published focus, and only a resolved Close queues Hide. The HOTAS H1 smoke and current Head Tracking change/discard/save/stale-setting tests completed without visible seams; abnormal destruction remains fail-safe Cancel exactly once. |
| Hundreds-scale catalog | Build verified; runtime budget pending | A 512-module/1,536-page/24,576-control fixture serializes only 512 summaries, three active-module pages, and 16 active-page controls/reads. Concurrent registry stress passes; in-game UI-frame and memory measurements remain required. |
| Typography/localization | Vector font implemented; localization pending | Embedded Roboto regular/bold replaces the temporary pixel glyph renderer. Localization, accessibility, and display scaling remain open. |

Absolute Power now registers module `absolute.power` with stable Presets, Automation / Cheats
(Coming Soon), and
Diagnostics pages. Its fixed 35-control labeled-choice Presets workbench remains constant across Power's bounded
256-preset envelope and includes one populated transient source/startup-aware profile selector, create/duplicate/delete-or-hide/
revert/startup actions, binding, allocator preview, activation status, within-tier ordering, and
18 exact tier sliders, plus explicitly labeled rename and host-ordered Save & Activate. Power ships a minimal Stealth profile and headless 1-4 bindings for Balanced, Combat, Travel, and Stealth. The six-system segmented grid publishes immutable low-rate frames through a
three-slot mailbox. All mutating actions/scalars/compound edits share Power's generation-stamped,
sparse verified transaction and host teardown pin. The early Automation route publishes three
controls: Coming Soon status, the unresolved cross-weapon/policy explanation, and an immediate
persisted Disable All action. The implemented rule editor is withheld and is not an SDK promise.
Diagnostics now exposes 18 grouped read-only runtime, ship, configuration, frontend, support, and
path rows without moving ownership into Control. Current runtime evidence covers registration,
three-page model publication, a fresh native snapshot, 22-step startup settlement through
convergence, normal menu Hide, responsive process state, normal process exit, and no new dump.
Human visual/pointer/text traversal, preset/rule Apply/Cancel, Save & Activate and Disable All
persistence qualification, Control-absent execution, and full controller navigation remain pending.

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
| Direct launch/supervised run | Current | `run-probe.ps1` can launch the local shortcut and drive the bounded title/load/pause handshake; a human still judges layout and UX. Evidence is parsed structurally and written to the current product-named log. |
| Runtime mailbox / retained PauseMenu cycle | ResearchDev only | Bounded accepted/completed commands; useful for repeat lifecycle evidence, never product functionality. |
| Disposable builder v1 / magenta sentinel | Archived | `tools/process/legacy/v1`; explicit opt-in only and never current product evidence. |

Evidence logging is best effort and bounded. Screenshots/pixel signals can establish visible
presence or diagnose dialogs/layout, not provider callbacks, persistence, rollback, input
ownership, or crash freedom.

## Retained runtime evidence and lifecycle assumptions

Earlier artifacts populated the additive entry on an isolated profile and a heavily modified
600+ mod profile. The lifecycle revision completed 25 isolated PauseMenu opens with no new dump,
timeout, rejection, or observed crash. On 2026-08-14 the first Absolute Power interaction run
produced an immediate Scaleform pure-virtual CTD while selecting a page. Dump and event ordering
identified synchronous replacement-tree publication inside pointer dispatch; publication is now
deferred to the next movie frame. The replacement build completed 12 alternating native-keyboard
`selectPage` switches across the synthetic and Power modules: each command/defer event preceded a
later frame flush/publication, Starfield remained responsive, clean Close published no replacement
tree, and no new dump appeared. A subsequent manual pointer run successfully crossed many page
switches, then found stale 16:9 scene-rect state after close and an early-pointer CTD on reopen.
The second dump reaches `MenuBridge::HandlePointerPhase` before Show completes. The hardened build
delegates vanilla scene-rect/lifecycle virtuals, defers even the initial model, and holds pointer
input behind Show + release + one-frame quarantine. A 2026-08-14 ultrawide regression then completed
PauseMenu entry, pointer Back to PauseMenu, native Pause close with full 3440x1440 restoration, and
a second PauseMenu entry/populated Show in the same process with no new dump. The exact held-mouse
entry gesture and a longer repeated cycle remain monitoring gaps. Tab Back has automated native/AS
coverage; Xbox B Back still needs physical-device runtime evidence. Two rarer crashes from earlier
builds also remain monitoring context. Slider dragging was manually accepted and keyboard chords
were verified.

The current architecture assumes the host and subscriber DLLs remain loaded until process exit,
Scaleform/provider calls stay on the UI/game path, and provider callbacks are short. Normal Close
rolls back the current v1 dirty draft; external Hide/destruction uses the same idempotent teardown
without publishing into the dying movie. PauseMenu origin is claimed by the displayed session;
normal Back reveals the still-resident underlay after the panel's native Hide, while a missing
underlay queues a recovery Show. A missing bridge root records a runtime fault and queues an
explicit hide. Missing/corrupt/wrong-version movie behavior and abnormal external teardown still
require in-game failure injection.

On 2026-08-15 the full three-page AP workbench artifact set completed a fresh isolated cycle:
ResearchDev SHA256 `CFF8D3D2DF6159E75B5B67BA7C090DB303EF41112CF0FC720B9457BE5EFAE993`,
SWF SHA256 `D2DC4B8E1FE0E26F68179C169DF9F81F8F600743BAD6A1B276F14ED22510A80F`,
and Power SHA256 `9EAF799165D208F5F18C41BFB2DB7D16DA3B2489FA8A4C5CC04D30BFF549B889`.
Run `ap-native-smoke-20260815-074145` logged 37 preset, 23 automation, and 17 diagnostic controls
plus a ready grid, then progressed Balanced from contextual
`PilotNotReady` through a fresh snapshot and 22 settled pips to confirmed convergence. The bridge
deferred and published the Power three-page route while native input traversed Presets, Automation,
and Diagnostics; Escape produced normal Hide; Starfield remained responsive, accepted normal
window close, and created no new dump.

Post-checkpoint binding run `ap-native-smoke-20260815-081600` used Power SHA256
`8E23F0BF504BE6483490059B7AC666AC4B538224721D177E86EF926CEA6AB829`. It reproduced a
Mod Organizer `overwrite`-path `ReplaceFileW` rejection after successful draft validation and
read-back, then verified the same-directory write-through `MoveFileExW` fallback through the actual
native menu: `W` capture completed, Apply was accepted, Power committed generation 1 -> 2, and the
overlay read back `Balanced=W`. Starfield remained responsive, exited normally, and produced no
dump; the original test-profile custom INI was restored byte-for-byte afterward.

Targeted Mod Organizer run `ap-native-smoke-20260815-110205` used Power SHA256
`C7C0B3EA84294222BE888341A3258E7962E410DEDBEEA91D0C2ADD47E4BD2073`, ResearchDev SHA256
`32944C2B9759788ACE9C6FE55A3FC2F4AEFEE7F428E10991A41F5EAED77F7A24`, and SWF SHA256
`C65598476D3729B7EBB0274307692DC2334AAC03D01F74AD615E609954746326`. The runner hit its known
guarded-W title-selection false negative, while Starfield continued into the save and auto-opened
Control. Power logged `35 preset, 21 automation, 17 diagnostic controls; grid=ready`; visual
inspection confirmed the revised Automation route rendered without Previous/Next or the redundant
selected-rule summary and retained a clean Apply state. User input ended automation before the
popover itself was exercised, so rule-dropdown interaction remains pending.

A later 2026-08-15 Mod Organizer smoke loaded the candidate Absolute Power WeaponGroup listener,
registered `35 preset, 21 automation, 18 diagnostic controls; grid=ready`, returned normally from
Control to gameplay, and confirmed that the Weapon 1 rule could allocate from available reactor
power. Subsequent cross-weapon testing failed: another weapon could drain Weapon 1 without charging
its own system. Audit found an over-broad pre-match listener observation, only one shipped
fixed-target rule, leaked zero-based terminology, releases-before-assignments behavior, and a hold
that can expire before convergence. The run therefore proves only a Weapon 1 research path and no
longer qualifies Automation. Current source replaces the editor with a three-control Coming Soon
safety preview; that reduced registration still needs a new runtime smoke.

## Remaining release gaps

The concise prioritized list is [the debt register](DEBT-REGISTER.md). Release still requires
continued lifecycle/UX evidence, full controller navigation and non-keyboard capture implementation,
choice/text/section metadata, dirty route/close modal implementation, hundreds-scale in-game
performance evidence, live-component promotion or removal,
localization/font/display/accessibility coverage, failure injection, patch-cycle proof, ABI freeze,
SDK/package/licence/version notes, and Nexus release preparation. Do not infer any of those from
the passing automated validator.
