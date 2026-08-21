# Subscriber UI standard

> The platform architecture for compound cards, rows, collections, workflows,
> pinned context, conditions, and live associations is defined in
> [SEMANTIC-UI-COMPOSITION-ARCHITECTURE.md](SEMANTIC-UI-COMPOSITION-ARCHITECTURE.md).
> Until those experimental capabilities are implemented and qualified, a subscriber
> using flat controls for a compound task must describe that surface as a scaffold,
> not interaction parity.

> **Status:** Normative target for the next subscriber-upgrade phase. Current runtime evidence is
> tracked separately in [TEST-MATRIX.md](TEST-MATRIX.md); this document defines what a subscriber
> must satisfy before its Absolute Control surface is called release-ready.

Absolute Control owns presentation, focus, input-mode adaptation, modal behavior, and generic
Apply/Cancel lifecycle. A subscriber owns meaning, validation, draft state, persistence, and any
gameplay operation. A polished page is assembled from host primitives; it must not depend on
subscriber-specific coordinates, colors, fonts, input polling, or Scaleform code.

## Conformance levels

| Level | Intended surface | Required SDK facilities | Current reference |
|---|---|---|---|
| A — scalar | A bounded settings or diagnostics page | typed controls, sections, inline actions, Apply/Cancel, capability fallback | AbsoluteZero |
| B — dynamic | Profiles, labeled choices, text, keyboard/HOTAS capture, conflicts | Level A plus dynamic options, transient selection, provider capture, reassignment | Absolute Head Tracking |
| C — compound | A live editor whose graphic and scalar controls share one transaction | Level B plus bounded live frames, compound edits, explicit row-control association | Absolute Power |

HOTAS is the scale and navigation acceptance subscriber: it must meet the applicable level on each
page while proving that a large module remains discoverable, bounded, and controller-complete.

## Information architecture

- Register one module with stable, lowercase IDs and a concise user-facing title.
- Keep pages task-oriented. Split pages when controls have different persistence, safety, or user
  goals; do not split merely to avoid implementing scrolling.
- Put the primary journey first, advanced tuning second, bindings after the behavior they operate,
  and diagnostics last.
- Use `GroupHeader` for real semantic sections. A header is non-focusable and must never be the only
  explanation of a destructive or unavailable action.
- Group only two or three adjacent `Action` controls with `kControlLayoutInline`. Preserve source
  order, compatible fallback rows, and touch/controller-sized hit targets.
- A dynamic record editor uses one `RecordCollection` as its authoritative selected-record
  list/detail surface. On older hosts, fall back to one populated transient `Choice`; avoid
  duplicate Previous/Next actions when either bounded selector is available.
- Editing context shared across pages uses at most three `kControlPinnedContext` controls: the
  selected record, its activation behavior, and its binding. They must use one provider draft and
  remain available as ordinary unpinned controls or a dedicated profile page on older hosts.

## Control semantics

- Use the narrowest truthful `ControlKind` and `ValueKind`. Bounds, step, flags, and callbacks must
  agree; unsupported combinations fail registration rather than degrading unpredictably.
- Labels name the value or action. Descriptions explain consequence, units, restart requirements,
  and unusual ownership; they do not repeat the label.
- Read-only diagnostics are concise, stable, and copy-friendly. Before SDK freeze, replace the
  current read-only `InputBinding` workaround with a dedicated status/text primitive.
- Destructive actions use explicit verbs, a consequence-bearing description, and
  `kControlRequiresConfirmation`. Providers must not disguise destructive work as a toggle or
  ordinary choice.
- A transient choice changes provider-owned view state without dirtying the configuration draft.
  All other successful writes and draft-mutating actions participate in the ordinary transaction.
- Binding capture returns stable provider syntax and a human-readable label. Clear, cancel,
  timeout, duplicate, reassign, restart/read-back, and missing-device states are part of the
  control contract, not subscriber-specific modal designs.

## Compound and live controls

- Live descriptors and frames are fixed-capacity copied POD. No provider pointers reach Scaleform.
- Provider callbacks invoked by the UI thread copy prepared state; they do not traverse gameplay,
  perform I/O, allocate without a bound, or wait on an input thread.
- Scalar controls embedded in a live graphic use an explicit capability-gated association record.
  ID naming conventions never imply layout.
- ABI-v1 grid associations are one-to-one and Choice-only: the association references one declared
  column and one `Choice` on the same page. An absent capability leaves the choice in the ordinary
  control list and keeps the graphic functional.
- The same provider draft and Apply/Cancel transaction owns scalar and compound edits. A compound
  callback cannot persist behind the host's back.
- Direct pip editing guarantees that the activated visual position acquires the requested tier.
  Aggregate providers normalize intervening positions into their documented canonical ordering.

## Accessibility and input parity

- Every visible value, action, popup, modal, grid row, and row-associated control is reachable and
  operable with mouse, keyboard, and Xbox-compatible controller.
- Focus order follows visual order. Hidden or deduplicated controls retain an equivalent focus path
  through the graphic that presents them.
- Color is never the sole signal. Tier pips use `1`, `2`, and `3`; warnings and state changes have
  text; focused state has contrast beyond hue alone.
- Long device and profile names remain inspectable through fitting, scrolling-on-focus, or a
  tooltip. Truncation must not hide the control identity needed to resolve a conflict.
- Controller A/Enter activates the focused semantic object, B/Escape cancels the innermost modal or
  capture first, and D-pad/arrow behavior is stable and documented per compound control.
- Input-mode transitions, held-button suppression, focus loss, and hot-plug do not replay commands
  or leak gameplay input through the menu.

## Capability degradation

Subscribers query and size-check capabilities before constructing descriptors. Each optional
feature has one explicit fallback:

| Capability | Enhanced presentation | Required fallback |
|---|---|---|
| labeled choices | populated selector | bounded Previous/Next or another truthful scalar route |
| structured layout | section headers and inline action rows | header-free, one-action-per-row controls |
| provider capture | physical device capture | omit the binding control or expose it read-only as unavailable |
| conflict resolution | Reassign/Cancel modal | reject duplicate while retaining the draft and detail |
| record collections | bounded list/detail selector | transient labeled Choice or bounded Previous/Next route |
| action confirmation | host-owned Confirm/Cancel modal | omit the consequential action or provide a safe non-menu workflow |
| pinned context controls | compact always-visible edit-target strip | unflagged controls or the dedicated Profiles page |
| grid control association | Choice rendered on its grid row | same Choice remains in the ordinary list |

Host absence, an older table, rejected registration, or missing live capability never disables the
subscriber's gameplay runtime or headless configuration path.

## Documentation and evidence required per subscriber

Each upgraded subscriber keeps one current integration document containing:

1. module/page/control inventory and conformance level;
2. capability negotiation and every fallback;
3. configuration ownership, transaction, and failure semantics;
4. keyboard/controller/pointer focus map;
5. automated test commands and results;
6. runtime artifact hashes and explicit `pending` UX journeys; and
7. SDK/header version or packaged SDK identity used to build it.

Release acceptance requires a clean build, deterministic contract tests, no vendored-header drift,
one mouse journey, one keyboard-only journey, one controller-only journey, Apply/Cancel/read-back,
host-absent operation, and failure injection appropriate to the page's conformance level.

## Review checklist

- [ ] IDs are stable, unique in the required scope, and unrelated to layout inference.
- [ ] Control kinds, values, bounds, flags, and callbacks agree.
- [ ] Sections and inline groups have compatible fallbacks.
- [ ] Dynamic selection is singular, labeled, and transient.
- [ ] All writes share provider-owned transaction semantics.
- [ ] Every visible object has pointer, keyboard, and controller access.
- [ ] Empty, unavailable, stale, conflict, and persistence failures remain visible.
- [ ] Long text and colorblind states remain identifiable.
- [ ] Automated tests cover capability fallback and ABI size gates.
- [ ] Documentation describes current behavior separately from aspirational design.
