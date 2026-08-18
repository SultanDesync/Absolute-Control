# SDK status and release checklist

> **Status:** Current SDK-readiness index. The ABI and tooling are experimental and not frozen.

Absolute Control Panel has a working provider ABI and integration harness, but the SDK is not yet
frozen or packaged as a supported developer release. This index separates current contracts from
historical research instructions and records what must be true before an SDK tag is published.

## Authoritative development sources

- `docs/CURRENT-STATE.md` — authoritative capability status and known limitations.
- `docs/DECISIONS.md` — accepted and provisional architectural decisions.
- `include/AbsoluteControlPanelAPI.h` — current public C ABI candidate.
- `include/LiveComponentsExperimentalAPI.h` — active bounded live/compound ABI candidate; name
  retained until performance and accessibility qualification.
- `docs/MODULE-API.md` — ownership, discovery, transactions, and failure behavior.
- `sdk/menu-definition.schema.json` — strict authoring schema.
- `sdk/tools/menu_codegen.py` — deterministic descriptor generator.
- `sdk/examples/absolute-head-tracking.menu.json` — reference definition.
- `sdk/CHANGELOG.md` — additive capability and migration notes.
- `catalog/catalog.json` — machine-readable component capability and confidence catalogue.
- `docs/AI-INTEGRATION-HARNESS.md` — provider inventory, implementation, and validation workflow.
- `docs/TEST-MATRIX.md` — runtime evidence and remaining compatibility coverage.
- `docs/RUNTIME-UPDATE-RUNBOOK.md` — version-update recovery and validation procedure.
- `docs/ARCHITECTURE.md` — source ownership, dependency direction, and lifecycle assumptions.
- `docs/DEBT-REGISTER.md` — resolved audit findings and remaining release debt.

Documents under `docs/process/`, the historical builder runbook, and SLOP-named research passages
describe disposable experiments. They remain useful provenance but are not instructions for a new
integration. `CURRENT-STATE.md` wins when a historical claim conflicts with current behavior.

## Current compatibility policy

The canonical host is `AbsoluteControlPanel.dll`; `AbsoluteControlPanel_QueryApi` and
`include/AbsoluteControlPanelAPI.h` are the product discovery/type authorities. `SlopAPI.h` aliases
shared types/constants/callbacks and preserves only the original table prefix and `SLOP_QueryApi`
for already-built experimental subscribers. `AbsoluteControlPanelResearchDev.dll` is a separate,
non-packageable research host. Legacy export/table removal requires a migration checkpoint.

The query table is discoverable during initialization. Providers retry `NotReady` from
registration/refresh and treat terminal `Rejected` as non-fatal host unavailability. Descriptors
are copied; callback/context lifetime continues until unregister succeeds or process exit.
Unregister returns retryable `Rejected` while a callback lease or dirty transaction is active.

Host limits are 512 modules, 2,048 pages globally, 32 pages per module, 128 controls per page, 512
controls per module, and 32,768 controls globally. Raw ABI IDs are page-local because pages may have
distinct contexts/callbacks. Generated pages share one `ProviderCallbacks` set and module-wide
parser, so generated definitions require module-wide unique control IDs. The generator rejects a
definition above the 512-control module limit; a shared machine-readable limits authority remains
an SDK-freeze maintenance task.

Subscriber gameplay must remain fully operational when the host is absent, incompatible, or
rejects registration. Existing Workbench, Dear ImGui, INI, and hotkey paths may coexist during the
experimental period; the host is not a loader dependency.

## Required before the first SDK release

- Freeze ABI v1 structure sizes, flags, capacities, calling conventions, and compatibility rules.
- Freeze the appended ABI-v1 labeled-choice callback and bounded `TextInput` semantics, then add
  deterministic option-kind schema/code-generator coverage. Structured section headers and inline
  actions are now generated through the capability-gated ABI-v1 extension.
- Freeze the appended provider-owned binding-capture callback set after the accepted Head Tracking
  and Absolute Power Input Bus journeys. Before freeze, standardize clear/unbound semantics; the
  provider-reported Reassign/Cancel collision handshake now has deterministic host and SDK
  coverage. Keep device libraries outside the host.
- Rename/freeze or revise the active live/compound-component protocol after Absolute Power and
  AbsoluteHOTAS performance/accessibility qualification.
- Package the public header, schema, generator, examples, licence/notice files, and version notes
  without research-only sources.
- Provide CMake and xmake consumption examples while allowing subscribers to keep an existing
  compiler preset and build system.
- Run ABI layout tests with at least the Head Tracking and AbsoluteZero subscribers.
- Prove host absence, incompatible-version rejection, Apply/Cancel rollback, binding capture, and
  provider-owned persistence in the isolated game harness.
- Complete the privacy gate: no local paths, mod-list locations, account names, logs, screenshots,
  tokens, or device-specific identifiers in the package or repository diff.
- Publish an upgrade/migration note for every future ABI or schema change.
- Source generator/catalog/host limits from one machine-readable authority to prevent drift.

## Checkpoint discipline

Each subscriber integration checkpoint records its host ABI, registered modules/pages, retained
fallback UI, build/test commands, runtime validation, and known limitations in that subscriber's
README. Every accepted host capability updates the component catalogue and test matrix in the same
change so SDK documentation follows executable behavior rather than anticipated behavior.

The current product validator passes the canonical build, 9/9 native tests, 8 generator tests,
generated fixture check/compile, 26 catalogue entries, 15-source SWF provenance, artifact
fixtures, canonical manifest, and compatibility ZIP. Runtime/UX is `not_run`; this is not an SDK
release or support declaration.
