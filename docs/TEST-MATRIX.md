# In-game test matrix

> **Status:** Current evidence ledger and pending regression list. A row marked pending is not a
> failed feature claim; it is work that has not yet earned the stated confidence.

Each accepted result records the game/runtime versions, host and SWF artifacts, profile class,
steps, semantic evidence, crash observation, and whether human UX judgment was involved. Local
logs and screenshots remain ignored.

## Status vocabulary

- **Verified** — completed with the evidence appropriate to the behavior.
- **Observed** — manually seen and useful, but lacks a complete artifact/evidence record.
- **Build verified** — automated tests pass; in-game validation is incomplete.
- **Pending** — scenario not completed on the current implementation.
- **Superseded** — result belongs to an earlier implementation and is retained only as context.

## Accepted runtime evidence

The current architecture-hardening tree has a new mechanical runtime result for the Absolute
Power subscriber but no new human visual/UX acceptance. Its automated product process passed the
release build, 9/9 native tests, 7 SDK tests, generated fixture check/compile, 26-entry catalogue,
complete ten-source SWF provenance, artifact fixtures, canonical manifest, and compatibility ZIP.
Other runtime rows below are retained evidence from earlier artifacts unless stated.

| Area | Status | Environment/evidence | Result |
|---|---|---|---|
| Native menu registration and movie bridge | Verified | Starfield 1.16.244 isolated baseline; lifecycle and bridge events | Dedicated menu factory, source SWF, `_root` bridge, model application, show/hide, and return to gameplay succeeded. |
| Additive PauseMenu entry, isolated | Verified | Host DLL SHA256 `2A87059AE94B4B5A75B4423B7092E55FBC82F3CB5A56C9AA2C44D409886A9F16`; 25 retained-save cycles | 25 boundaries, listeners, injections, and completed cycles; zero timeouts, rejections, new dumps, or observed crashes. One Control Panel show/hide was followed by five more cycles. |
| Additive PauseMenu entry, heavy profile | Verified for broad compatibility | Heavily modified 600+ mod profile with multiple UI mods | Entry populated without replacing a vanilla SWF and subsequently opened consistently. This is broad compatibility evidence, not universal compatibility. |
| Pause-to-panel visual/audio transition | Runtime verified | 2026-08-18 supervised run; resident-underlay DLL SHA256 `A389C0C52A080520F44313C7EE34A0DAE92AA82FACFE480E7AEEF3595F6F1547` followed by deployed audio-lease/unbound-hotkey build SHA256 `6606817DC790C463FEA36D7E187ADDB3597A57BC259CC553F6FD1372FF3CE1EF`; structured lease/config evidence plus human visual/audio judgment | Keeping PauseMenu resident removed the sub-second gameplay flash and Back revealed the same instance. The final build exact-gates and balances PauseMenu audio mode `2` across panel Show/Hide; repeated evidence recorded paired acquisitions/releases, and runtime validation confirmed the gameplay ambient loop is suppressed in Absolute Control and restored normally afterward. The run loaded `OpenHotkey=0x00` and recorded `open_hotkey_disabled`. |
| Multiple subscriber modules | Verified for current external subscriber set | 2026-08-17 supervised run with Absolute Head Tracking, Absolute Power, and AbsoluteZero | All three external providers appeared together in the vertical sidebar and exposed isolated pages/tabs without a module-list conflict. |
| Absolute Head Tracking | Verified | Three-page integration on Starfield 1.16.244 | General, one scrollable 12-control Axes page, and Bindings rendered and applied provider-owned values. |
| AbsoluteZero | Adapter/menu and paired HOTAS compatibility verified | Starfield 1.16.244 supervised runs on 2026-08-17; compatibility deployment Zero SHA256 `9D52AA4F345C1E8D9D10C78D65127AAC0A87EF7932EC69E5A6A2955913BCEFE1`, HOTAS SHA256 `94C47C81316A89E64EF2B0050E8463CD3108C42CBF3E0251F4ACA2AB3C574FA8`; Zero 3/3 and HOTAS 10/10 tests | The product-query adapter's Mouse Alignment page and provider-owned editing remain runtime verified. In paired mode, Zero's installed presence makes HOTAS release pitch/yaw, source aim, and embedded alignment while retaining the single writer hook and other flight lanes; Zero consumes bounded accumulator operations and installs no second trampoline. The supervised flight smoke confirmed AbsoluteZero owns mouse auto-centering. Older/mismatched HOTAS remains fail-closed. |
| Absolute Power | Complete mechanical menu exposure verified; UX/persistence and live Automation backend pending | Starfield 1.16.244; 2026-08-15 isolated run `ap-native-smoke-20260815-074145`; ResearchDev SHA256 `CFF8D3D2DF6159E75B5B67BA7C090DB303EF41112CF0FC720B9457BE5EFAE993`; SWF SHA256 `D2DC4B8E1FE0E26F68179C169DF9F81F8F600743BAD6A1B276F14ED22510A80F`; Power SHA256 `9EAF799165D208F5F18C41BFB2DB7D16DA3B2489FA8A4C5CC04D30BFF549B889` | Product-query registration accepted Presets, Automation / Cheats, and Diagnostics; Power logged fixed 37-preset/23-automation/17-diagnostic control surfaces and `grid=ready`. Native input selected Power, traversed all three pages, and the bridge published each three-page model without error. The exact-gated executor acquired a fresh ship snapshot and settled Balanced through 22 one-pip, next-frame-confirmed mutations to convergence. Escape produced normal Hide, the process remained responsive, normal window close succeeded, and no new dump appeared. The Automation page truthfully reports live event sources as not ready. Bounded rename, source-aware rule actions, collision-free allocation, and apply-before-invoke ordering pass automated tests; human preset/rule edit persistence, Save & Activate/Disable All interaction, headless shortcut execution, and live Automation event-source/telemetry implementation remain pending. |
| Power preset shortcut persistence | Verified for native keyboard capture and Mod Organizer overwrite-path fallback | Starfield 1.16.244; run `ap-native-smoke-20260815-081600`; same ResearchDev/SWF artifacts; Power SHA256 `8E23F0BF504BE6483490059B7AC666AC4B538224721D177E86EF926CEA6AB829` | The first exact reproduction proved capture and draft validation succeeded but `ReplaceFileW` rejected the verified overlay on the Mod Organizer `overwrite` target. Power now falls back to same-directory write-through `MoveFileExW`. The repeated real-menu journey captured `W`, accepted Apply, committed configuration generation 1 -> 2, and read back `Balanced=W`; normal close/exit succeeded with zero new dumps. The original custom INI was restored after the test. |
| Power Automation selected-record UX | Registration and live rendering verified; dropdown interaction pending | Starfield 1.16.244; targeted run `ap-native-smoke-20260815-110205`; ResearchDev SHA256 `32944C2B9759788ACE9C6FE55A3FC2F4AEFEE7F428E10991A41F5EAED77F7A24`; SWF SHA256 `C65598476D3729B7EBB0274307692DC2334AAC03D01F74AD615E609954746326`; Power SHA256 `C7C0B3EA84294222BE888341A3258E7962E410DEDBEEA91D0C2ADD47E4BD2073` | Power registered the 35-preset/21-automation/17-diagnostic labeled-choice surfaces with a ready grid. The harness hit the known guarded-W title-selection false negative, but Starfield continued and auto-opened Control. Live inspection confirmed Automation rendered without Previous/Next or a redundant selected-rule summary and remained clean. User input ended automation before the rule popover was exercised. |
| Deferred model publication | Mechanically verified for native keyboard; exact pointer regression pending | Same current Absolute Power artifact set; structured event sequence and dump delta | Twelve accepted `selectPage` commands each logged `bridge_model_deferred`, returned through native input handling, and then logged a later `bridge_model_flush`/applied/published sequence. Clean Close queued Hide with no replacement publication. Starfield remained responsive and produced no new dump. |
| Pause-origin Back, ultrawide repair, and reopen | Verified for one two-open cycle; continued monitoring | Starfield 1.16.244 at 3440x1440; ResearchDev SHA256 `EDE27E967BD54AC3D255F5DB4189AB59B7752E2FF7D43FC53110C04C12A156B1`; runtime SWF SHA256 `33A7E2F7089A67C102827CDD338AE49E5543651C747278E29D000A485336B769`; Power SHA256 `38D758AEB11E95CCEB51DB3969588E763B176B8707B39CEDD5571E674B281D58`; ordered evidence plus visual inspection | First Pause entry populated five pages; pointer Back queued PauseMenu after panel Hide; closing Pause restored full-width gameplay; second Pause entry populated the panel again with the pointer barrier armed only after release and one frame. No new dump appeared. The exact held-mouse entry gesture was not directly driven. |
| Back inputs | Tab automated and Pause-origin pointer Back runtime verified; Xbox B implemented, runtime pending | Native router/source tests, ActionScript source contract, and 2026-08-18 resident-underlay smoke | Tab and Escape dispatch Close. Xbox B cancels an active binding capture or dispatches Close with held/repeat suppression. Pause origin is bound to the displayed session; normal Back reveals the resident PauseMenu after native Hide, while a missing underlay receives a recovery Show. An opt-in standalone-hotkey origin closes to gameplay and remains outside the supported ultrawide lifecycle. Physical-controller evidence remains pending. |
| 512-module lazy catalog | Build verified; runtime performance pending | `slop_api_test`, `host_hardening_stress_test`, bridge/source tests; 512 × 3 × 16 fixture | Registry holds 1,536 pages/24,576 controls; one snapshot carries 512 summaries, three page records, 16 controls, and performs 16 reads. In-game UI-frame, payload, and memory budgets remain open. |
| Concurrent registry/lifetime stress | Build verified | 64 modules registered in parallel; four snapshot readers, four refresh producers, eight unregister workers; reentrant provider refresh | No deadlock or invalid snapshot; unregister is retryable while leased; terminal lifecycle rejects later mutation. This is deterministic host stress, not ThreadSanitizer or engine runtime evidence. |
| External teardown semantics | Build verified; engine fault injection pending | Session and bridge source tests | Hide/destruction teardown is idempotent, clears capture/pending publication, rolls a dirty provider back once under its transaction pin, and does not build a dying movie model. |
| Keyboard binding capture | Verified | Head Tracking integration | Single keys and Ctrl/Alt/Shift chords recorded through the native input stream and round-tripped through provider draft/persistence. Escape cancel and clear behavior have automated coverage. |
| Provider-owned controller/HOTAS capture | Runtime verified for binding and action paths | `slop_api_test`; Head Tracking physical binding/action smoke; Absolute Power Input Bus/pilot-context integration, local releasedbg SHA256 `615F2444B0899DA4570F3AE6734DDB59829A16E1AADE6A7574C34F0B2E6DEBF0` | Complete callback-set validation, capture/poll/terminal-state flow, stable binding draft write, cancellation, timeout detail, counter rebase, and capture suppression pass deterministically. Head Tracking recorded, persisted, and actuated a physical binding in gameplay. Absolute Power independently recorded preset bindings and consumed the shared pilot flag. Structured collision resolution and a formal timeout/cancel runtime matrix remain open. |
| Slider pointer drag | Verified | DLL SHA256 `407B1A8E509830B1580BF959A032167B4E148FC35CCCD61CD646FFB249BED207`; SWF SHA256 `9387416553805EF554ECDEFA4F6B7126E25B51775FF3B0142A78196AA0A26917` | Click-to-position, continuous bidirectional movement, redraw survival, reversal, and clean release worked. Runtime evidence recorded down, repeated writes, and up; human judgment rated the sliders as working well. |
| Mouse pointer navigation | Observed | Current semantic hit-target movie | Module/page/control/footer clicks were usable; rendered sprites owned hit testing. |
| Apply and Cancel | Observed plus automated coverage | Head Tracking and AbsoluteZero providers | Apply reached provider persistence; Cancel restored provider state. Cross-page dirty rejection and close rollback have native tests. |

## Known observations requiring continued monitoring

- The 2026-08-14 first Absolute Power interaction run crashed immediately while selecting a page.
  The initial dump records a Scaleform pure-virtual path and the semantic log ends inside pointer
  down after a replacement model was synchronously applied. The bridge now defers and coalesces
  dynamic publication to `ENTER_FRAME`. The native-keyboard route regression now passes with 12
  module switches and no new dump; the exact pointer interaction and a longer repeated cycle remain
  pending, so the original incident is not closed solely by this result.
- A later manual run completed many exact pointer page switches through the deferred publication
  path, then exposed two lifecycle defects. Closing the panel left a 21:9 game in the movie's
  letterboxed 16:9 scene rect, and reopening from PauseMenu crashed before the second Show message
  completed. The new dump is an access violation reached from `MenuBridge::HandlePointerPhase`:
  the global pointer poller treated `UI::IsMenuOpen` as readiness and invoked the freshly created
  movie with the PauseMenu click still in flight. The hardened build (a) delegates
  `UpdateSceneRectEvent` and related viewport slots to vanilla, (b) quarantines pointer input until
  Show completion, observed button release, and one additional movie frame, and (c) returns a
  Pause-origin user Close to PauseMenu so native teardown repairs the viewport. One 21:9 cycle
  restored 3440x1440 and reopened the panel without a new dump. Longer repetition and an exact
  held-mouse activation remain required before closing monitoring.
- Two infrequent PauseMenu/close crashes occurred in earlier builds. The later lifecycle-owned
  revision was introduced after those reports and did not reproduce them in 25 consecutive cycles.
  This reduces but does not close crash monitoring.
- An earlier scripted controller research probe left apparent right-stick values frozen near a
  negative diagonal even though vJoy monitoring appeared centered. Disabling vJoy/Steam Input and
  removing that automation restored normal mouse authority. Product builds must not drive a
  controller during startup or menu use.
- Initial PauseMenu population in the heavy profile varied across early builds, sometimes requiring
  repeated opens. The event-driven insertion revision later populated on first entry in observed
  runs. Patch/profile testing must continue to record first-open versus retry behavior.

## Pending input validation

| Scenario | Status | Required evidence |
|---|---|---|
| Mouse wheel up and down | Pending clean regression | Scroll overflowing Axes controls in both directions, plus hovered sidebar and overflowing tabs; confirm no setting mutation. |
| Keyboard-only full route | Pending formal regression | Open from PauseMenu, traverse modules/pages/controls/footer, edit, Apply, Cancel, and Close without mouse. Test the opt-in standalone recovery hotkey separately. |
| Xbox-compatible controller only | A/B/D-pad implemented; runtime pending | Prove physical traversal, activation, dirty Apply/Discard/Stay, capture cancellation, held/repeat suppression, no leaked command, and no ghost/stuck axis; add remaining reverse-adjust/page shortcuts before claiming parity. |
| Input-device transition | Build policy present; runtime pending | Mouse/keyboard switching needs a current regression. The Head Tracking Input Bus subscriber rebases on first observation, binding/device-generation change, and capture suppression; physical transition evidence is pending. |
| Mouse binding capture | Not implemented | Capture, clear, cancel, conflict behavior, and round-trip. |
| Controller/HOTAS binding capture | Core journey verified; expanded matrix pending | Head Tracking and Absolute Power now prove physical recording, persistence/action use, and shared pilot context. Still exercise POV, cancel, clear, timeout, restart/read-back, collision resolution, and generation reseed as one formal matrix. Axis capture is intentionally excluded from these action rows. |
| Close during capture/save/live update | Pending | Reliable cancel/rollback and no replayed input. |

## Pending context and lifecycle validation

| Scenario | Status |
|---|---|
| Main/title menu load with no attempt to show the panel | Pending formal regression |
| PauseMenu on foot, ship seat, docked, landed, and in space | Partial/observed; matrix incomplete |
| Save/load, death/reload, fast travel, and return to main menu | Pending |
| Alt-tab, focus loss, display-mode change, and controller hot-plug | Pending |
| Missing, corrupt, old, newer, or wrong-root bridge SWF | Pending failure injection; source tests cover canonical wrong-root fail-closed hide |
| External hide/destruction while a provider page is dirty | Session-destructor rollback is automated; engine-driven external hide remains pending runtime validation. |
| Provider absent, incompatible, rejecting, failing, or disappearing | Headless coverage; in-game failure injection pending |
| Save/reload/read-back failure | Pending failure injection |
| Starfield/SFSE update with stale Address Library or mappings | Pending; use `RUNTIME-UPDATE-RUNBOOK.md` |

## Pending display validation

| Scenario | Status |
|---|---|
| 1280x720 | Pending |
| 1920x1080 | Verified design canvas; broader UI-scale combinations pending |
| 2560x1440 | Observed; formal layout record pending |
| 3840x2160 | Pending |
| 21:9 and 32:9 | 21:9 close exposed stale scene-rect restoration; exact vanilla delegation is build-verified and runtime regression is pending. 32:9 remains pending. |
| Starfield UI scaling/accessibility settings | Pending |
| Long/pseudo-localized labels and missing glyphs | Pending; pixel glyph set is temporary |

## Pending compatibility validation

| Scenario | Status |
|---|---|
| Vanilla UI isolated profile | Verified for current core path |
| Heavy 600+ mod/UI profile | Broad compatibility verified for invocation and basic use |
| Representative PauseMenu replacement/overhaul variants | Partial; exact matrix not recorded |
| Inventory/HUD/UI-overhaul mods | Broad profile observation only |
| Frame generation and overlay/capture tools | Pending formal matrix; Control Panel has no project renderer hook |

## Required invariants

- Vanilla menus remain usable after any host failure.
- The panel always has a reachable close path.
- A Pause-origin panel covers a still-resident PauseMenu at higher render/input priority; no input
  reaches the underlay and gameplay is never exposed between menus. The panel owns one balanced
  native PauseMenu audio-mode lease for either invocation route. Opt-in standalone-hotkey sessions
  do not create a PauseMenu underlay.
- No input is emitted to gameplay while capture owns the session.
- Held inputs are reseeded before gameplay resumes.
- No disk access occurs from render/advance callbacks.
- No unbounded ActionScript payload, telemetry queue, or string crosses the bridge.
- Rendered sprites own pointer hit regions; native code does not maintain duplicate pixel geometry.
- A disabled or absent host changes no save data and requires no cleanup.
- A provider remains usable when the host is missing, incompatible, or rejected.
