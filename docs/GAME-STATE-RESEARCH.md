# AC game-state research register

Date: 2026-09-19. Status: initial source inventory and qualification plan.
No new runtime flag has been qualified by this document. No game launch,
instrumentation or memory writes were performed for this source review.

## Objective

Establish dependable, independently observable game state before requiring all
input/output routes for those states. AC will own version-specific observation,
publish coherent snapshots and emit state-change events. Subscribers own the
rules that consume that state. The headless service must work without AC's menu.

Prefer existing, inspectable community definitions and the suite's evidence over
inventing offsets. A symbol name or an Address Library entry is a candidate,
not proof of semantics, current-build compatibility or safe calling phase.

## State definitions and initial evidence

The names below are proposed semantic names, not a released ABI. States overlap:
the player can remain the pilot while paused, in dialogue, or docked. Physical
role, camera mode, UI state and permission to execute an action are separate.
`dialog.fps` means on-foot dialogue here, not a requirement for first-person
camera. Split outpost construction from ship construction rather than hiding
both behind one ambiguous `buildmenu` flag.

| Proposed state | Candidate evidence already located | What still needs proving |
|---|---|---|
| `ui.menu_open(name)` | CommonLibSF `UI::IsMenuOpen`, `MenuOpenCloseEvent`; AC already observes named menu lifecycle | Startup snapshot, nested menus, teardown ordering and modded UI behavior |
| `ui.in_menu` | Named menu inventory plus `MenuModeChangeEvent` candidates | Which interactive menus qualify; HUD presence alone must not make this true |
| `game.paused` | `MenuPauseChangeEvent` and pause-counter events declared on `UI` | Event payload and counter semantics; inventory/map/dialogue can differ from Pause |
| `game.loading` | Named loading menu and game lifecycle candidates | Loading boundaries, fast travel, load failures and retained/stale menu objects |
| `build.outpost` | `WorkshopMenu` community menu name, local workshop event declarations, `kWorkshopIsometric` camera candidate | Ground/overhead views, object placement, nested prompts, ship decorating; camera alone is insufficient |
| `build.ship` | `SpaceshipEditorMenu` community menu name | Editor versus ship information screens, preview modes and exit confirmation |
| `dialogue.active` | Local dialogue event declarations and `MenuTopicManager::speaker`; candidate `DialogueMenu` query | Conversation versus ambient speech, choice-list visibility, camera disabled and exit transitions |
| `dialogue.on_foot` | Dialogue evidence combined with independently established player role | Third-person dialogue, conversation inside a ship while standing, nested trade/persuasion menus |
| `dialogue.flight` | Dialogue evidence combined with piloting/ship communication observations | Hail, docking request, scripted/radio speech, choices hidden during speech; exact native source unresolved |
| `player.is_pilot` | HOTAS selected-flight-handler cadence; CommonLibSF `GetSpaceshipPilot` candidate | Seat enter/exit, passenger, autopilot, landed/docked, pause and stalls; current actor/reference/call signature |
| `player.in_space` | CommonLibSF `TESObjectREFR::IsInSpace(bool)` declaration | Argument meaning, reference selection, being aboard versus piloting, transitions |
| `ship.docked` / `ship.landed` | Current upstream `TESObjectREFR` declarations and local ship lifecycle event types | Current-build mappings, reference ownership, nested docking and takeoff/landing transitions |
| `ship.targeting` | HOTAS currently reads targeting camera state through `NativeShipControl` | Whether camera state exactly matches action availability and how it behaves during transition/menu overlap |
| `app.focused` | Existing platform input/focus observations to inventory next | Alt-tab, background polling, dialogs and restored focus; distinct from game pause |

These rows are a starting coverage set, not an exhaustive claim about all game
states. Add inventory, scanner, vehicle, photo mode, console, death/bleedout,
cutscene and text-entry subcontexts as the consumer inventory requires them.

## Existing suite behavior to reconcile

Source inspected in the current workspace:

- `AbsoluteHOTAS/src/PilotState.cpp` and `include/PilotState.h` derive automatic
  state from selected output age, targeting state and a source-object byte at
  `+0x1B4`. The byte is documented as gameplay/suspension evidence, not a pilot
  flag. The code documents a previously mis-resolved `GetSpaceshipPilot` ID and
  explicitly rejects first-person camera as a unique cockpit signal.
- That policy uses a configurable 500–30000 ms pilot latch, while its head
  tracking gate uses 400 ms freshness. These are different policies over an
  observation, not interchangeable authoritative player-role flags.
- `AbsoluteHOTAS/include/InputBusContextPolicy.h` exports pilot validity only
  outside its Suspended state. AC should preserve the distinction between
  unknown role and forbidden output while improving the observation source.
- `Absolute Head Tracking/src/CameraHook.cpp` and `src/HeadTracking.cpp` use
  flight observation freshness (400 ms), either local or supplied by HOTAS.
  The same observation should eventually have one AC owner, not two hooks.
- `Absolute Power/src/InputBusClient.cpp` consumes context validity bits. It
  must migrate to explicit AC state validity, not infer false from missing data.
- Zero's alignment path checks AC menu/capture suppression. This is a consumer
  rule to retain, not a general definition of every game menu or player role.

The existing comments report prior validation, but that historical evidence has
not been independently re-qualified by this desk review. In particular, absence
of handler ticks during pause or a frame stall does not establish that the player
has left the pilot seat.

## Community source register

The local CommonLibSF checkout reports commit
`6eeb9e60590db21b6f983d487b6b2d51a0fb6726`, remote `libxse/commonlibsf`.
Compare that pinned checkout with upstream; do not mix layouts from different
revisions merely because class names match.

1. [CommonLibSF UI definitions](https://github.com/libxse/commonlibsf/blob/main/include/RE/U/UI.h)
   expose named menu queries, a menu stack and open/close, mode and pause event
   sources. They provide an observation starting point, not an aggregate
   definition of gameplay blocking. Local `IMenu.h` still has numerous unnamed
   flag bits; do not import meanings from another Bethesda game's library.
2. [CommonLibSF reference definitions](https://github.com/libxse/commonlibsf/blob/main/include/RE/T/TESObjectREFR.h)
   declare spaceship/pilot, in-space, docked and landed queries. Validate the
   exact revision, relocation, signature, reference lifetime and calling phase
   before invoking them. A newer declaration does not resolve the old HOTAS
   failure by itself.
3. [Creation-Hub Interface](https://github.com/Creation-Hub/Interface), formerly
   Starfield-Creators/Interface, provides community UI resources for follow-up
   menu/data-provider inspection. Its contents were not yet exhaustively audited
   for dialogue or flight-state fields in this pass.
4. [UIPlayNice's author documentation](https://github.com/Ideki/UIPlayNiceExample)
   identifies Workshop, spaceship editor and ship HUD menus, and reports a
   limitation in its Papyrus-to-UI communication while piloting. Treat that as
   a route-specific warning to test, not proof that all Papyrus events stop.
5. [DisableDialogueCameraSFSE source](https://github.com/JonathanFeenstra/DisableDialogueCameraSFSE/blob/main/src/main.cpp)
   is a historical source lead; the repository is archived. Inspect semantics
   and provenance before reusing any implementation. It is not a current-build
   dialogue-state authority.
6. [Original CommonLibSF repository](https://github.com/Starfield-Reverse-Engineering/CommonLibSF)
   is archived and directs users to libxse. Search results pointing at the old
   repository must not be mistaken for the maintained source.

## Qualification procedure

1. **Define the fact.** Record an exact meaning, consumers and expected positive,
   negative and unknown results before selecting a field. For example, pilot
   identity must remain distinguishable from whether flight controls can run.
2. **Record provenance.** For each candidate keep game executable version/hash,
   source commit, Address Library version, signature/layout checks, object
   lifetime, thread/phase constraints and any historical evidence links.
3. **Observe without acting.** Compare named menus, camera, actor/ship references
   and existing handler observations simultaneously. Do not inject an action
   merely to determine whether the state is active. Research instrumentation
   belongs in the dedicated research project under its own operating rules.
4. **Exercise confounders.** Repeat both transition directions and negative
   controls: pause while piloting, stand inside a ship, switch camera, fly without
   touching controls, wait beyond existing latch windows, alt-tab, save/load,
   enter and leave builders, hail, on-foot dialogue with camera disabled, and
   nested persuasion/trade/confirmation menus. Include frame stalls.
5. **Measure failures.** Record transition latency, false positives/negatives,
   stale duration, missing events and startup/reload behavior. A visually
   correlated sample is insufficient; require an independent state observation.
6. **Promote only the supported scope.** Mark candidates as source-only,
   runtime-observed, qualified for a named scenario/build, or rejected. Missing
   evidence remains unavailable. Ship a detector only with replayable evidence
   and tests for its policy, invalidation and lifecycle behavior.

Start with named menu lifecycle and pause/loading, then construction, pilot/ship
role, and dialogue split by role. Output pathways can be implemented later and
must not be prerequisites for declaring an observation service ready.

## AC publication contract

Publish copied, versioned state snapshots with per-signal validity, source,
observation time and world/session generation. Change events should reference
that generation; consumers can resynchronize after a missed event. Seed a
snapshot at startup rather than relying only on future open/close events.
Invalidate observations at load/revert and object loss. Sample game-owned state
at its verified game-thread phase; workers consume copies, not raw game pointers.

Keep observation separate from subscriber intent and output permission. AC can
know `player.is_pilot=true` while `game.paused=true`, with flight output denied.
Menu open, menu visible, game paused, camera mode and input owner are independent
until evidence proves a relationship. Pending or conflicting evidence should
remain explicit rather than being folded into a guessed boolean.
