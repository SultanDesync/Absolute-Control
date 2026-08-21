# SDK release and subscriber rollout plan

> **Prepared:** 2026-08-19. **Updated:** 2026-08-21 after the Absolute Suite `0.2.0-beta.1`
> packaging checkpoint. This is now the execution map for coordinated outside beta integrations
> and eventual public SDK freeze.

## Audited subscriber baseline

| Subscriber | Deployed UI role | Conformance | Private-beta evidence role |
|---|---|---|---|
| AbsoluteZero | compact single-page scalar editor | A | oldest-prefix compatibility, fallback ownership, and a small integration reference |
| Absolute Head Tracking | three-page settings, axes, keyboard and Input Bus capture | B | binding capture, read-back, and multi-page reference |
| Absolute Power | profile editor plus six-row live allocation grid | C | older live-header compatibility, compound editing, and telemetry reference |
| AbsoluteHOTAS | broad multi-page flight-control suite | A/B/C by page | current-header C2 scale, composition, direct tuning, profiles, layers, and deep-link reference |

The repositories intentionally vendor different generations of `AbsoluteControlPanelAPI.h`. The
registry records those exact shipped hashes; the SDK rollout must not normalize them with blind file
copies. Each future migration records its SDK package identity, compiles an ABI fixture, and lands
independently so stable standalone modules remain reviewable and deployable.

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
| deep-link/open request | HOTAS and other frontends need a supported route into a module/page/control | appended page-open request is implemented; qualify it with outside consumers before freeze |
| localization | labels are currently provider literals | freeze string ownership and key/catalog strategy before declaring SDK stable |
| packaged dependency identity | copied headers drift silently | beta kit now ships version/manifest metadata and a deployment registry; add installed-consumer layout fixtures before public release |

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

## Coordinated private-beta queue

1. Issue the numbered beta kit only to applicants accepted through direct author contact and add a
   contact-free deployment record for each accepted module.
2. Review the applicant's existing configuration ownership, fallback UI, build system, target
   pages, bindings, telemetry, and release window before selecting API levels.
3. Require an installed-kit compile check plus host-absent, incompatible-host, Apply/Cancel,
   persistence, and supported-input acceptance before deployment.
4. Feed generic gaps into the schema, generator, examples, and capability-gated APIs; never add a
   mod-named host branch to satisfy one applicant.
5. Freeze a public SDK only after at least one outside integration has exercised the private kit
   and its upgrade path without destabilizing the shipped suite modules.
