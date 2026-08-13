# SDK status and release checklist

Absolute Control Panel has a working provider ABI and integration harness, but the SDK is not yet
frozen or packaged as a supported developer release. This index separates current contracts from
historical research instructions and records what must be true before an SDK tag is published.

## Authoritative development sources

- `include/AbsoluteControlPanelAPI.h` — current public C ABI candidate.
- `docs/MODULE-API.md` — ownership, discovery, transactions, and failure behavior.
- `sdk/menu-definition.schema.json` — strict authoring schema.
- `sdk/tools/menu_codegen.py` — deterministic descriptor generator.
- `sdk/examples/absolute-head-tracking.menu.json` — reference definition.
- `catalog/catalog.json` — machine-readable component capability and confidence catalogue.
- `docs/AI-INTEGRATION-HARNESS.md` — provider inventory, implementation, and validation workflow.
- `docs/TEST-MATRIX.md` — runtime evidence and remaining compatibility coverage.

Documents under `docs/process/` and the historical builder runbook describe disposable SLOP-era
experiments. They remain useful provenance but are not instructions for a new integration.

## Current compatibility policy

The product-named `AbsoluteControlPanel_QueryApi` is the preferred discovery export. The temporary
research DLL name and `SLOP_QueryApi` remain available for the already-built experimental
AbsoluteZero adapter. Removing either alias requires a migration release and an updated subscriber
beforehand.

Subscriber gameplay must remain fully operational when the host is absent, incompatible, or
rejects registration. Existing Workbench, Dear ImGui, INI, and hotkey paths may coexist during the
experimental period; the host is not a loader dependency.

## Required before the first SDK release

- Choose the permanent DLL filename, query export, namespace, and module identity.
- Freeze ABI v1 structure sizes, flags, capacities, calling conventions, and compatibility rules.
- Decide whether choice labels, text editing, sections, and presentation hints extend ABI v1 or
  require ABI v2.
- Promote or remove the experimental live/compound-component protocol.
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

## Checkpoint discipline

Each subscriber integration checkpoint records its host ABI, registered modules/pages, retained
fallback UI, build/test commands, runtime validation, and known limitations in that subscriber's
README. Every accepted host capability updates the component catalogue and test matrix in the same
change so SDK documentation follows executable behavior rather than anticipated behavior.
