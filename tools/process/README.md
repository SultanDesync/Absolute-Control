# Product validation process

This directory has one current product-validation entry point:

```powershell
.\tools\process\validate-current.cmd
```

`validate-current` executes the machine-readable gates in `current-process.json`. It builds and
tests the canonical product, validates the SDK generator and compile fixture, validates the
component catalogue, checks Scaleform source/dist provenance, exercises the artifact tooling,
creates and validates the canonical release-role manifest, and validates the compatibility
package. It writes an ignored JSON report beneath `artifacts/process` by default.

Passing this command means **the automated gates passed**. It never means that the release is
runtime- or UX-verified. The report always leaves the in-game and human-judgment gates at
`not_run`; complete those from `docs/TEST-MATRIX.md` and record their evidence separately.
Screenshots and pixel signals may document appearance or menu presence, but cannot prove semantic
behavior such as callback delivery, persistence, rollback, input ownership, or crash freedom.

The current validator does not launch Starfield, modify an MO2 profile, or deploy a mod. Those are
separate, explicitly initiated research activities. It contains no local account, drive, mod-list,
shortcut, or device paths.

Run the lightweight definition tests without compiling the project:

```powershell
.\tools\process\tests\Test-CurrentProcess.cmd
```

## Archived builder-process v1

The following root-level tools are the **legacy-v1 disposable SLOP builder experiment**, retained
to reproduce and interpret historical `artifacts/builder-runs` evidence:

- `new-builder-run.cmd`
- `new-phase-prompt.cmd`
- `evaluate-phase.cmd`
- `evaluate-builder-run.cmd`
- `discard-builder-run.cmd`
- `builder-result.schema.json`
- `fixtures/`

They are not product validation and must not be selected by a new agent. Their PowerShell entry
points fail closed unless the caller supplies `-AllowLegacyV1`. Phase 03's magenta framebuffer
sentinel remains only as an archived v1 criterion; it is absent from the current process contract.
See `legacy/v1/README.md` and `legacy/v1/contract.json` for provenance and version boundaries.

The `build-host`, `build-subscriber`, and `build-interface` wrappers are implementation helpers
from that experiment. They are not an alternative current checklist and confer no runtime claim.
