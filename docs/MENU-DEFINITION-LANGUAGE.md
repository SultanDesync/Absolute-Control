# Absolute Control Panel menu definition language

> **Status:** Accepted target language with an implemented ABI-v1 subset. Direct ABI providers can
> publish labeled choices and bounded text inputs, but the checked-in JSON compiler does not yet
> generate them and still rejects rendered sections, formatting, and presentation hints. See
> [current state](CURRENT-STATE.md).

## Decision

Absolute Control Panel uses three separate layers:

1. A host-owned Scaleform component library renders and operates every standard widget.
2. A constrained declarative menu definition describes modules, pages, sections, and options.
3. The versioned C ABI connects stable option IDs to provider-owned read, draft, apply, cancel,
   action, and validation callbacks.

This follows the useful part of Skyrim MCM's model: a mod asks for semantic option types and the
framework supplies consistent UI behavior. It does not allow a subscriber to inject arbitrary
ActionScript, CSS, coordinates, textures, or drawing callbacks into the shared menu.

## Standard component library

The host supplies and versions these components:

- persistent mod sidebar;
- horizontal page tabs;
- scrolling option workspace and scrollbar;
- section header, divider, selected-option help, and validation message;
- toggle switch;
- bounded integer/float slider with numeric editor;
- dropdown/selector with labeled choices;
- bounded text editor;
- input-binding capture field;
- content-sized action button;
- input-aware command footer; and
- confirmation, error, and binding-capture modals.

The advanced component catalogue also includes range meters, bounded telemetry plots, and
segmented allocation grids. Their static definitions, live frames, and compound interactions are
specified in [Live and compound components](LIVE-COMPONENTS.md).

Each component owns its visual states, measured layout, focus, pointer hit region, click handling,
wheel handling, keyboard/controller behavior, accessibility text, and semantic command emission.
Native code must not maintain a second table of pixel hitboxes for rendered controls.

## Definition format

The authoring representation is JSON so it is easy to validate, generate, diff, and supply to an
AI integration harness. A minimal definition resembles:

```json
{
  "schemaVersion": 1,
  "module": {
    "id": "absolute.head_tracking",
    "title": "Absolute Head Tracking"
  },
  "pages": [
    {
      "id": "yaw",
      "title": "Yaw",
      "sections": [
        {
          "id": "response",
          "title": "Response",
          "options": [
            {
              "id": "enabled",
              "type": "toggle",
              "label": "Enabled",
              "description": "Enable yaw response."
            },
            {
              "id": "gain",
              "type": "float",
              "widget": "slider",
              "label": "Gain",
              "minimum": 0.1,
              "maximum": 4.0,
              "step": 0.1,
              "format": "{0.0}x"
            },
            {
              "id": "mode",
              "type": "choice",
              "label": "Response mode",
              "choices": [
                { "value": 0, "label": "Direct" },
                { "value": 1, "label": "Smoothed" }
              ]
            }
          ]
        }
      ]
    }
  ]
}
```

Definitions may express semantic type, bounded values, format/suffix, labeled choices, section,
description, availability, restart/advanced warnings, and limited span/density hints. They may
not specify absolute coordinates, fonts, colors, focus behavior, or executable UI code.

## Build and runtime use

The first SDK compiler validates the definition and generates immutable C++ descriptor tables plus
stable callback stubs. Existing plugins may construct the same descriptors directly in C++.
Either route reaches the identical C ABI and renderer. The JSON is therefore an authoring and
harness language, not a second runtime API with different behavior.

Provider callbacks remain authoritative for current values, validation, draft state, persistence,
and gameplay behavior. The menu definition never grants the host permission to infer or rewrite
an arbitrary INI file.

The checked-in ABI v1 profile and compiler live in [`sdk/`](../sdk/README.md). Developers may
handwrite descriptors or generate them; generated output is ordinary compile-time descriptor data
and callback wiring. The compiler has not yet caught up with the appended ABI-v1 labeled-choice
callback or `TextInput`, so definitions using those features are rejected instead of silently
degraded. Sections currently group and order source controls but cannot render headings until the
public ABI gains a section descriptor.

Generated definitions require control IDs unique across the module because generated pages share
one callback/context set and one module-wide `ParseControlId`. Direct ABI consumers may use
page-local IDs when each page's context/callbacks disambiguate them. The host admits 512 modules,
2,048 pages globally, 32 pages per module, 128 controls per page, 512 controls per module, and
32,768 controls globally. The generator now rejects a subscriber definition above its 512-control
module limit before emitting C++.

## Styling

The component library consumes host-owned design tokens for typography, colors, focus/disabled
states, spacing, row density, and animation timing. This provides the useful consistency of CSS
without exposing an open-ended cascade or per-mod theme fragments. A future global user theme may
replace a validated token set; individual subscribers do not resize or restyle shared widgets.

## Release gate

Before freezing the public SDK:

- labeled-choice and bounded-string semantics are frozen and generated deterministically; rendered
  sections, formatting, and presentation hints are either added through a compatible extension or
  explicitly deferred from the first SDK;
- JSON schema validation and C++ generation are deterministic;
- every component has keyboard, controller, pointer, and wheel tests;
- rendered elements own their hit regions and semantic events;
- long labels, localization, scaling, and high option counts pass the display matrix; and
- unknown future fields fail or degrade according to the schema compatibility rules.
