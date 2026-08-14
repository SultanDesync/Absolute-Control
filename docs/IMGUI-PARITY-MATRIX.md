# ImGui Utility Parity Matrix

> **Status:** Target acceptance matrix, not a list of completed features. Current evidence and gaps
> are tracked in [current implementation state](CURRENT-STATE.md) and [the test matrix](TEST-MATRIX.md).

The goal is workflow utility, not pixel-for-pixel imitation. A native Starfield control may replace
an ImGui widget only when it preserves the associated state, safety, input, and feedback behavior.

| Capability | Native approximation required | Research proof |
|---|---|---|
| Suite navigation | Dynamic categories/pages, current-route identity, missing-module state | Two fake providers add/remove pages |
| Draft editing | Toggles, choices, numbers, sliders, dependency visibility | Representative settings page |
| Save and apply | Validation, atomic write, reload, read-back, visible result | Inject each failure independently |
| Dirty close | Save, discard, cancel; no accidental back-stack escape | Keyboard/mouse/gamepad close tests |
| Profile context | Current target, switching guard, create/import/export/reset commands | Fake profile repository |
| Binding capture | Modal ownership, device identity, cancel/timeout, held-input reseed | HOTAS plus keyboard/gamepad capture trace |
| Calibration | Begin/sample/commit/cancel with live min/max | Synthetic axis harness |
| Live telemetry | Bounded graphs, values, availability, stale state | Sustained update and frame-time capture |
| Power editor | Six-system allocation, priority/order, clipping and preview | Representative dense grid/page |
| Macros | Ordered actions, add/remove/reorder, target picker, validation | Nested list editor proof |
| Diagnostics | Versions, paths, API status, copyable errors | Degraded-provider scenarios |
| Accessibility | Keyboard-only, controller-only, focus visibility, non-color status | Full route audit |
| Layout | 720p, 1080p, 1440p, 4K, ultrawide, supported UI scaling | Screenshot matrix |
| Localization | Externalized strings, expansion tolerance, glyph coverage | Pseudo-localization pass |
| Safe session | Notify providers open/capture state and reseed on close | No leaked gameplay outputs |

## Explicit non-parity

The native UI does not need ImGui window dragging, docking, or styling. It does need predictable
Starfield-native back behavior, button prompts, cursor behavior, sound, focus restoration, and
controller navigation—areas the overlay currently has to approximate itself.

## First vertical slice

R5 uses a fake provider rather than an Absolute module. It must include:

1. one toggle with dependent controls;
2. one fine-grained slider with direct numeric adjustment;
3. one enum/choice group;
4. one hardware binding capture;
5. one live graph and stale/unavailable state;
6. validation failure on a named field;
7. save/reload/read-back success and injected failures; and
8. a dirty close modal operable by mouse, keyboard, and controller.

Only after this slice passes should a real daughter API be adapted in another repository.
