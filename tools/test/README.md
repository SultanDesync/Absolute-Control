# In-game test runner

Runs suites of in-game checks against the **test-hooks build** of Absolute Control and
reports pass/fail without anyone watching. It launches Starfield through MO2, advances the
title screen, continues the latest save, drives the menu through a command mailbox, checks the
plugin's evidence log, and closes the game.

## One-time setup

1. Build the test-hooks plugin. It has the same file name as the product but carries the
   mailbox, title advance and guarded Continue, and builds into `build/test-hooks/`:
   ```bash
   xmake build -y AbsoluteControlPanelTestHooks
   ```
2. Copy `test.local.example.json` to `test.local.json` (git-ignored) and check the MO2 paths
   and profile.
3. Copy `fixtures.example.json` to `fixtures.local.json` (git-ignored) and list your saves. See
   [Choosing a save](#choosing-a-save).
4. Close MO2, then let the runner add the test mod to the profile and disable other Absolute
   Control hosts. It backs up `modlist.txt` into the run folder first:
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\test\Invoke-AbsoluteTests.ps1 -PrepareProfile -DryRun
   ```

## Running

```powershell
powershell -ExecutionPolicy Bypass -File tools\test\Invoke-AbsoluteTests.ps1
```

- **MO2 and Starfield must be closed.** The runner launches
  `ModOrganizer.exe -p "<profile>" "moshortcut://:<shortcut>"` itself.
- **Leave the PC alone during a run.** The plugin's key presses go to the foreground window.
  The runner refocuses the game before each key command and refuses to send one if it can't.
- Startup is driven by native evidence, not pixels:
  1. MainMenu inserted, then loading done.
  2. The plugin presses Enter.
  3. The title entry is injected once the main panel is interactive.
  4. `select_continue_entry` natively selects the Continue row.
  5. `accept`, then the guarded `confirm_continue`.
  6. `HUDMenu` inserted.
- `-DryRun` checks config, suite, profile and build without launching. `-KeepGameRunning`
  leaves the game open afterwards. `-Suite <path>` picks another suite.
- Every run redeploys the test build with a fresh run id, so stale INIs can't answer.
- On a timeout the game is closed normally, then killed if `killOnTimeout` is true.

Results go to `artifacts/test-runs/<suite>-<timestamp>/`:
- `report.md` and `result.json`
- `runner-events.jsonl` and `plugin-events.jsonl` (this run's evidence only)
- screenshots
- any crash dump. Dumps are moved out of MO2's `overwrite\Root`, where Root Builder would
  otherwise copy them into the game folder.

A new dump, a game exit or an error dialog marks the current test `crashed` and the rest
`not_run`.

## Choosing a save

Each run can load a known context from the save catalog, `fixtures.local.json`. A suite names its
save with `"fixture": "<id>"`, and `-Fixture <id>` overrides that for one run. With neither, or with
`-Fixture latest`, the run continues the newest save in the profile.

```powershell
powershell -ExecutionPolicy Bypass -File tools\test\Invoke-AbsoluteTests.ps1 -Suite tools\test\suites\save-load.json -Fixture pilot-seated-control-v2
```

For each run with a fixture, the runner:
1. Refuses to start if the catalogued file doesn't match its SHA256, or the MO2 profile doesn't use
   local saves.
2. Backs up every save the game can see to `<saveStageRoot>\<run id>-save-stage\`, then hides them.
   That means the profile's saves and MO2's real `My Games\Starfield\__MO_Saves` folder, which MO2
   overlays them on. Set `overlaySaveDirectory` in `test.local.json` if yours is elsewhere.
3. Copies the fixture in as the only visible save, so the guarded Continue has to load it.
4. Has the plugin list the saves folder from inside the game, under MO2's virtual filesystem, and
   fails the run unless the fixture is the only save there. The report shows that listing.
5. After the game closes, moves the fixture copy and any saves made during the run to that stage
   folder's `run-saves\`, then restores the originals and checks their hashes.

Continue loads the newest save the game can see, so a stray save in any other folder MO2 overlays
would outrank an older fixture. Step 4 catches that.

If a run crashes, or used `-KeepGameRunning`, the next run restores the saves before doing anything
else. If an original has changed, the restore stops and leaves the files for inspection. The open
transaction is `absolute-test-save-stage.json` in the profile folder.

To add a save, copy it somewhere permanent, get its hash with `Get-FileHash`, and add an entry with
`id`, `file`, `sha256` and `description`. Give a changed save a new id. State labels record what
someone saw when the save was made; they aren't proof of game state.

## Testing the ship control loop

The input bus won't start while `AbsoluteHOTAS.dll` is loaded, so these runs need an MO2 profile
without HOTAS. Copy your test profile's folder (an empty `saves` folder is fine), copy
`test.local.json` to `test.rewrite.local.json`, point its `profile` at the copy, add `AbsoluteHOTAS*`
to `conflictingModPatterns`, and prepare it once:

```powershell
powershell -ExecutionPolicy Bypass -File tools\test\Invoke-AbsoluteTests.ps1 -ConfigPath tools\test\test.rewrite.local.json -Suite tools\test\suites\ship-loop.json -PrepareProfile -DryRun
```

- `ship-loop` needs no hardware. With `"testInput": true`, the run swaps DirectInput for a fake device,
  **Absolute Test Stick**, and uses the bindings in `test-bindings.ini`. `test_button:<n>:<0 or 1>`
  presses its buttons.
- `ship-loop-devices` uses real controllers and your saved bindings from `AbsoluteHOTAS.ini` and
  `AbsoluteHOTAS_Custom.ini`, which are only read. Press bound buttons during its 25 second wait.

Either way, buttons only produce `ship_action_intent` log events for now. Nothing reaches the game.

Walk speed has two suites, both for that same profile:

- `walk-speed` needs no hardware. It measures how far a movement key moves the player at each axis
  position, and tests Alt + wheel, the speed keys, saving, the gauge and its menu toggle. Test runs
  read `test-walk-speed.ini` and save to a separate file, which ends up in the run's folder, so your
  real settings aren't touched.
- `walk-speed-curve` needs no hardware. It records how far a movement key moves the player at 23 speeds,
  to show the game's response curve. It only measures, so read the distances from the evidence.
- `walk-speed-devices` starts the axis capture with real controllers connected, checks that nothing
  rewrites your settings and that device polling doesn't stall during play, and leaves real input
  deployed in the profile. Don't touch the controllers while it runs.

Start runs from PowerShell. Launched from the app's Git Bash tool, MO2 fails to start `sfse_loader.exe`
with access denied and waits on a dialog until the run times out.

## Writing suites

A suite is JSON: `name`, `description`, an optional `fixture`, an optional `testInput`, and `tests`. Each
test has an `id`, a `description`, an optional `repeat`, and `steps`, each with exactly one of:

| Step | Meaning |
|---|---|
| `{"command": "<name>"}` | Send a mailbox command and wait until the plugin completes it (or fail if rejected). |
| `{"expect": "<event>", "detail": "..." \| [...], "timeoutSeconds": 10, "since": "run"}` | Wait for an evidence event whose detail contains every listed substring. By default only events after the latest command count; `"since": "run"` searches the whole run. |
| `{"absent": "<event>", "detail": "...", "since": "run"}` | Fail if that event has already appeared, with the same matching rules as `expect`. |
| `{"waitMs": 30000}` | Wait, still watching for crashes. |
| `{"screenshot": "<label>"}` | Save a window capture. |

Mailbox commands, from `src/research/ResearchSupport.cpp`:
- **Keys:** `menu_up`, `nav_down`, `nav_left`, `nav_right`, `accept`, `confirm_continue`, `pause`,
  `probe_escape`, `key_hold:<hex scan code>:<milliseconds>` for any other key or a longer hold, and
  `key_press:<hex scan code>:<0 or 1>` to hold a key across other commands.
- **Mouse:** `test_wheel:<delta>` scrolls, where one notch is 120.
- **Fake device** (`"testInput": true`): `test_button:<n>:<0 or 1>` and `test_axis:<n>:<-1 to 1>`.
- **Menu:** `show_probe` and `hide_probe` open and close Absolute Control; `open_page:<module>/<page>`;
  `select_main_entry` and `select_continue_entry` select a title-menu row without activating it;
  `inject_pause_entry`.
- **Read-only probes:** `probe_pause_root`, `probe_main_root`, `list_saves:<folder>`, `player_probe`,
  which reports the player's position, and `ship_probe`, which reports the ship, whether
  `GetSpaceshipPilot` returns the player, and landed and docked state.

The test build also logs a `flight_signal` evidence event on every change to the pilot and flight
state in `flight-model-map.md`, and once a second with the moving values. The `signals-*` suites check
those claims against saved contexts.

## Self-test

Offline checks of everything except launching the game (suite validation, profile
preparation, deploy, mailbox format, evidence parsing against real plugin records, crash
detection, reporting):

```powershell
powershell -ExecutionPolicy Bypass -File tools\test\tests\Test-AbsoluteTestRunner.ps1
```

## Not automated yet

- Physical binding capture.
- In-cockpit ship-button effects.
- Write/Apply and dirty-close flows. These need mailbox commands that drive bridge writes.
- OSF UI coexistence and other aspect ratios. These need other profiles and display modes.
