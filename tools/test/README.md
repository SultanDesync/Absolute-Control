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
3. Close MO2, then let the runner add the test mod to the profile and disable other Absolute
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

## Writing suites

A suite is JSON: `name`, `description` and `tests`. Each test has an `id`, a `description`, an
optional `repeat`, and `steps`, each with exactly one of:

| Step | Meaning |
|---|---|
| `{"command": "<name>"}` | Send a mailbox command and wait until the plugin completes it (or fail if rejected). |
| `{"expect": "<event>", "detail": "..." \| [...], "timeoutSeconds": 10, "since": "run"}` | Wait for an evidence event whose detail contains every listed substring. By default only events after the latest command count; `"since": "run"` searches the whole run. |
| `{"waitMs": 30000}` | Wait, still watching for crashes. |
| `{"screenshot": "<label>"}` | Save a window capture. |

Mailbox commands, from `src/research/ResearchSupport.cpp`:
- **Keys:** `menu_up`, `nav_down`, `nav_left`, `nav_right`, `accept`, `confirm_continue`, `pause`,
  `probe_escape`.
- **Menu:** `show_probe` and `hide_probe` open and close Absolute Control; `open_page:<module>/<page>`;
  `select_main_entry` and `select_continue_entry` select a title-menu row without activating it;
  `inject_pause_entry`.
- **Read-only probes:** `probe_pause_root`, `probe_main_root`.

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
