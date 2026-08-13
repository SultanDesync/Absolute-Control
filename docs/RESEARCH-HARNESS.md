# Bounded native-menu research harness

The harness adapts the useful evidence pattern preserved by earlier AbsoluteHOTAS research
without copying its gameplay injection seam. Structured events and narrow pixel tests are the
authority; screenshots and short tones make runs easy to supervise.

## Full startup run

Each full run performs one controlled native-menu experiment:

1. build the plugin and source-controlled SWF with the pinned toolchain;
2. stamp a unique run ID into a local deployed configuration;
3. verify the Address Library file and required isolated-profile mods;
4. deploy the DLL, SWF, and INI to the caller-supplied test mod;
5. launch the caller-supplied SFSE shortcut;
6. wait for the title footer pixel signal, then ask the plugin to pulse Enter from an SFSE game
   task;
7. send `W`, capture the main menu, and require the Continue row to be visibly brighter than New;
8. send `E`, wait, send `E` again, and allow the last save to load;
9. send `Esc`, then require the plugin's direct `UI::IsMenuOpen("PauseMenu")` observation;
10. send `Esc` again and require `PauseMenu` to be closed;
11. arm SLOP, wait for movie/bridge events, capture the frame, and require the magenta sentinel;
12. exercise the synthetic provider and vJoy binding through native input; and
13. close SLOP and require gameplay to resume with `PauseMenu` still closed.

All synthetic keys are fixed scan-code pulses whose down and up halves execute in game-task
callbacks. PowerShell only writes a two-line command mailbox and waits for `sent=1 error=0`
acknowledgement. It does not drive Starfield input directly. Accepted mailbox commands are
`menu_up`, `accept`, `pause`, `show_probe`, and `hide_probe`; arbitrary keys or code are rejected.

The Continue luminance test and direct PauseMenu state checks are synchronization oracles only.
They are not reverse-engineering targets and do not depend on an AbsoluteHOTAS control-cluster
offset.

## Render oracle and diagnostics

The movie contains an opaque 96x96 `#FF00FF` block. The final frame must contain a sufficiently
large near-magenta cluster; the runner records pixel count, thresholds, dimensions, and bounds
in `sentinel-signal.json`. The verified exclusive-menu 3440x1440 run contained 16,384 sentinel
pixels. This
binary signal keeps routine smoke tests independent of a vision model.

Short tones mark Continue selection, load dispatch, PauseMenu confirmation, and movie load for a
human listener. They never determine pass/fail.

On failure, the runner preserves partial plugin JSONL, enumerates visible windows owned by the
exact Starfield process, records their metadata, and captures diagnostic screenshots. Artifacts
live under ignored `artifacts/research-runs/<run-id>/` and may contain machine paths, process IDs,
or screenshots; they must never be committed.

## Retained-session cycle

Set `keepGameRunning` in a local ignored manifest to retain the loaded save. Then run:

```powershell
.\tools\research\cycle-pause.ps1 -CycleCount 3
```

The cycle runner uses the persistent plugin mailbox to pulse `Esc`, requires an ID-correlated
`open=true` or `open=false` observation, and captures both sides of every transition. This avoids
replaying the title/save-load path while studying PauseMenu construction and teardown.

Plugin DLL changes still require a process restart. Starfield also caches a loaded SWF definition
by movie path, so changing SWF bytecode at the same path requires a restart even though hot
hide/show creates a fresh menu instance. Unchanged movie lifecycle work can use `show_probe` and
`hide_probe` in the retained session.

The mailbox `show_probe` command is a research-only invocation seam. It is not the product
launch design and must never be used to leave SLOP layered above PauseMenu.

## Toolchain and local configuration

The interface build uses Apache Flex 4.16.1 (`mxmlc` build 20171115), Flash Player 11.5, SWF
version 18, and PlayerGlobal commit `fef560243029214656d83fc673be0267a1ea0816`. Bootstrap verifies
the published MD5 and a repository-pinned SHA-256. Downloaded tools remain under ignored
`.tools/`. The compiler embeds varying build metadata, so each output hash is recorded but
byte-identical hashes are not treated as a gate; the source hash and pinned inputs are.

```powershell
.\tools\research\bootstrap-interface-toolchain.ps1
.\tools\research\build-interface.ps1
```

Copy the example manifest to a `*.local.json` file, fill in local test paths, and run:

```powershell
.\tools\research\run-probe.ps1 `
  -ManifestPath .\tools\research\manifests\r1-movie-smoke.local.json
```

Local manifests are ignored. The example contains placeholders only. If clean shutdown does not
finish in time, the runner records the condition and deliberately avoids forced termination.

For adapting an independently installed module, follow the
[AI-assisted provider workflow](AI-INTEGRATION-HARNESS.md) and the
[versioned provider API](MODULE-API.md).
