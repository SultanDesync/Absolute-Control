# Per-run overwrite diagnostics

`tools/research/measure-overwrite-run.ps1` records bounded filesystem metadata for
one explicit Mod Organizer overwrite directory. It has no timer, does not hash or
read file contents, and never deletes or changes the overwrite directory.

`run-probe.ps1` now runs these two checks when its ignored local manifest supplies
`overwritePath`: immediately before launch and at the end of runner cleanup.
The latter emits `phase=live_session` when the game is retained, not `post_exit`.
For a retained run, repeat Compare against the same baseline after game exit to
capture shutdown writes; use a distinct output file to retain both observations.

Before launching a test run:

```powershell
& tools/research/measure-overwrite-run.ps1 `
    -Mode Snapshot `
    -OverwritePath $manifest.overwritePath `
    -OutputPath (Join-Path $runDirectory 'overwrite-before.json')
```

In the runner's `finally` block, after normal shutdown handling:

```powershell
& tools/research/measure-overwrite-run.ps1 `
    -Mode Compare `
    -OverwritePath $manifest.overwritePath `
    -BaselinePath (Join-Path $runDirectory 'overwrite-before.json') `
    -OutputPath (Join-Path $runDirectory 'overwrite-after.json')
```

If `keepGameRunning` retains the game, that second artifact is a retained-session
snapshot, not a post-exit snapshot. Its filename may stay `overwrite-after.json`
for consistency, but the runner event should explicitly say that capture happened
while the game remained running.

The JSON distinguishes complete totals from observed partial totals. Missing roots,
locked entries, metadata failures, and configured scan limits make authoritative
totals `null`; they are not reported as zero. Directory reparse points and reparse
files are recorded as boundaries and are not traversed or measured, so totals cover
regular non-reparse files beneath the root. Filename/path matches for `OSF`,
`OSFUI`, `WebView2`, `cache`, `GPUCache`, and `Code Cache` are only a bounded
suspicion list and do not identify a writer or establish cause. Comparison artifacts include bounded per-directory
change rollups and the largest positive-growth files.

Run the fixture tests with:

```powershell
& tools/research/tests/Test-OverwriteRunDiagnostics.ps1
```
