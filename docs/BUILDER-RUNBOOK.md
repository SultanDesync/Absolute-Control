# Historical builder run: first external SLOP page

> **Archived v1:** This records the disposable SLOP/AbsoluteZero experiment and its former magenta
> criterion. It is not current product validation. Use `tools/process/validate-current.cmd` and
> [the AI integration harness](AI-INTEGRATION-HARNESS.md); runtime/UX remains a separate manual
> evidence run. Reproducing v1 requires the explicit legacy opt-in documented under
> `tools/process/legacy/v1`.

> This document preserves the disposable experiment that produced the first external subscriber.
> **Status: Historical.** It is evidence, not current SDK guidance. New integrations use
> [`AbsoluteControlPanelAPI.h`](../include/AbsoluteControlPanelAPI.h), the
> [AI integration harness](AI-INTEGRATION-HARNESS.md), and the
> [menu-definition SDK](../sdk/README.md). Legacy SLOP names below are retained so the recorded
> experiment remains reproducible.

This is the single entry point for an autonomous builder. Do not begin by reading every research
document. Follow the phases below and open a referenced document only when that phase needs it.

The objective of a builder run is to measure whether this runbook can produce a working vertical
slice with few interventions. Code produced in an evaluation worktree is disposable evidence; it
is not presumed suitable for merge.

This runbook builds SLOP as the general host API and renderer. AbsoluteZero is only the first
external fixture used to prove that the host contains no daughter-module policy. Do not optimize
the host around AbsoluteZero or treat the subscriber adapter as the product.

The builder must rely only on this run's supplied specification snapshot and repository contents.
Do not use another builder's patch, report, conversation, or remembered implementation as input.
The evaluator will discard rejected work under
[the disposable-iteration policy](process/DISPOSABLE-ITERATIONS.md). Independently accepted SLOP
host/API phases may be promoted by the maintainer and become the cumulative product baseline.

## Fixed objective

Build one descriptor-driven SLOP page supplied by an independently compiled AbsoluteZero plugin.
The page is **Mouse Alignment**. It must render from registered descriptors rather than fixed
control names in the host or SWF.

The run is incomplete until all of these are true:

1. AbsoluteZero dynamically resolves `SLOP_QueryApi(1)` and registers the page without linking to
   the host.
2. Host absence or incompatibility does not prevent AbsoluteZero from loading or using its
   existing INI/hotkeys.
3. SLOP enumerates copied page/control descriptors and serializes them using
   [Bridge Protocol v1](BRIDGE-PROTOCOL-V1.md).
4. ActionScript constructs visible controls from that model. No runtime branch may name
   AbsoluteZero, ResearchModule, `toggleFeature`, `responseLevel`, or the three synthetic control
   IDs.
5. Generic UI messages route by `(moduleId, pageId, controlId)` to `readValue`, `writeDraft`,
   `invokeAction`, `apply`, and `cancel`.
6. Headless tests cover invalid ABI, copied descriptors, unknown IDs, wrong value kinds,
   out-of-range writes, apply, cancel, and host absence.
7. Both repositories build and their tests pass.
8. The builder writes `builder-result.json` at the run root using
   `tools/process/builder-result.schema.json`.

Do not stop after registration, descriptor enumeration, a static UI mock, or a prose plan. If a
gate cannot be completed, continue every independent gate and report the precise blocker.

## Non-negotiable boundaries

- Work only in the two disposable worktrees named in the generated handoff.
- Do not commit, push, open a PR, edit the source repositories, or copy local manifests.
- Do not replace or cover PauseMenu. The research mailbox remains an acceptable invocation seam
  for this evaluation.
- Do not make SLOP a loader dependency of AbsoluteZero.
- AbsoluteZero owns validation, live preview, persistence, and rollback.
- SLOP owns rendering, navigation, dirty-state UX, apply/cancel orchestration, and safe close.
- Never pass STL, CommonLibSF, exceptions, or allocator ownership across the public DLL boundary.
- Generated logs, screenshots, local paths, device data, and build directories stay untracked.

## Phase 0: baseline and inventory

Before editing:

1. Record both baseline commits and initial `git status` values in `builder-result.json`.
2. Require `preflight.json` to show both clean baselines passed. Reuse the supplied
   `build-host.ps1` and `build-subscriber.ps1` wrappers after edits; they select the installed
   Visual Studio developer environment, Ninja, vcpkg root, and pinned project commands. A sandbox
   denial is a permission problem, not evidence that source or the selected toolchain is broken.
   For another subscriber, prefer its documented known-good compiler environment and configure,
   build, and test presets. Use the SLOP-recommended environment only when the project supplies no
   working recipe or when reproducing this exact evaluation baseline.
3. Read [the public API contract](MODULE-API.md) and `include/SlopAPI.h`.
4. Read AbsoluteZero's public API, configuration implementation, policy tests, and existing
   Workbench manifest. Reuse its configuration functions; do not add a second settings store.

## Phase 1: external subscriber

Add the smallest fail-optional adapter to AbsoluteZero:

- Resolve the loaded SLOP module and `SLOP_QueryApi` at runtime after all SFSE plugins load.
- Validate requested ABI, `structSize`, identity, and every function used.
- Register one page and the controls listed below.
- Copy the published `SlopAPI.h` into a clearly vendored SDK location or consume a configured SDK
  include path. Add a layout/ABI drift test; do not maintain a compressed handwritten duplicate.

Mouse Alignment controls, in this order:

| ID | Kind | Range/step | Behavior |
|---|---|---|---|
| `enabled` | Toggle | Boolean | Live preview through existing configuration apply |
| `radius` | FloatSlider | 1–200 / 1 | Live preview |
| `idle-ms` | IntegerSlider | 10–500 / 10 | Live preview |
| `decay-rate` | FloatSlider | 0.5–20 / 0.5 | Live preview |
| `poll-rate-hz` | IntegerSlider | 30–500 / 10 | Live preview |
| `suppress-key` | IntegerSlider | 0–255 / 1 | Advanced; live preview |
| `diagnostic-log` | Toggle | Boolean | Requires restart flag |

On the first draft write, snapshot the committed configuration. `writeDraft` validates the exact
kind/range and calls the existing live `Configuration::Apply`. Page `apply` calls the existing
save path and starts a new clean session. Page `cancel` restores the snapshot through the existing
apply path. Host close with unapplied changes calls `cancel`. Do not add a Save action control;
Apply and Cancel belong to SLOP.

## Phase 2: generic host model

Add a registry snapshot function that copies the current page list while holding the registry
lock, then releases the lock before invoking provider callbacks. Never call a provider while the
registry mutex is held.

Build the exact model in [Bridge Protocol v1](BRIDGE-PROTOCOL-V1.md). Read each control through
its page callback. A failed read marks that control unavailable and records a bounded error; it
must not remove other pages or trap input.

Keep the synthetic provider only as a headless fixture. The runtime renderer and command router
must contain no synthetic IDs or semantics.

## Phase 3: dynamic SWF

Replace the three fixed sprites with a bounded vertical list constructed from `pages[].controls`.
For this evaluation, support Toggle, IntegerSlider, FloatSlider, and Action. Choice and
ButtonBinding may render as unsupported/read-only rows, but must not crash.

Requirements:

- one active page at a time;
- descriptor-order controls;
- selected-row highlight;
- label, current value, and advanced/restart markers;
- scrolling when controls exceed the visible region;
- generic mouse and W/S/E/A/D input;
- Apply, Cancel, and Close buttons;
- the existing magenta sentinel remains present;
- no device font dependency; retain the vector alphabet until a proven font path exists.

## Phase 4: command and transaction tests

Implement the flat command payload exactly as documented. Native code validates schema version,
page identity, control identity, descriptor kind, value kind, finite numeric values, and range
before calling a provider.

Test at least:

- model serialization of two pages and mixed controls;
- copied descriptors surviving source mutation;
- duplicate registration rejection;
- generic Boolean, integer, and float writes;
- kind/range/unknown-control rejection without mutation;
- action routing;
- apply clearing dirty state;
- cancel restoring the committed value;
- close cancelling a dirty page;
- unregister/refresh revision behavior; and
- AbsoluteZero host-absence initialization.

## Phase 5: builds and optional game run

Build the SWF, SLOP plugin/tests, and AbsoluteZero/tests. Record exact commands and outcomes in the
result file. Run the isolated game harness only if an ignored local manifest is already available
to the disposable host worktree or explicitly supplied by the evaluator. Never search broadly for
private environment values.

The in-game pass requires dynamic `Mouse Alignment` labels, successful Boolean/numeric edits,
provider persistence, PauseMenu closed before SLOP opens, and direct return to gameplay.

## Reporting discipline

The result file is part of the experiment. Record:

- documents actually read;
- assumptions and why they were necessary;
- evaluator interventions verbatim by category, excluding secrets;
- every gate as `passed`, `failed`, `blocked`, or `not_run`;
- commands and concise outcomes;
- files changed in each worktree;
- remaining blockers; and
- whether another agent could reproduce the run from committed inputs.

Do not describe a preparatory seam as a completed implementation. A truthful partial result
scores higher than an unsupported completion claim.
