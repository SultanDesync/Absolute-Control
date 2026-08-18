# Research harness status and workflows

> **Status:** Current product-validation and ResearchDev workflows. The disposable builder-v1
> process and monolithic sentinel runner are archived evidence, not current gates.

The harness adapts the useful AbsoluteHOTAS research pattern: bounded commands, structured
evidence, retained game sessions, narrow visual diagnostics, and privacy-safe local artifacts. It
does not copy the HOTAS gameplay-injection seam or continuously drive a virtual controller.

## Current operating rule

Run `tools/process/validate-current.cmd` first. It is the maintained product validator and does not
launch Starfield, deploy a mod, modify MO2, or use local paths. A pass proves only its automated
gates; runtime and UX stay `not_run`.

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

`deploy-probe.ps1` builds/consumes only the non-packageable ResearchDev manifest and stages
`AbsoluteControlPanelResearchDev.dll`. It refuses to mix that host with canonical
`AbsoluteControlPanel.dll` or retired `AbsoluteControlPanelResearch.dll`, and verifies deployed
DLL/SWF hashes. Canonical package checks consume only the release-role/packageable manifest.

### Profile preparation

`prepare-test-profile.ps1` verifies that the explicitly named mod and every required profile mod
exist beneath the same MO2 mods directory and are enabled in the specified profile. Creation and
mod-list editing require explicit switches and a backup. The script does not download or choose
SFSE, Address Library, or another dependency.

### Direct launch and manual validation

1. Launch the manifest's `shortcut` directly.
2. Load the safe Continue save manually unless startup automation itself is under test.
3. Open Absolute Control Panel through its populated PauseMenu entry. Enable a standalone recovery
   hotkey explicitly only when that path is the subject of the run.
4. Exercise the intended UX while the evidence log records lifecycle and semantic commands.
5. Preserve diagnostics locally, then report semantic outcomes without copying local paths.

Manual validation is the authority for slider feel, layout, text alignment, visual hierarchy, and
navigation comfort. Structured events and provider read-back remain the authority for mechanical
behavior.

### Runtime command mailbox

ResearchDev registers an independent two-line mailbox in SFSE's resolved log directory. Use
`invoke-runtime-input.ps1` with an explicit mailbox directory, run ID, evidence file, and one
allow-listed command. The helper assigns an ID atomically and requires accepted and completed
evidence. It does not accept arbitrary keys or code.

The current evidence filename is `AbsoluteControlPanel.evidence.jsonl` for both canonical and
ResearchDev hosts. ResearchDev starts only the bounded title-advance watcher before PostDataLoad;
the remaining experiment services still wait for normal runtime initialization. Runner event
matching parses JSON fields and must not depend on serialization field order.

Current commands are:

- `menu_up`, `nav_down`, `nav_left`, `nav_right`, and `accept`;
- `pause` and `probe_escape`;
- `show_probe` and `hide_probe`;
- `inject_pause_entry`; and
- `probe_pause_root` and `probe_main_root`.

`show_probe` is a research invocation seam, not the product launch design. The product path is the
additive PauseMenu entry. The standalone recovery hotkey is unbound by default.

### Retained PauseMenu cycles

`cycle-pause.ps1` reuses one loaded save to inspect repeated PauseMenu construction and teardown.
This is the efficient lifecycle regression path when the DLL and SWF are unchanged. Plugin or SWF
changes still require a Starfield restart because both binaries are cached by the process.

`run-probe.ps1` is the current supervised cold-start cycle. Invoke `run-probe.cmd` (or explicitly
use Windows PowerShell) with an ignored local manifest; the embedded `System.Drawing` helper is not
PowerShell 7 compatible. It validates the ResearchDev manifest, deploys
the exact DLL/SWF pair, launches the shortcut, advances the title screen through bounded commands,
loads the retained safe save, opens the native movie, and requires structural
`bridge_model_applied` plus populated `bridge_model_published` evidence. Manual mode returns with
the game retained; `visibleMilliseconds` remains bounded to 15 minutes so the ResearchDev watchdog
still provides eventual recovery. A magenta pixel threshold of zero disables the archived visual
sentinel requirement; semantic movie/bridge evidence remains authoritative.

The lifecycle-owned implementation has completed 25 consecutive isolated cycles. New runtime
support must repeat that test according to [the update runbook](RUNTIME-UPDATE-RUNBOOK.md).

## Current process versus archived v1

`tools/process/validate-current.cmd` is the maintained process. On the audited tree it passed the
release build, 9/9 native tests, 7 SDK tests, generated fixture check/compile, 26-entry catalogue,
ten-source SWF provenance, artifact fixtures, canonical manifest, and compatibility ZIP. It always
reports runtime/UX `not_run`.

The disposable builder-v1 contract lives under `tools/process/legacy/v1` and requires explicit
opt-in. Root-level builder phase commands remain only to interpret historical run artifacts.
`run-probe.ps1` is current ResearchDev runtime tooling but is not part of the automated product
pass gate: it launches the local game/profile, uses research-only bounded input and screenshots,
and still requires human judgment for UX. Its semantic model/movie checks do not restore or depend
on the archived permanent magenta sentinel.

Do not translate a v1 phase/result into current product evidence. Do not restore a permanent
magenta square to satisfy its old oracle; a future visual oracle must be explicit development-only
and can prove visible presence, not semantics.

## Evidence and diagnostics

The JSONL evidence stream records wall-clock time, monotonic time, sequence, thread ID, run
ID, event, and bounded detail. Useful authorities include registration, movie/bridge readiness,
PauseMenu boundary/listener/injection, menu lifecycle, pointer down/up, semantic bridge commands,
provider results, and mailbox accepted/completed events.

Producers enqueue into a bounded asynchronous single-writer sink and do no evidence-file I/O or
disk wait from UI/render callbacks. Product defaults to 4,096 outstanding records; ResearchDev uses
16,384 and trace level. New records are dropped at capacity, and accepted/written/dropped/I/O-error
statistics are observable. Owners are intentionally process-lived to avoid loader-lock teardown;
explicit flush/shutdown is for controlled tests, not dynamic plugin unload.

Screenshots, window metadata, crash dumps, process IDs, and logs live only in ignored local artifact
directories. Screenshots help diagnose unexpected dialogs and layout failures; they do not replace
semantic or lifecycle evidence. Audio cues may help a supervising human but never decide pass/fail.

## Toolchain

The interface build uses Apache Flex 4.16.1 (`mxmlc` build 20171115), Flash Player 11.5, SWF
version 18, and PlayerGlobal commit `fef560243029214656d83fc673be0267a1ea0816`. Bootstrap verifies
the pinned checksums. Downloaded tools stay under ignored `.tools/`. Compiler metadata may vary, so
the complete ordered source inventory, `sourceTreeSha256`, output hash, and pinned inputs are the
authorities; the root-only source hash is deprecated compatibility metadata. Byte-identical SWFs
across compiler environments are not claimed.

## Privacy and handoff

Local manifests use `*.local.json` and remain ignored. A handoff reports versions, artifact hashes,
semantic results, failure conditions, and evidence event counts without reporting drive-qualified
paths, account names, mod-list locations, screenshots, device identities, or tokens.

For provider adaptation, follow [the AI integration harness](AI-INTEGRATION-HARNESS.md). For a game
patch, follow [the runtime update runbook](RUNTIME-UPDATE-RUNBOOK.md).
