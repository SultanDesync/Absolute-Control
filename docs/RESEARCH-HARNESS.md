# Research harness status and workflows

> **Status:** Current inventory with explicit legacy gaps. Use the supported component workflows
> below. The monolithic `run-probe.ps1` success path is not currently authoritative.

The harness adapts the useful AbsoluteHOTAS research pattern: bounded commands, structured
evidence, retained game sessions, narrow visual diagnostics, and privacy-safe local artifacts. It
does not copy the HOTAS gameplay-injection seam or continuously drive a virtual controller.

## Current operating rule

The ignored local manifest supplies the exact MO2 test mod, profile requirements, Address Library,
and SFSE launcher shortcut. The shortcut is the launch interface: start it directly with
`Start-Process` or equivalent process launch. Do not navigate MO2, depend on general computer
control, or encode the user's local shortcut path in the repository.

Human input is preferred for UX judgments. Scripted input is restricted to bounded, named research
commands whose press/release lifecycle and completion are observable. The product does not operate
vJoy or Steam Input.

## Supported components

### Build and deploy

The source SWF is built with the pinned toolchain and the native host is built with xmake. Deploy a
matched DLL/SWF/config set to one explicitly supplied test mod:

```powershell
.\tools\research\bootstrap-interface-toolchain.ps1
.\tools\research\build-interface.ps1
xmake
xmake test
.\tools\research\deploy-probe.ps1 -ModPath '<test mod>' -RunId '<run id>'
```

`deploy-probe.ps1` remains research-named and stages `AbsoluteControlPanelResearch.dll`. The
product-named package path is not yet normalized; record which DLL was deployed and never combine a
DLL and SWF from different builds.

### Profile preparation

`prepare-test-profile.ps1` verifies that the explicitly named mod and every required profile mod
exist beneath the same MO2 mods directory and are enabled in the specified profile. Creation and
mod-list editing require explicit switches and a backup. The script does not download or choose
SFSE, Address Library, or another dependency.

### Direct launch and manual validation

1. Launch the manifest's `shortcut` directly.
2. Load the safe Continue save manually unless startup automation itself is under test.
3. Open Absolute Control Panel through its populated PauseMenu entry or F2 fallback.
4. Exercise the intended UX while the evidence log records lifecycle and semantic commands.
5. Preserve diagnostics locally, then report semantic outcomes without copying local paths.

Manual validation is the authority for slider feel, layout, text alignment, visual hierarchy, and
navigation comfort. Structured events and provider read-back remain the authority for mechanical
behavior.

### Runtime command mailbox

The plugin registers an independent two-line mailbox in SFSE's resolved log directory. Use
`invoke-runtime-input.ps1` with an explicit mailbox directory, run ID, evidence file, and one
allow-listed command. The helper assigns an ID atomically and requires accepted and completed
evidence. It does not accept arbitrary keys or code.

Current commands are:

- `menu_up`, `nav_down`, `nav_left`, `nav_right`, and `accept`;
- `pause` and `probe_escape`;
- `show_probe` and `hide_probe`;
- `inject_pause_entry`; and
- `probe_pause_root` and `probe_main_root`.

`show_probe` is a research invocation seam, not the product launch design. The product path is the
additive PauseMenu entry with F2 fallback.

### Retained PauseMenu cycles

`cycle-pause.ps1` reuses one loaded save to inspect repeated PauseMenu construction and teardown.
This is the efficient lifecycle regression path when the DLL and SWF are unchanged. Plugin or SWF
changes still require a Starfield restart because both binaries are cached by the process.

The lifecycle-owned implementation has completed 25 consecutive isolated cycles. New runtime
support must repeat that test according to [the update runbook](RUNTIME-UPDATE-RUNBOOK.md).

## Legacy monolithic runner

`run-probe.ps1` still contains useful process, screenshot, watchdog, evidence, and profile logic,
but its current pass path is internally inconsistent with the product:

- it requires the magenta framebuffer sentinel removed from the current SWF;
- it retains SLOP-era naming and synthetic-provider assumptions;
- it assumes the research title/Continue sequence as the default validation path; and
- its deploy/package path targets the research-named DLL rather than the product-named artifact.

Accordingly, a successful current build must not claim that this monolithic runner passed. Reuse
its supported components or repair it in a dedicated harness change. Do not restore a permanent
magenta square merely to satisfy the old oracle; if a new visual oracle is needed, make it an
explicit development mode that cannot ship accidentally.

## Evidence and diagnostics

The plugin JSONL evidence stream records wall-clock time, monotonic time, sequence, thread ID, run
ID, event, and bounded detail. Useful authorities include registration, movie/bridge readiness,
PauseMenu boundary/listener/injection, menu lifecycle, pointer down/up, semantic bridge commands,
provider results, and mailbox accepted/completed events.

Screenshots, window metadata, crash dumps, process IDs, and logs live only in ignored local artifact
directories. Screenshots help diagnose unexpected dialogs and layout failures; they do not replace
semantic or lifecycle evidence. Audio cues may help a supervising human but never decide pass/fail.

## Toolchain

The interface build uses Apache Flex 4.16.1 (`mxmlc` build 20171115), Flash Player 11.5, SWF
version 18, and PlayerGlobal commit `fef560243029214656d83fc673be0267a1ea0816`. Bootstrap verifies
the pinned checksums. Downloaded tools stay under ignored `.tools/`. Compiler metadata may vary, so
the source hash and pinned inputs are reproducibility authorities; byte-identical SWFs are not
claimed.

## Privacy and handoff

Local manifests use `*.local.json` and remain ignored. A handoff reports versions, artifact hashes,
semantic results, failure conditions, and evidence event counts without reporting drive-qualified
paths, account names, mod-list locations, screenshots, device identities, or tokens.

For provider adaptation, follow [the AI integration harness](AI-INTEGRATION-HARNESS.md). For a game
patch, follow [the runtime update runbook](RUNTIME-UPDATE-RUNBOOK.md).
