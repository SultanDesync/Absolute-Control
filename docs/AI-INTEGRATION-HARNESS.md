# AI-assisted provider integration harness

> **Status:** Current provider-integration workflow. Automated product checks are scripted;
> runtime/UX steps remain supervised. Archived builder v1 is not a current dependency.

This workflow is designed so an AI coding agent can add Absolute Control Panel configuration
support to an existing SFSE module with minimal user intervention. The gameplay module must
remain fully usable if Absolute Control Panel is missing, incompatible, or fails to render.

For a new integration, begin with [current implementation state](CURRENT-STATE.md),
[the module API](MODULE-API.md), and [the menu-definition SDK](../sdk/README.md), then follow the
workflow below. `BUILDER-RUNBOOK.md` preserves the first disposable SLOP/AbsoluteZero experiment;
it is historical evidence, not current builder instruction.

AI generation is a declared project method, not an embarrassing implementation detail.  The
harness exists to turn fast generation into reviewable evidence: stable contracts, negative
tests, isolated game runs, visual oracles, preserved failure diagnostics, and privacy gates.
Debate about authorship is not a release gate; demonstrated behavior and maintainability are.

## One-time human setup

The user supplies only environment facts that must stay local:

- the mod's source repository;
- the isolated Mod Organizer 2 deployment directory;
- the SFSE launcher shortcut;
- the matching Address Library file;
- a save that can be continued safely; and
- optionally, a known test device when controller binding capture is under development.

These values belong in an ignored `*.local.json` manifest.  They must never appear in source,
documentation, screenshots committed to Git, or a pull-request description.  The harness
derives Program Files and other system locations at runtime.

The user should only need to intervene when Starfield presents an unexpected diagnostic dialog,
the selected save cannot load, a prerequisite is absent, or a design choice changes actual
module behavior.

## Agent workflow

### 1. Inventory the provider

The agent identifies each existing setting, its stable ID, type, range, step, current value,
validation rule, persistence owner, restart requirement, and any preview or reset behavior.  It
does not copy ImGui rendering code into the provider adapter.  It maps existing module state to
renderer-neutral descriptors and callbacks.

The inventory also records the module's known-good compiler environment, configure/build preset
or command, test command, and artifact path. The builder reuses that recipe by default. Absolute
Control Panel's documented MSVC/xmake/CMake baseline is a recommended fallback, not a requirement
that a working project replace its own build system.

### 2. Add a fail-optional adapter

The agent includes `AbsoluteControlPanelAPI.h`, dynamically resolves
`AbsoluteControlPanel_QueryApi`, registers pages, and preserves the existing configuration path.
Host absence is a normal state, not a plugin load failure. Callback code catches all internal
failures and returns an API `Result`; no C++ exception crosses the DLL boundary.

### 3. Run headless contract checks

Before launching Starfield, the agent verifies:

- all ABI structures have the expected `structSize` and version;
- module/page IDs are stable and unique; raw ABI control IDs are unique within a page, while the
  generated SDK requires module-wide uniqueness for its shared callback/parser model;
- every descriptor type matches the values returned by `readValue`;
- invalid IDs, invalid types, and out-of-range writes are rejected;
- cancel restores the committed snapshot;
- apply reaches the module's existing persistence layer; and
- the module still builds and starts with no Absolute Control Panel binary present.

The test should use a temporary configuration location and compare semantic values, not
machine-specific absolute paths.

### 4. Run the isolated in-game proof

First run `tools/process/validate-current.cmd`; record its automated result while leaving runtime
and UX `not_run`. The in-game proof is a separate supervised workflow assembled from ResearchDev
manifest deployment, direct shortcut launch, bounded mailbox commands, structured evidence, and
human UX validation. Archived builder-v1/`run-probe.ps1` sentinel results are not current proof.
Follow [the harness status](RESEARCH-HARNESS.md) and perform these gates:

1. build/deploy the non-packageable ResearchDev host, provider, and source-controlled SWF from one
   validated manifest; never mix it with the canonical host;
2. launch the ignored manifest's SFSE shortcut directly, without navigating MO2 or using general
   desktop control;
3. load the safe test save manually or through bounded game-thread input when that path is under
   test;
4. open and close PauseMenu as a synchronization check;
5. require PauseMenu to be closed before showing Absolute Control Panel;
6. require bridge-model and menu-lifecycle acknowledgements;
7. exercise representative controls through native keyboard and mouse input, with a human judging
   layout and interaction feel;
8. record a keyboard chord and require the normalized binding to round-trip;
9. validate provider-owned persisted values;
10. close Absolute Control Panel and require gameplay to resume with PauseMenu still closed; and
11. preserve ignored logs and screenshots on both success and failure.

The runtime input mailbox is independent from automatic menu opening and lives in SFSE's resolved
log directory so it remains writable and visible after MO2 has launched. Use
`tools/research/invoke-runtime-input.ps1` to write an atomic command and require both acceptance
and completion evidence. The evidence stream includes an ordered sequence, monotonic timestamp,
and Windows thread ID. PauseMenu mutation is permitted only after the engine's active-menu
insertion boundary and from that movie's own Scaleform advance callback; no worker may poll a
foreign movie.

Structured plugin events and semantic read-back are mechanical pass/fail authorities. A narrow
pixel oracle may be added when the current movie deliberately exposes one, but the removed magenta
sentinel is not evidence. A vision model is reserved for failure screenshots, layout review, and
unexpected dialogs; routine success does not spend vision-model tokens.

### 5. Prove isolation and privacy

The agent disables or removes Absolute Control Panel from the isolated profile and confirms the provider gameplay
plugin still loads.  Before commit it scans staged text for usernames, drive-qualified local
paths, process IDs, screenshot paths, device serials, tokens, and generated research artifacts.
Only placeholder manifests may be committed.

### 6. Open a draft integration PR

The PR reports the ABI version, registered pages and controls, validation behavior, automated and
manual evidence, known gaps, and rollback behavior. It must not claim full MCM readiness until dynamic
rendering, a product invocation route, controller navigation, dirty-state UX, current harness
automation, and multi-provider composition are separately proven.

## Definition of done for one provider

A provider integration is complete when it registers without linking gameplay to the host,
round-trips every supported setting type, uses the module's existing validation and persistence,
survives host absence, passes the supervised isolated in-game proof, and introduces no local-machine data
into Git.  The current synthetic provider is the reference fixture, not a product dependency.
