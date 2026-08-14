# Starfield runtime update runbook

> **Status:** Current maintenance procedure. This runbook is designed to be handed to a capable
> builder agent when the maintainer is unavailable. It does not authorize publishing or uploading
> a release without the repository owner's normal review policy.

The host uses runtime-specific menu mappings. A Starfield patch is therefore a deliberate
revalidation event, even when the public provider ABI and SWF do not change.

## Inputs supplied locally

- Updated Starfield executable and its exact version.
- Matching SFSE build.
- Matching Address Library database supplied or selected by the maintainer/user.
- The isolated MO2 test profile and ignored local manifest.
- A safe Continue save and the direct SFSE launcher shortcut.
- Optionally, a heavily modified compatibility profile after isolated validation passes.

Never download or guess a dependency version silently. Never commit executable paths, Address
Library files, game binaries, logs, screenshots, or the local manifest.

## Phase 0 — Preserve and identify

1. Create a dedicated branch such as `agent/runtime-1-xx-xxx` from the last known-good host
   checkpoint.
2. Record the previous and new game, SFSE, Address Library, CommonLibSF, host, and SWF versions in
   a local result file under ignored artifacts.
3. Build and run headless tests before changing mappings. A source/toolchain failure is distinct
   from a runtime incompatibility.
   Use `tools/process/validate-current.cmd` for the canonical automated baseline; it must leave
   runtime/UX `not_run` rather than converting a build pass into support evidence.
4. Confirm the gameplay providers still load with Absolute Control Panel removed. The update must
   not turn the optional host into a daughter-plugin failure.

## Phase 1 — Fail closed on the new runtime

1. Do not add the new runtime to plugin compatibility metadata merely to make SFSE load it.
2. Audit every symbol and layout in `docs/COMMONLIBSF-COMPATIBILITY.md`,
   `src/runtime/RuntimeCompatibility.cpp`, the generated IDs overlay, menu object size,
   menu flags/priority, active-menu insertion callsite/target, and PauseMenu model path.
3. Treat unchanged numeric IDs as unverified until their new-runtime addresses and behavior are
   correlated. Never copy an old RVA into a new version table by assumption.
4. If any required mapping cannot be proven, leave the host unsupported on that runtime and report
   the exact missing seam. Providers must remain usable without it.

## Phase 2 — Recover and corroborate mappings

Use the narrowest reliable evidence available: the new Address Library, executable disassembly,
shipped menu constructors/vtables, and controlled runtime traces. Cheat Engine may be used for
local reference and breakpoints, but an address found by one live scan is only a candidate.

For each required function or layout:

1. Identify at least one structural anchor: vtable slot, shipped derived constructor, callsite,
   function shape, or invariant data access.
2. Correlate the Address Library ID with the new executable address.
3. Compare multiple shipped menus where the seam is shared.
4. Record ID, RVA, correlation method, and validation result in the compatibility document.
5. Add an exact source/build anchor so a changed dependency fails configuration rather than
   silently applying an overlay.

At minimum revalidate:

- `GameMenuBase` construction and object size;
- required `IMenu` virtuals and `LoadMovie`;
- menu factory ownership/reference behavior;
- `UI::IsMenuOpen` and UI message show/hide;
- active-menu insertion and render-order storage;
- PauseMenu flags, priority, root/model path, population boundary, and selection event;
- cursor and input ownership; and
- Scaleform bridge creation and callback order.

## Phase 3 — Implement the version bridge

1. Keep version-specific mappings explicit; do not weaken exact checks into broad signatures merely
   to pass a new build.
2. Preserve the public provider ABI unless a provider-facing semantic change is truly required.
3. Update plugin runtime metadata only after every mandatory mapping is corroborated.
4. Build the source SWF from the pinned toolchain and validate the complete ordered ActionScript
   source tree, `sourceTreeSha256`, output hash, and compiler inputs.
5. Build canonical `AbsoluteControlPanel` and generate its release-role/packageable manifest.
   If research automation is required, separately build `AbsoluteControlPanelResearchDev` and its
   non-packageable research manifest. Never co-load, rename, or package the ResearchDev host.
6. Run `tools/process/validate-current.cmd`; also build/test ResearchDev boundaries when research
   sources or the runtime harness changed.
7. Update `COMMONLIBSF-COMPATIBILITY.md`, `CURRENT-STATE.md`, `TEST-MATRIX.md`, and the catalog in
   the same branch.

## Phase 4 — Validation ladder

Stop at the first unsafe layer; do not use a heavy profile to discover basic ownership corruption.

1. **Automated:** the current product validator passes build, 8 native test targets, 6 SDK tests,
   codegen/compile fixture, catalogue, complete SWF provenance, artifact fixtures, canonical
   manifest, and package checks. Record actual counts if they change; runtime/UX remains not run.
2. **Host absent:** Head Tracking and AbsoluteZero still load and retain legacy configuration.
3. **Registration only:** launch isolated profile, confirm plugin load, factory registration, and
   no diagnostic dialog or crash without opening the panel.
4. **F2 lifecycle:** open/close repeatedly, verify bridge model, input ownership, and return to
   gameplay.
5. **PauseMenu composition:** require populated vanilla entries, append once, activate, return, and
   repeat at least 25 cycles in one retained save.
6. **Standard controls:** toggle, Apply, Cancel, binding chord, wheel in both directions, and slider
   drag with reversal/release.
7. **Multiple providers:** Head Tracking plus AbsoluteZero, including persistence and host removal.
8. **Display/input:** keyboard, mouse, controller, supported resolutions, UI scale, and alt-tab.
9. **Compatibility profile:** repeat entry/open/close and representative edits in the maintained
   heavy UI/mod profile.
10. **Failure injection:** missing/corrupt SWF, incompatible ABI, provider rejection/failure, and
    stale Address Library behavior all fail without harming vanilla menus or gameplay providers.
    Include wrong/missing bridge root, abnormal external menu teardown while dirty, callback-active
    and dirty unregister/retry, and verify canonical fail-closed hide behavior.

Preserve structured evidence and Windows crash/dump observations locally. Human review is required
for layout and interaction feel; screenshots are diagnostic evidence, not a substitute for bridge
and lifecycle events.

## Phase 5 — Handoff and publication

The update PR or fork must report:

- old and new runtime/dependency versions;
- every changed and unchanged-but-revalidated mapping;
- exact tests and runtime scenarios completed;
- artifact hashes used for runtime evidence;
- known gaps and failure behavior;
- provider ABI/schema impact, if any;
- privacy scan result; and
- whether the heavy compatibility profile was tested.
- canonical DLL/SWF/manifest hashes and confirmation that no ResearchDev artifact entered a package.

Do not claim runtime support from a successful compile, first menu open, or one Address Library
lookup. A release-support claim requires the validation ladder appropriate to the changed seams.
