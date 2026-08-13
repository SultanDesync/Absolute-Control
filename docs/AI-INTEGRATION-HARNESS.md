# AI-assisted provider integration harness

This workflow is designed so an AI coding agent can add SLOP configuration support to an
existing SFSE module with minimal user intervention.  The gameplay module must remain fully
usable if SLOP is missing, incompatible, or fails to render.

For an autonomous first-implementation run, use
[the builder runbook](BUILDER-RUNBOOK.md) as the single entry point. The broader document below
describes the long-term integration policy; it is not intended to be pasted wholesale into every
builder prompt.

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
- optionally, vJoy device 1 for deterministic button-capture tests.

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
or command, test command, and artifact path. The builder reuses that recipe by default. SLOP's
documented MSVC/xmake/CMake baseline is a recommended fallback, not a requirement that a working
project replace its own build system.

### 2. Add a fail-optional adapter

The agent includes the public API header, dynamically resolves `SLOP_QueryApi`, registers pages,
and preserves the existing configuration path.  SLOP absence is a normal state, not a plugin
load failure.  Callback code catches all internal failures and returns an API `Result`; no C++
exception crosses the DLL boundary.

### 3. Run headless contract checks

Before launching Starfield, the agent verifies:

- all ABI structures have the expected `structSize` and version;
- module, page, and control IDs are non-empty, stable, terminated, and unique;
- every descriptor type matches the values returned by `readValue`;
- invalid IDs, invalid types, and out-of-range writes are rejected;
- cancel restores the committed snapshot;
- apply reaches the module's existing persistence layer; and
- the module still builds and starts with no SLOP binary present.

The test should use a temporary configuration location and compare semantic values, not
machine-specific absolute paths.

### 4. Run the isolated in-game proof

The existing bounded runner performs the expensive validation:

1. build and deploy the host, provider, and source-controlled SWF;
2. launch through SFSE;
3. use guarded game-thread input to continue the last save;
4. open and close PauseMenu as a synchronization check;
5. require PauseMenu to be closed before showing SLOP;
6. require the magenta pixel sentinel and bridge snapshot acknowledgement;
7. exercise representative controls through native keyboard input;
8. pulse vJoy and require the enumerated binding to round-trip;
9. validate provider-owned persisted values;
10. close SLOP and require gameplay to resume with PauseMenu still closed; and
11. preserve ignored logs and screenshots on both success and failure.

Pixel thresholds and structured plugin events are pass/fail authorities.  A vision model is
reserved for failure screenshots, layout review, and unexpected dialogs; routine success does
not spend vision-model tokens.

### 5. Prove isolation and privacy

The agent disables or removes SLOP from the isolated profile and confirms the provider gameplay
plugin still loads.  Before commit it scans staged text for usernames, drive-qualified local
paths, process IDs, screenshot paths, device serials, tokens, and generated research artifacts.
Only placeholder manifests may be committed.

### 6. Open a draft integration PR

The PR reports the ABI version, registered pages and controls, validation behavior, automated
evidence, known gaps, and rollback behavior.  It must not claim full MCM readiness until dynamic
rendering, a product invocation route, controller navigation, dirty-state UX, and multi-provider
composition are separately proven.

## Definition of done for one provider

A provider integration is complete when it registers without linking gameplay to the host,
round-trips every supported setting type, uses the module's existing validation and persistence,
survives host absence, passes the isolated in-game runner, and introduces no local-machine data
into Git.  The current synthetic provider is the reference fixture, not a product dependency.
