# SDK release and subscriber rollout plan

> **Prepared:** 2026-08-19. This is the execution map for bringing every current subscriber to the
> [subscriber UI standard](SUBSCRIBER-UI-STANDARD.md) and freezing a modular SDK capable of the
> Absolute Power workbench without making Power a hard-coded host special case.

## Audited subscriber baseline

| Subscriber | Current UI role | Conformance target | Next migration focus |
|---|---|---|---|
| AbsoluteZero | compact single-page scalar editor | A | first canary for packaged headers, sections, labels, fallback, and full input traversal |
| Absolute Head Tracking | three-page settings, axes, keyboard and Input Bus capture | B | adopt current structured layout/conflict contract and prove capture/error/read-back matrix |
| Absolute Power | dynamic profile editor plus six-row live allocation grid | C | reference row associations, positional pip edits, transactions, long binding names, and compound performance |
| AbsoluteHOTAS | broad multi-page flight-control suite | A/B per page, C where live telemetry is justified | scale, discoverability, conditional groups, deep-link needs, and controller-only end-to-end acceptance |

The repositories currently vendor different generations of `AbsoluteControlPanelAPI.h`. The rollout
must not normalize them with blind file copies. Each migration records the SDK package identity,
compiles an ABI fixture, and lands independently so unrelated subscriber work remains reviewable.

## SDK product shape

The first release package should contain four deliberate layers:

1. **Stable configuration ABI** — modules, pages, scalar controls, structured layout, dynamic
   choices, text, capture/conflict callbacks, transaction flags, and capability negotiation.
2. **Declarative authoring** — versioned JSON Schema plus deterministic code generation for every
   stable facility, generated compatibility fallbacks, and compile-tested examples.
3. **Compound/live extension** — separately named until freeze, fixed-capacity frames and operations,
   API-table capabilities, explicit row associations, and no renderer-specific coordinates.
4. **Integration kit** — CMake/xmake examples, one minimal Level-A subscriber, one generated Level-B
   subscriber, an Absolute-Power-class Level-C fixture, changelog, migration notes, and licenses.

The SDK version and the C ABI version are different concepts. Package releases may advance while an
ABI remains compatible; an ABI number advances whenever a previously compiled record changes size,
offset, enum meaning, ownership, or callback contract incompatibly.

## Gaps to resolve before freeze

| Gap | Why it matters | Release decision |
|---|---|---|
| status/notice primitive | subscribers currently misuse read-only bindings for prose and diagnostics | add a bounded non-editable text/status kind or explicitly defer it from v1 |
| confirmation metadata | delete/reset actions need consistent host-owned confirmation | specify destructive-action policy before broad subscriber migration |
| conditional visibility/enabling | HOTAS has dependent controls and mode-specific groups | prefer bounded provider predicates/state over arbitrary layout scripting |
| live components in schema/codegen | Power-level UI is handwritten today | generate bounded descriptors, callbacks, capability checks, and fallback controls |
| deep-link/open request | HOTAS and other frontends need a supported route into a module/page/control | design separately from descriptor registration and gameplay input |
| localization | labels are currently provider literals | freeze string ownership and key/catalog strategy before declaring SDK stable |
| packaged dependency identity | copied headers drift silently | ship version metadata and add consumer-side hash/layout fixtures |

## Execution waves

### Wave 0 — contract hardening

- preserve the committed grid-column ABI layout and test its exact sizes;
- size-gate every appended table or descriptor tail and advertise optional live behavior;
- convert implicit layout naming to explicit association records;
- add old-table, old-descriptor, absent-capability, invalid-association, and fallback tests;
- decide the remaining stable primitives above and update schema/codegen together;
- publish the SDK package layout and one migration guide.

Exit: host, SDK, package, and synthetic Level A/B/C fixtures pass from a clean checkout.

### Wave 1 — simple subscriber canaries

Migrate AbsoluteZero first, then Head Tracking. These prove that the package is pleasant for an
ordinary subscriber and that capture/conflict behavior is reusable without the compound API.

Exit: both meet their conformance checklists, use the packaged SDK identity, pass host-absent tests,
and complete mouse, keyboard, and controller runtime journeys.

### Wave 2 — Absolute Power reference workbench

Finish the final profile hierarchy, 12-pip allocation grid, row-associated priority choices,
keyboard/HOTAS bindings, conflict handling, Apply/Cancel/read-back, controller operation, and live
performance measurement. Feed every generic requirement back into schema, examples, and tests;
do not add a Power-named renderer branch.

Exit: Level C acceptance passes and a separate fixture can produce the same layout primitives.

### Wave 3 — HOTAS scale migration

Migrate page families incrementally, using advanced sections and explicit conditional policy. Use
the existing feature inventory to prevent capability loss and the large surface to measure model
publication, navigation depth, and discoverability.

Exit: all supported HOTAS capabilities have a documented Control route or an explicit justified
non-goal, and controller-only traversal does not depend on the legacy Dear ImGui frontend.

### Wave 4 — release candidate

- freeze headers, enums, sizes, calling conventions, ownership, and thread rules;
- run consumer builds against the installed SDK rather than repository-relative headers;
- produce compatibility/package artifacts and verify their exact contents;
- complete the runtime matrix on the supported Starfield/SFSE version;
- publish version notes, migration guide, license/notice set, and known limitations;
- require one release-candidate soak before tagging.

## Non-negotiable release gates

- ABI layout fixtures include the previous public structure sizes and an older-table consumer.
- Schema, generator, checked-in output, compile fixture, and SDK documentation change atomically.
- Every optional capability has an automated fallback test.
- The SWF source tree and compiled movie provenance match exactly.
- No subscriber gameplay path depends on Control being loaded.
- No UI-thread live callback performs gameplay traversal, file I/O, or unbounded work.
- All subscriber pages pass pointer, keyboard, controller, dirty-close, persistence, long-text,
  missing-provider, and incompatible-host journeys appropriate to their conformance level.
- Test evidence distinguishes automated, runtime-observed, human-judged, and still-pending claims.

## First-session queue after the usage reset

1. Decide the status/notice, confirmation, conditional-visibility, deep-link, and localization scope
   for SDK v1; record each as accept, defer, or experimental.
2. Extend the schema/code generator through the accepted stable features and add an installed-SDK
   consumer matrix.
3. Package a numbered SDK candidate and migrate AbsoluteZero as the first external canary.
4. Migrate Head Tracking, then finish the Power Level-C runtime journeys.
5. Start HOTAS page-family migration only after the smaller consumers stop finding contract gaps.
