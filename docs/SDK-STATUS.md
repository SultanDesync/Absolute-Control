# SDK status and release checklist

> **Status:** `0.2.0-beta.1` coordinated private beta. The public SDK and experimental APIs are not
> frozen. Beta applicants contact the author so integrations and deployment baselines remain
> reviewable.

Absolute Control has a working provider ABI, four deployed suite integrations, a menu-definition
generator, an integration harness, and a versioned private-beta kit. The stable configuration query
remains ABI version 1 with append-only size/capability gates. Live components and semantic
composition remain separately negotiated experimental APIs. This index records what must be true
before a public SDK tag is published.

The current beta baseline is the Absolute Control artifact in Absolute Suite `0.3.0-beta.1`.
Exact plugin and interface hashes are recorded in that release's `AbsoluteSuite.manifest.json`.

## Authoritative development sources

- `docs/CURRENT-STATE.md` — authoritative capability status and known limitations.
- `docs/DECISIONS.md` — accepted and provisional architectural decisions.
- `include/AbsoluteControlPanelAPI.h` — current public C ABI candidate.
- `include/LiveComponentsExperimentalAPI.h` — active bounded live/compound ABI candidate; name
  retained until performance and accessibility qualification.
- `include/AbsoluteControlCompositionExperimentalAPI.h` — active C2 semantic composition
  candidate; status/card/row/condition/anchor and same-page live-association capabilities are
  currently advertised.
- `docs/MODULE-API.md` — ownership, discovery, transactions, and failure behavior.
- `sdk/menu-definition.schema.json` — strict authoring schema.
- `sdk/tools/menu_codegen.py` — deterministic descriptor generator.
- `sdk/examples/absolute-head-tracking.menu.json` — reference definition.
- `sdk/CHANGELOG.md` — additive capability and migration notes.
- `sdk/VERSION` and `sdk/package-files.json` — private-beta package identity and exact source list.
- `sdk/BETA-ACCESS.md` — application, privacy, fallback, and coordinated deployment rules.
- `sdk/integration-registry.json` — shipped and accepted integrations with consumed header hashes.
- `catalog/catalog.json` — machine-readable component capability and confidence catalogue.
- `docs/SUBSCRIBER-UI-STANDARD.md` — normative Level A/B/C subscriber acceptance contract.
- `docs/SDK-RELEASE-PLAN.md` — audited subscriber order, SDK gaps, freeze gates, and rollout waves.
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

The optional `ApiV1::requestOpenPage` tail follows the same rule. Subscribers must size-check the
table and feature-detect `kCapabilityPageOpenRequests`; `Ok` only acknowledges an asynchronous
validated route. Any unavailable or unsuccessful result retains the subscriber's legacy menu.

## Current deployed compatibility baseline

AbsoluteHOTAS consumes the current configuration, live, and C2 composition headers. Absolute Power,
Absolute Head Tracking, and AbsoluteZero intentionally consume older append-compatible ABI-v1
generations proven with the shipped host. Their exact header hashes and bundled commits are recorded
in `sdk/integration-registry.json`. Updating the SDK must not trigger blind header replacement or a
standalone-module rebuild.

The private-beta kit is for individually coordinated integrations. It is not a Nexus optional file,
does not promise long-term source compatibility for the experimental APIs, and does not make
Absolute Control a loader dependency. Each applicant must retain provider-owned configuration,
host-absent operation, and a fallback UI while the SDK is in beta.

## Required before the first public SDK release

- Freeze ABI v1 structure sizes, flags, capacities, calling conventions, and compatibility rules.
- Freeze the appended ABI-v1 labeled-choice callback and bounded `TextInput` semantics, then add
  deterministic option-kind schema/code-generator coverage. Structured section headers and inline
  actions are now generated through the capability-gated ABI-v1 extension.
- Qualify the appended fixed-capacity `RecordCollection` callback and host-owned Action
  confirmation with the HOTAS profiles/layers, macros, and device/calibration journeys. Automated
  host, bridge, renderer, and generator coverage exists; in-game pointer/controller UX remains a
  freeze gate.
- Freeze the appended provider-owned binding-capture callback set after the accepted Head Tracking
  and Absolute Power Input Bus journeys. Before freeze, standardize clear/unbound semantics; the
  provider-reported Reassign/Cancel collision handshake now has deterministic host and SDK
  coverage. Keep device libraries outside the host.
- Rename/freeze or revise the active live/compound-component protocol after Absolute Power and
  AbsoluteHOTAS performance/accessibility qualification. The row-to-Choice association now uses a
  capability-gated live-channel tail without changing the committed v1 column-array stride.
- Qualify the experimental semantic-composition protocol through HOTAS Flight Axes and at least
  one second subscriber before promoting any structures to the stable API. C2 has synthetic
  native/bridge/renderer/input/performance coverage plus complete HOTAS Flight Axes descriptor,
  telemetry, draft, and fallback coverage. In-game visual, three-input-mode, physical capture,
  long-label, accessibility, and teardown evidence remains required.
- Promote the private-beta package boundary only after its public header, schema, generator,
  examples, licence/notice files, and version notes have survived an outside integration without
  acquiring research-only sources.
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

The suite release baseline was built from clean component repositories and the final FOMOD passed
its exact 14-entry archive and hash validation. The user then validated the suite installer in both
MO2 and Vortex. Those results establish the beta host/deployment baseline; they do not freeze the
public SDK or replace per-integration runtime, fallback, and persistence acceptance.
