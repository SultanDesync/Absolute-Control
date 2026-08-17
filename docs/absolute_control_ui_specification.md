# Absolute Control — UX Redesign & Style Guide Candidate
**Normalized implementation baseline for the Starfield native mod configuration shell**

> **Status:** This is an accepted visual direction, not an authority over runtime behavior. The
> repository's `CURRENT-STATE.md`, `NATIVE-MENU-CONTRACT.md`, and provider contracts remain the
> sources of truth. Illustrative values and future components below must not be exposed as live
> features unless their host/provider schema and runtime behavior exist. The normalization notes in
> this document supersede unsupported assumptions in the original design proposal.

---

## 1. Executive Summary & Design Direction Evaluation

When building **Absolute Control**, the native mod configuration host for Starfield, we evaluated two distinct aesthetic and interaction paradigms:

| Dimension | Option 1: Native Starfield Seamless (NASA-punk Avionics) | Option 2: Bespoke Tactical Workbench (Cyber-Avionics) | **Recommended Direction: Unified Tactical Avionics** |
| :--- | :--- | :--- | :--- |
| **Visual Tone** | Clean, restrained, aeronautical industrialism. Slate backgrounds, hairline chamfers, warm white/cyan text, muted amber accents. | High-contrast, glowing accents, rich micro-animations, sci-fi cybernetics, stylized borders. | **Restrained NASA-punk Avionics with Tactile Precision.** Matches Starfield's in-universe ship computer / flight deck aesthetic while providing high-density configuration power. |
| **Player Immersion** | High familiarity. Feels like an official Bethesda flight-computer sub-terminal; zero cognitive friction. | High visual distinctiveness, but risks feeling like a detached third-party mod launcher. | **Seamlessly Integrated.** Fits directly beside Starfield’s Pause Menu without copying proprietary assets or creating visual dissonance. |
| **Density & Readability** | High legibility, crisp monospace readouts, strict hierarchical grouping. | Prone to oversized cards and decorative visual noise that harms dense pages. | **Optimized 1080p Density.** Clean 10–12 row viewport with contextual bottom help drawer; prevents card fatigue. |
| **Scaleform AS3 Constraints** | Highly compatible with vector geometry, solid fills, simple alpha, and lightweight rendering. | Heavy glow/blur filters cause severe draw-call and frame-drop penalties at 30 FPS. | **100% Vector/Geometry Compliant.** Deterministic rendering, zero heavy GPU filters, instant frame convergence. |
| **Audio-Tactile Feedback** | Subtle feedback may eventually use verified native events. | Custom synthesizer beeps can clash with ambient game audio. | **Deferred pending evidence.** No event names or native audio calls are part of the accepted baseline until runtime discovery proves them. |

---

### 1.1 The SkyUI Benchmark: PC Ergonomics & Starfield Translation

**SkyUI MCM (Mod Configuration Menu)** remains the gold standard for PC user interface design across Bethesda modding history. Its design triumphs were not accidental—they were rooted in fundamental PC UX principles that we directly adopt, modernize, and elevate for **Absolute Control**:

```
+--------------------------------------------------------------------------------------------------+
| SKYUI MCM PROVEN PARADIGMS            | HOW ABSOLUTE CONTROL ADOPTS & ELEVATES THEM             |
+---------------------------------------+----------------------------------------------------------+
| 1. High-Density, Non-Card Layout      | 10–12 settings visible simultaneously in a clean 52px    |
|    (Labels left, values right)        | aligned two-column workspace. No bloated "mobile cards". |
+---------------------------------------+----------------------------------------------------------+
| 2. Contextual Bottom Info Drawer      | Long descriptions, valid ranges, performance warnings,   |
|    (Progressive disclosure on hover)  | and default values live in the 104px bottom help drawer. |
+---------------------------------------+----------------------------------------------------------+
| 3. Instantaneous Mouse & Tab Flow     | Zero animation latency. 1-click tab and mod switching    |
|    (Zero transition lag)              | with immediate visual and sound feedback.                |
+---------------------------------------+----------------------------------------------------------+
| 4. Predictable Slider & Toggle Math   | Sliders support click-to-point, drag-scrub, and discrete |
|    (Fast scrub + fine stepping)       | arrow stepping with tabular monospace numeric badges.    |
+---------------------------------------+----------------------------------------------------------+
| 5. Universal Keyboard/Gamepad Parity  | Full D-pad / Arrow traversal, native button glyphs, and  |
|    (No mouse-only dead ends)          | input-aware command ribbon prompts.                      |
+---------------------------------------+----------------------------------------------------------+
| 6. Persistent Sidebar Context         | Unlike SkyUI (which swapped the sidebar out), Absolute   |
|    (Next-Gen Refinement)              | Control keeps the mod sidebar visible at all times!      |
+---------------------------------------+----------------------------------------------------------+
| 7. Transactional Draft Safety         | Prevents accidental configuration loss with generation-  |
|    (Next-Gen Refinement)              | tracked drafts and "Apply / Discard / Stay" modals.      |
+---------------------------------------+----------------------------------------------------------+
```

---

## 2. Core Layout Architecture & 1920×1080 Geometry

The native Scaleform movie paints a logical **1920×1080 (16:9)** stage. The layout expands beyond the constrained prototype (1580×920) while preserving a **56 px edge-safe margin** for 720p/1080p/4K overscan and TV display compliance.

```
+--------------------------------------------------------------------------------------------------+ (0,0)
| [56px Safe Margin]                                                                               |
|  +--------------------------------------------------------------------------------------------+  |
|  | [HEADER BAR]                                         [SELECTED MODULE / PROVIDER STATUS] |  | Y: 56..104 (H: 48)
|  |  ABSOLUTE CONTROL  //  ABSOLUTE POWER                         READY  [DRAFT: CLEAN]         |  |
|  +--------------------------------------------------------------------------------------------+  |
|  | [MODULE SIDEBAR]          | [PAGE TABS ROW]                                                |  | Y: 116..164 (H: 48)
|  | (W: 340px)                | [ PRESETS (Active) ] [ AUTOMATION ] [ DIAGNOSTICS ]            |  |
|  |                           +----------------------------------------------------------------+  |
|  | * Absolute Power          | [CENTRAL WORKSPACE / ACTIVE PAGE]                              |  | Y: 172..856 (H: 684)
|  |   - 3 Pages / 56 Controls | (W: 1444px, H: 684px)                                          |  |
|  |                           |                                                                |  |
|  | o Absolute Zero           | - Form Rows (Height: 52px each, 10-12 rows visible)            |  |
|  |   - 2 Pages / 14 Controls | - Segmented 6x32 Allocation Matrix                            |  |
|  |                           | - Rule Inspector / Bounded Text Editors                        |  |
|  | o Flight Dynamics         | - Scrollable Viewport with Custom Scroll Indicator             |  |
|  |   - 4 Pages / 32 Controls |                                                                |  |
|  |                           +----------------------------------------------------------------+  |
|  | o Engine Automation       | [SELECTED-CONTROL HELP DRAWER]                                 |  | Y: 864..968 (H: 104)
|  |   - 1 Page / 8 Controls   | Contextual description, validation constraints, default value  |  |
|  +---------------------------+----------------------------------------------------------------+  |
|  | [INPUT-AWARE COMMAND BAR]                                   [DIRTY / TRANSACTION CONTROLS] |  | Y: 976..1024 (H: 48)
|  | [Select]  [Apply]  [Cancel]  [Tab / B: Back]                 STATUS: 2 PENDING CHANGES      |  |
|  +--------------------------------------------------------------------------------------------+  |
+--------------------------------------------------------------------------------------------------+ (1920,1080)
```

### Exact Coordinate & Dimensions Specification

| Surface Region | X | Y | Width | Height | Purpose & Behavior |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Canvas Safe Frame** | `56` | `56` | `1808` | `968` | Primary boundary for all interactive elements. |
| **Header Bar** | `56` | `56` | `1808` | `48` | Shell title, selected module/page breadcrumb, and provider-reported status. |
| **Module Sidebar** | `56` | `116` | `340` | `848` | Vertical list of installed subscriber modules (virtualized, up to 512 entries). |
| **Page Tab Row** | `416` | `116` | `1448` | `48` | Horizontal tabs for the active module's pages. |
| **Active Workspace** | `416` | `172` | `1448` | `684` | Scrollable control viewport containing form rows, custom widgets, or allocation matrix. |
| **Contextual Help Drawer**| `416` | `864` | `1448` | `104` | Focus-linked description box with parameter metadata, defaults, and error messages. |
| **Command Bar / Footer** | `56` | `976` | `1808` | `48` | Dynamic input prompt ribbon with click targets for Apply, Cancel, Rebind, and Close. |

---

## 3. Style Guide & Design Tokens

### 3.1 Color System & Token Mapping

All colors are designed for high contrast (>7:1 against dark backgrounds for text) and include non-color companion cues (symbols, shapes, and badges) for full accessibility.

```
       DEEP SPACE SLATE        HULL PANEL BG          AVIONICS CYAN          CONSTELLATION GOLD
          #060E14                #0F1E28                #5CE1E6                #E5A93C
      [Shell Backdrop]       [Card/Workspace]       [Focus & Active]       [Draft / Warning]
```

| Token Name | Hex Code | RGB (0-255) | Semantic Usage | Companion Visual Cue |
| :--- | :--- | :--- | :--- | :--- |
| `COLOR_BG_STAGE` | `0x060E14` | (6, 14, 20) | 16:9 full background backdrop fill | Solid matte finish |
| `COLOR_BG_PANEL` | `0x0F1E28` | (15, 30, 40) | Workspace, sidebar, and drawer base panel | Hairline border framing |
| `COLOR_BG_ROW_EVEN` | `0x132633` | (19, 38, 51) | Alternating row background (even) | Flat surface |
| `COLOR_BG_ROW_ODD` | `0x10212D` | (16, 33, 45) | Alternating row background (odd) | Flat surface |
| `COLOR_BG_ROW_HOVER` | `0x1A3546` | (26, 53, 70) | Hovered row state | Left edge 2px marker |
| `COLOR_BG_ROW_FOCUS` | `0x22465C` | (34, 70, 92) | Keyboard / gamepad focused row | Left edge 4px cyan notch |
| `COLOR_BORDER_DEFAULT` | `0x254457` | (37, 68, 87) | Section dividers and panel outlines | 1px solid stroke |
| `COLOR_BORDER_FOCUS` | `0x5CE1E6` | (92, 225, 230) | Active focus outline and tab chevron | 2px solid stroke + corner pip |
| `COLOR_TEXT_PRIMARY` | `0xF0F7FB` | (240, 247, 251) | Setting labels, headers, active values | High contrast bold weight |
| `COLOR_TEXT_MUTED` | `0x9CB5C4` | (156, 181, 196) | Inactive labels, secondary readouts | Regular weight |
| `COLOR_TEXT_DIM` | `0x647E8E` | (100, 126, 142) | Units, breadcrumbs, shortcuts | Regular / condensed |
| `COLOR_ACCENT_CYAN` | `0x5CE1E6` | (92, 225, 230) | Primary active state, toggle ON, focus | Light/Glow indicator pip `[*]` |
| `COLOR_STATE_DIRTY` | `0xE5A93C` | (229, 169, 60) | Unsaved draft setting or dirty page | Amber asterisk badge `[*] DRAFT` |
| `COLOR_STATE_WARNING`| `0xE5A93C` | (229, 169, 60) | Game balance or validation warning | Warning triangle icon `[!]` |
| `COLOR_STATE_ERROR` | `0xFA5252` | (250, 82, 82) | Validation error, backend fault | Octagon cross icon `[X]` |
| `COLOR_STATE_DISABLED`| `0x384B56` | (56, 75, 86) | Unavailable or read-only controls | Strikethrough or lock icon `[/]` |
| `COLOR_TIER_OPTIMAL` | `0x38D9A9` | (56, 217, 169) | Priority Tier 1 (Green/First) | Up-triangle symbol `[^]` |
| `COLOR_TIER_NOMINAL` | `0xFCC419` | (252, 196, 25) | Priority Tier 2 (Yellow/After Green) | Dash symbol `[-]` |
| `COLOR_TIER_MAXIMUM` | `0xFF6B6B` | (255, 107, 107) | Priority Tier 3 (Red/Last) | Double-arrow symbol `[>>]` |

---

### 3.2 Typography & Font System Specification

#### 3.2.1 Problem Analysis: Retiring the "Warez" Bitmapped Prototype (`PixelTextRenderer.as`)
The initial prototype relied on `PixelTextRenderer.as`, which manually painted 5×7 bitmapped pixel blocks (`target.graphics.drawRect`) in forced all-caps. While useful as an early zero-dependency proof-of-concept, this created several critical UI defects:
1. **Severe Horizontal Space Waste**: Monospaced 5×7 grid with 6px advance per character forced wide labels to truncate prematurely (e.g. `20` characters consumed `240px` with no proportional kerning).
2. **"Warez / Demoscene" Aesthetic**: Blocky bitmapped fonts in high-resolution 1080p/4K render as harsh, unaliased squares that conflict with Starfield's sleek, anti-aliased HUD.
3. **Zero Lowercase or Extended Glyphs**: Inability to render lowercase letters, punctuation (`/`, `(`, `)`, `,`, `_`), or localized international characters (Cyrillic, Umlauts, CJK).
4. **Draw-Call & Frame Overhead**: Drawing hundreds of individual vector rects per string flooded the Scaleform display list.

---

#### 3.2.2 Deterministic Interim Font Baseline

Scaleform supports vector text through `flash.text.TextField` and `flash.text.TextFormat`, but the
original `$MainFont`, `$TitleFont`, and `$FixedFont` aliases have not been proven in Starfield.
`TextFormat.font` also accepts one face name; a comma-separated CSS fallback stack is not valid.

The implemented baseline therefore embeds unmodified Apache-licensed Roboto Regular and Bold TTF
files into the SWF under the internal face name `AbsoluteControlBody`. This makes rendering
reproducible and independent of OS fonts. A verified pause-menu-compatible face may replace Roboto
later after licensing, glyph coverage, package size, and in-game rendering are proven. That change
must not alter layout or provider contracts.

---

#### 3.2.3 Space-Efficient Typography Hierarchy Table

Proportional sentence casing materially reclaims horizontal space compared with the all-caps
bitmapped prototype. The percentage must be measured from real labels rather than treated as a
fixed guarantee. Current sizes are intentionally larger than the original proposal so the logical
1920×1080 stage remains legible when scaled to 720p:

| Text Role | Font Family | Size | Weight | Case & Tracking | Line Height | Color Token | Alignment / Overflow |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Shell Header Title** | `AbsoluteControlBody` | `26 px` | Bold | UPPERCASE | 30 px | `COLOR_TEXT_PRIMARY` | Left-aligned, no wrap |
| **Module / Tab Label** | `AbsoluteControlBody` | `16–17 px` | Regular/Bold active | Sentence Case | 22 px | `COLOR_TEXT_PRIMARY` | Left-aligned, bounded ellipsis |
| **Section Header** | `AbsoluteControlBody` | `18–19 px` | Bold | Sentence Case | 24 px | `COLOR_TEXT_PRIMARY` | Left-aligned + optional rule |
| **Setting Row Label** | `AbsoluteControlBody` | `17 px` | Regular/Bold focused | Sentence Case | 22 px | `COLOR_TEXT_PRIMARY` | Left-aligned, bounded ellipsis |
| **Numeric / Value Badge**| `AbsoluteControlBody` | `16 px` | Regular | Tabular face is future work | 20 px | `COLOR_ACCENT_CYAN` | Right-aligned, measured cell |
| **Keybinding Chord** | `AbsoluteControlBody` | `16 px` | Bold | UPPERCASE | 20 px | `COLOR_TEXT_PRIMARY` | Centered in bounded field |
| **Help Drawer Prose** | `AbsoluteControlBody` | `16 px` | Regular | Sentence Case | 20 px | `COLOR_TEXT_MUTED` | Multi-line wrap, 3-line max |
| **Help Metadata**| `AbsoluteControlBody`| `14 px` | Regular | Sentence Case | 18 px | `COLOR_TEXT_DIM` | Left-aligned |
| **Status / Dirty Badge** | `AbsoluteControlBody` | `13–14 px` | Bold | UPPERCASE | 18 px | semantic token | Text plus non-color symbol |
| **Command Ribbon Prompt**| `AbsoluteControlBody` | `15 px` | Regular/Bold focused | Sentence Case | 20 px | `COLOR_TEXT_PRIMARY` | Inline with verified prompt |

---

#### 3.2.4 Scaleform ActionScript 3 Vector Implementation

The implementation lives in `interface/src/acp/ui/VectorTextRenderer.as`. It embeds the two
verified TTF weights, enables embedded-font rendering and advanced antialiasing, supports bounded
multi-line help text, and keeps text mouse-transparent so the existing semantic hit targets remain
authoritative. Build success proves compiler compatibility; 720p, 1080p, 4K, and in-game Scaleform
screenshots are still required before the typography is release-accepted.

---

### 3.3 Tactile Audio Feedback Mapping

Audio is optional polish and is not in the current implementation baseline. Candidate feedback
classes are focus move, activation, slider step, capture arm/commit, apply, back, validation failure,
and emergency disable. Before adding any of them, runtime research must identify valid Starfield
events, confirm menu-category volume behavior, and prove that rapid navigation cannot spam or layer
sounds. Unsupported names from the original proposal must not be wired through
`ExternalInterface` as guesses.

---

## 4. UI Component Redesigns & State Specifications

### 4.1 Form Setting Row (52 px Standard Height)

The standard form row uses an aligned two-column layout:
- **Left Column (480 px)**: Setting label + Dirty/State badge.
- **Right Column (920 px)**: Contextual widget (Slider, Toggle, Choice, Text, Keybind).

```
+-------------------------------------------------------------------------------------------------------+
|  [>] Target Pitch Deflection Threshold      [*] DRAFT | [======|===================] 60.0%  (Default) |
+-------------------------------------------------------------------------------------------------------+
    ^ Focus Notch     ^ Label                  ^ State Badge       ^ Custom Widget             ^ Readout
```

#### States:
1. **Idle**: Neutral background (`0x132633`), muted border, white text.
2. **Hover**: Highlight background (`0x1A3546`), subtle 2px left border.
3. **Focused (Active)**: Deep cyan accent fill (`0x22465C`), 4px bright cyan focus bracket (`0x5CE1E6`), bold text.
4. **Draft / Dirty**: Amber indicator chip `[*] DRAFT` next to label; value displayed in gold font (`0xE5A93C`).
5. **Disabled / Locked**: Muted grayed text (`0x647E8E`), lock icon `[/] LOCKED`, non-responsive to clicks/taps.
6. **Validation Error**: Red outline (`0xFA5252`), error chip `[X] OUT OF BOUNDS`, help drawer displays actionable fix.

---

### 4.2 Segmented Toggle Switch

Replaces basic checkboxes with an aeronautical rocker switch with illuminated status pips:

```
[ OFF ]  ===>   [ [X] OFF | ( ) ON  ]   (Inactive Gray Fill)
[ ON  ]  ===>   [ ( ) OFF | [*] ON  ]   (Active Cyan Illuminated Fill)
```
- **Dimensions**: Width: `140 px`, Height: `32 px`.
- **Keyboard/Controller Input**: Pressing `Enter` / Controller `A` flips the state with an instant 3-frame sliding pill transition.

---

### 4.3 Slider with Artifact-Safe Ring Thumb and Aligned Readout

Combines pointer click/drag with discrete focus-based stepping and an aligned numeric readout:

```
[====================(o)-----------]   [  120.00  ]
 ^ active cyan fill    ^ ring thumb       ^ bounded value box
```
- **Track Geometry**: `584 px` wide × `6 px` high with rounded caps. The inactive rail uses the
  muted border token; the completed range uses Cyan (`0x5CE1E6`).
- **Endpoint Clearance**: Inset the rail by at least the thumb radius plus pointer padding. A thumb
  at minimum or maximum must remain entirely inside its widget and hit target at every supported
  scale; no part may clip against the row or expose the rail end through the knob.
- **Thumb Construction**: A `20 px` diameter circle with an opaque panel-colored body, `2 px`
  semantic outline, and `3 px` solid center marker. Draw the opaque body over the rail before the
  outline and marker so Scaleform antialiasing cannot produce seams or fill artifacts.
- **Value Box**: `86 × 32 px`, aligned across rows, opaque, bounded to eight display characters in
  the current host, and visually separate from the draggable rail.
- **Input**: Clicking the rail positions the value; dragging scrubs it; `Left` / `Right` step by the
  provider's `stepSize`. Controller stepping remains a target until directional routing is wired.
  Do not draw decorative `-`/`+` buttons unless they are genuine focusable hit targets.

---

### 4.4 Labeled Choice / Dropdown

Provides a directly labeled selector with an expandable popover for long option lists:

```
[  BALANCED POWER DISTRIBUTION  [balanced]  STARTUP                 v  ]
   ^ Provider-owned option label and stable identity                   ^ Expand
```
- **Expanded Popover**: Pops out as a high-z-index overlay panel over the workspace without breaking row layout. It virtualizes eight rows, exposes hidden counts, and supports pointer, wheel, Up/Down, Page Up/Down, Enter, Escape, and Tab.
- **Selected-Record Authority**: When a choice selects the record being edited, it is the one
  authoritative selection control. Populate every available record and include compact metadata
  needed to disambiguate it, such as source/provenance, startup state, or ON/OFF state.
- **No Redundant Navigation**: Do not pair an authoritative dropdown with Previous/Next actions,
  another selected-record summary, or a second editable name that appears to select a different
  record. Rename controls and lifecycle actions explicitly operate on the dropdown selection.
- **Transaction Semantics**: Changing a provider-owned view selection is transient and must not
  create dirty state. Choices that alter configuration values remain ordinary draft edits.

#### 4.4.1 Workspace Scrollbar and Hidden-Content Indicator

Every workspace taller than its row viewport exposes an immediately visible position indicator;
mouse-wheel discovery is never the only evidence that more settings exist.

- Draw a muted `4 px` rail and a high-contrast Cyan thumb at least `10 px` wide and `32 px` high.
- Thumb size represents the visible fraction and thumb position represents the first visible row.
- Show directional markers plus exact hidden counts above and below (for example `^ +9` and
  `+4 v`) whenever content remains outside the viewport.
- Hide the scrollbar only when all controls fit. Preserve focus visibility while keyboard,
  controller, pointer-wheel, Page Up/Down, or scrollbar interactions move the viewport.
- The scrollbar is a navigation affordance, not a value control; it never dirties the page.

#### 4.4.2 Explicit Action Affordance

An action row must contain an obvious button adjacent to its descriptive label. The row label alone
may remain a generous hit target, but it cannot be the only visual cue that the row executes work.

```
| Rebuild input cache                                      | [ RUN ] |
```

- Use `RUN` for neutral immediate commands when no clearer short verb exists. Prefer a specific
  verb such as `ACTIVATE`, `DUPLICATE`, `REVERT`, `DELETE`, or `DISABLE ALL` when it communicates
  the consequence more precisely.
- Never use an unlabeled chevron (`>`) as the sole action affordance.
- Destructive, apply-before-invoke, unavailable, and in-progress actions need distinct text plus
  non-color state cues; the help drawer describes scope and transaction ordering.

---

### 4.5 Keybinding Capture Field & Listening Pill

Captures keyboard keys, modifiers (Ctrl, Alt, Shift), and controller inputs with failsafe cancel protection:

```
Default State:    [  ALT + SHIFT + W  ]   [ CLEAR ]
Listening State:  [ >> PRESS ANY KEY << ] [ ESC: CANCEL ]  <-- Pulsing Amber Glow
```
- **Failsafe target**: `Escape`, `Tab`, or controller `B` cancels capture without clearing according
  to the active input route. A Clear command may set `UNBOUND` only when the provider/control
  contract exposes that operation.

---

### 4.6 6×32 Segmented Power Allocation Matrix (Absolute Power Special Widget)

Represents the six Starfield-aligned ship-system slots (`W0`, `W1`, `W2`, `ENG`, `SHD`, `GRV`)
with up to 32 discrete power pips per system. The provider supplies both the short HUD label and the
full display name:

```
SYS   PIPS ALLOCATION (1..32)                                            CUR  MAX  TIER 1/2/3
----------------------------------------------------------------------------------------------
W0    [||||||||||||||||||||||||        ]                                  24   32   [ 8/16/24 ]
W1    [||||||||||||||||                ]                                  16   28   [ 6/12/16 ]
W2    [||||                            ]                                   4   12   [ 2/ 4/ 8 ]
ENG   [||||||||||||                    ]                                  12   24   [ 4/ 8/12 ]
SHD   [||||||||                        ]                                   8   16   [ 2/ 4/ 8 ]
GRV   [||||                            ]                                   4   12   [ 1/ 2/ 4 ]
----------------------------------------------------------------------------------------------
TOTAL REACTOR: 68 / 124 ALLOCATED                                             AVAILABLE: 56
```

#### Multi-Modal Pip Design (Accessible without Color):
- **Tier 1 (First - Green `0x38D9A9`)**: Solid vertical bar with top pip notch `[^]`.
- **Tier 2 (After Green - Yellow `0xFCC419`)**: Solid vertical bar with central hollow slit `[-]`.
- **Tier 3 (Last - Red `0xFF6B6B`)**: Solid vertical bar with diagonal cross hatch `[X]`.
- **Hollow (Unallocated `0x17303D`)**: 1px perimeter box with transparent center.
- **Live Readback**: `2 px` Cyan outline around each currently powered pip.
- **Draft Target Preview**: Short Gold tick inside each pip included in the pending allocator target.

#### Required Inline Explanation and Editing Context:
- Keep a legend on the graph itself: `GREEN: FIRST`, `YELLOW: AFTER GREEN`, `RED: LAST`,
  `CYAN OUTLINE: LIVE`, `GOLD TICK: PREVIEW`, and `HOLLOW: AVAILABLE CAPACITY`.
- Show each system's requested Green/Yellow/Red counts beside that system rather than requiring the
  user to decode the pip colors or consult another page.
- Tier buttons live with the graph and state which priority is currently being edited. Each system
  row is independently adjustable within that selected tier; the interaction copy must explain
  fill-through and trim behavior.
- Show live current/maximum and draft target values on every system row. Color remains a secondary
  cue; wording, counts, outlines, and marks carry the same meaning.

---

### 4.7 Future AbsoluteHOTAS Pattern: Real-Time Axis Telemetry with Inward Dual-Pips

> Later-phase design pattern only. AbsoluteHOTAS must publish raw and processed values through the
> provider telemetry contract. Absolute Control must never poll DirectInput/XInput or calculate the
> processed output itself.

Designed for flight joysticks, throttles, rudder pedals, and head tracking. Displays instantaneous physical hardware position against processed game engine output at up to 30 Hz host cadence:

```
                     ▼ [RAW HARDWARE: 64.2%] (Top Inward White Pip)
+--------------------+-------------------------+-------------------------+
| [//] DEADZONE 0-5% |  NORMAL FLIGHT 5-80%    | [>>>] BOOST GATE 80-100%|
+--------------------+-------------------------+-------------------------+
                          ▲ [FILTERED OUTPUT: 52.8%] (Bottom Inward Cyan Pip)
```

#### Anatomical Breakdown:
1. **Top Inward Pip (`▼` White Needle)**:
   - Tracks **Raw Physical Hardware Deflection** directly from DirectInput / XInput polling.
   - Needle points DOWN into the top rail of the graph track.
2. **Bottom Inward Pip (`▲` Cyan Needle)**:
   - Tracks **Processed Game Engine Output** (after deadzone subtraction, non-linear S-curves, slew-rate damping, and throttle penalties).
   - Needle points UP into the bottom rail of the graph track.
3. **Vernier Alignment Reticle**:
   - When input equals output (linear 1:1 mapping), the top and bottom pips align to form a single vertical crosshair reticle.
   - When diverging (e.g. inside a deadzone or along an exponential curve), the horizontal distance between pips visually explains the exact curve attenuation in real-time.

---

### 4.8 Future AbsoluteHOTAS Pattern: Adjustable Functional Zones & Gate Markers

Allows flight sim pilots to configure multi-stage operational bands on a single physical throttle or joystick axis:

```
0%                                60% (Penalty Gate)      85% (Boost Gate)        100%
+-------+-------------------------+-----------------------+-----------------------+
| ///   |                         | \\\\\\\\\\\\\\\\\\\\\ | >>>>>>>>>>>>>>>>>>>>> |
| DEAD  |   LINEAR CRUISE ZONE    | PITCH PENALTY DAMPING | AFTERBURNER / BOOST   |
+-------+-------------------------+-----------------------+-----------------------+
        ^ [|] Handle 1 (5%)       ^ [|] Handle 2 (60%)    ^ [|] Handle 3 (85%)
```

#### Functional Zone Categories & Visual Shading:
| Zone Role | Shading Pattern | Color Tint Token | Purpose & In-Game Function |
| :--- | :--- | :--- | :--- |
| **Deadzone Band** | Muted diagonal hatch (`///`) | `0x253846` (30% Alpha) | Prevents stick drift near center or bottom detent. |
| **Normal / Cruise Band**| Solid clean matte | `0x10212D` (40% Alpha) | Standard 1:1 or curved flight maneuver range. |
| **Deflection Penalty Gate**| Warning cross-hatch (`\\\`) | `0x594925` (Amber Tint) | Highlights the 60% Starfield pitch throttle penalty threshold. |
| **Boost / WEP Gate** | High-contrast chevron (`>>>`)| `0x5C2028` (Crimson Tint) | Engages ship afterburner when throttle passes detent. |
| **Reverse Throttle Band**| Alternating hazard lines (`###`)| `0x4A2030` (Coral Tint) | Bipolar axis (-100%..0%) for zero-point reverse thrust. |

#### Interactive Gate Handles (`[|]`):
- **Pointer Dragging**: Click and drag the vertical bracket handle along the axis track; emits bounded typed draft writes to companion threshold parameters.
- **Precision Keyboard / Gamepad Stepping**: Focusable handles step by 1.0% (Arrow keys) or 5.0% (`Shift` / Gamepad Triggers).

---

### 4.9 True Expandable Dropdown Popover (Select Component)

Replaces primitive integer steppers with a high-density, keyboard/gamepad-accessible floating dropdown list:

```
Closed State (52px Row):
+-------------------------------------------------------------------------------------------------------+
| Joystick Throttle Detent Behavior                           | [ S-CURVE EXPO DAMPING           ( v ) ]|
+-------------------------------------------------------------------------------------------------------+

Opened State (High-Z-Index Floating Popover):
                                                              +-----------------------------------------+
                                                              | [ ] LINEAR 1:1 PASSTHROUGH              |
                                                              | [*] S-CURVE EXPO DAMPING        (Active)|
                                                              | [ ] DUAL-RATE COMBAT EXPONENTIAL        |
                                                              | [ ] CUSTOM 5-POINT POLYNOMIAL           |
                                                              | [ ] HARDWARE REVERSE SPLIT              |
                                                              +-----------------------------------------+
```

#### Behavior & Accessibility:
- **Z-Index Layering**: Opens as a top-level overlay inside `MenuShellRenderer.as`, casting a clean 1px cyan border over subsequent rows without reflowing the page layout.
- **Keyboard / Gamepad Flow**: `Enter` / `A` opens the popover; `Up` / `Down` arrows highlight options; `Enter` / `A` selects and closes; `Escape` / `B` dismisses without changing.
- **Scroll Handling**: If options exceed the eight-row virtualized viewport, a compact vector
  scrollbar and hidden-option counts appear.

---

### 4.10 Direct-Entry Bounded Numeric Input Box

Enables pilots and modders to type exact floating-point or integer values directly instead of scrubbing sliders:

```
Readout State:  [  1.450 ms  ]  <-- Click or press Enter to edit
Active Typing:  [| 1.450     ]  <-- Blinking cyan caret, numeric/decimal character filter
Committed:      [  1.450 ms  ]  <-- Clamped between min/max, formatted with units
```

#### Validation Rules:
1. **Input Masking**: Filters non-numeric characters in real time (permits digits `0..9`, `.`, and leading `-` for signed axes).
2. **Provider Validation**: On `Enter` or accepted focus change, send the bounded draft command to
   the provider. The provider owns clamping, rejection, and canonical formatting. Render its returned
   value and warning; the host must not silently invent successful clamping.
3. **Failsafe Escape**: Pressing `Escape` discards typed text and restores the previous valid snapshot value.

---

### 4.11 Future AbsoluteHOTAS Pattern: 2D Response Curve Transfer Function Graph

Visualizes non-linear mathematical transfer functions ($f(x_{in}) = y_{out}$) for flight stick axes:

```
Y: Output (0..100%)
100% +------------------------------------+ [*] Live Point (X: 72%, Y: 84%)
     |                               . *' |
     |                           . *'     |
     |                       . *'         |  Curve Parameters:
 50% |                  . * '             |  - Expo Power: 1.65
     |             . * '                  |  - Center Deadzone: 4.0%
     |       . * '                        |  - Dynamic Smoothing: 12 ms
  0% +------------------------------------+
     0%                50%            100%  X: Physical Input (0..100%)
```
- **Real-Time Point Tracking**: A bright cyan illuminated pip `[*]` slides smoothly along the vector curve as the player physically deflects their flight stick, providing instant visual feedback on curve progression.

---

## 5. Absolute Power Screen Redesigns

### Screen 1: Presets Workbench (35-Control Labeled-Choice Envelope)
- **Profile Selector**: One source/startup-aware populated dropdown is the authoritative selected
  profile. Do not duplicate it with Previous/Next, a selected-preset summary, or a second ambiguous
  preset-name field; label the text field `Rename selected profile`.
- **Toolbar**: `[ + New Preset ]`, `[ Duplicate ]`, `[ Set Startup ]`, `[ Revert ]`, `[ Delete ]`.
- **Central Visualizer**: 6×32 Segmented Allocation Matrix with the inline tier/live/preview/capacity
  legend, per-system G/Y/R request counts, selected-priority editing context, and live power readback.
- **Precision Sliders**: 18 exact tier sliders grouped by system for precise integer configuration.
- **Context Actions**: `[ Save Power Changes ]`, `[ Save & Activate ]`, and
  `[ Activate Saved ]`. `Activate Saved` always uses the last verified backend record, never the
  local draft. Do not add a second generic Save Draft action inside the page body.

### Screen 2: Automation / Cheats (Coming Soon)

For the early simultaneous release, retain the stable route but render only the provider's
read-only deferral/status records and explicit **Disable All Automation Now** action. Do not expose
the experimental rule editor. Cross-weapon identity, convergence lifetime, available-first
assignment, and the final On-Demand Power behavior are not release-qualified.

The detailed design below is retained as deferred research guidance, not current release scope:

- **Top Caution Banner**:
  ```
  [!] CAUTION: AUTOMATION MODIFIES GAME BALANCE BEHAVIOR
  Automated rules will dynamically reallocate ship power during live combat.
  ```
- **Activation Checklist and Gates**: Put a provider-authored readiness row above the editor that
  resolves to a concrete `BLOCKED`, `PENDING: choose Apply`, or `ARMED` state. Number the required
  controls `1. Global automation gate` and `2. Selected rule gate`; place the selected-rule gate
  immediately beside the selector and rule meaning rather than below lifecycle actions. A ready
  event source alone is not an armed rule: both gates must be ON and the transaction applied. Keep
  only gates for unavailable event sources disabled. Include the provider-owned
  `[ DISABLE ALL RULES NOW ]` action only when its command is registered and available.
- **Rule Selector**: One populated dropdown is the authoritative selected-record control. Each option
  shows display name, source provenance, and current ON/OFF state; selection changes do not dirty the
  transaction. Do not also render Previous/Next navigation or a redundant selected-rule summary.
- **Rule Inspector Grid**: Selected-rule fields (Trigger event, Priority, Target System, Hold Time, Hysteresis, Action).
- **Readiness Truth Indicator**: Report each event source independently. Weapon Fire may display
  `READY` while Damage, Throttle, and Manual remain `UNAVAILABLE`; do not collapse partial readiness
  into one misleading global state. Show current demand count and settlement/restoration state when
  the provider publishes them.

### Screen 3: System Diagnostics & Health Tree
- **Grouped Categories**:
  1. `Compatibility`
  2. `Executor`
  3. `Live Ship`
  4. `Configuration`
  5. `Activation`
  6. `Frontends`
- Render only provider-published fields. Vtable addresses, lock timing, ship reference IDs, and
  frontend polling details in the original proposal were illustrative—not promised schema.
- A copyable support summary is desired but remains an engineering dependency until a dedicated,
  privacy-reviewed provider command exists.

---

## 6. Modal Dialogs & Transaction Workflows

### 6.1 Dirty-State Navigation Modal ("Apply / Discard / Stay")

Triggered when the user attempts to switch modules, change pages, or exit with pending unapplied drafts:

```
+-----------------------------------------------------------------------------+
|  [!] UNAPPLIED CONFIGURATION CHANGES                                        |
|                                                                             |
|  You have unsaved changes in: Absolute Power -> Flight Presets              |
|  What would you like to do with these changes before continuing?            |
|                                                                             |
|  [ (A) APPLY CHANGES ]     [ (X) DISCARD CHANGES ]     [ (B) STAY & EDIT ]  |
+-----------------------------------------------------------------------------+
```

### 6.2 Keybinding Capture Modal
- Darkens background with a 60% alpha matte.
- Displays large glowing capture badge: `LISTENING FOR INPUT...`.
- Shows countdown timeout and `[ ESCAPE to Cancel ]`.

---

## 7. Gamepad, Keyboard & Mouse Navigation Matrix

The semantic actions below are targets, not permission to hard-code guessed Starfield bindings.
Prompts must reflect the events actually routed by the native adapter. The currently accepted back
paths are keyboard `Tab`/`Escape` and controller `B`; their exact behavior remains context-aware
during capture and modals.

```
[ GAMEPAD CONTROLLER MAP ]
----------------------------------------------------------------------
(LB / RB)         --> Candidate: cycle module pages / tabs after runtime validation
(LT / RT)         --> Candidate: rapid scroll / coarse slider step after runtime validation
(D-PAD Up/Down)   --> Move Row Selection
(D-PAD Left/Right)--> Adjust the focused slider / choice
(A) Button        --> Activate / Toggle / Enter Sub-menu
(B) Button        --> Back / Close / Dismiss Modal
(X / Y)           --> Unassigned until native routing and destructive-action safety are proven
(View / Select)   --> Candidate: change focus region after runtime validation
(Menu / Start)    --> Reserved; do not open a fabricated global Diagnostics route
```

Keyboard focus-region traversal remains explicit: arrow keys operate the current region, `Enter`
activates, and the implemented region navigation is preserved. Mouse hover never becomes the only
focus or state cue.

---

## 8. Title Branding Treatment: Production vs Early Release

1. **Quiet Production Treatment (Recommended)**:
   - Clean, minimalist Starfield sans-serif lettering: `ABSOLUTE CONTROL` in pure white with a thin 1px cyan baseline.
   - Sits seamlessly beside vanilla Pause Menu options (`STATUS`, `INVENTORY`, `SETTINGS`, `ABSOLUTE CONTROL`).

2. **Discoverable Early-Release Treatment**:
   - Includes a subtle amber version badge: `ABSOLUTE CONTROL [ DEV-PREVIEW v1.2 ]`.
   - Useful for testing and community beta releases to distinguish from final production builds.

---

## 9. Engineering & ActionScript 3 Implementation Map

| Feature / UI Component | Implementation Status in Current Code | Required Engineering Work |
| :--- | :--- | :--- |
| **1920×1080 Stage Safe-Area Layout** | Implemented locally as a 1808×968 safe frame | In-game 720p/1080p/4K and ultrawide visual acceptance. |
| **Embedded Vector Typography** | Roboto Regular/Bold compile into the SWF through `VectorTextRenderer.as` | In-game glyph, hinting, package-size, and localization review; verified production-face decision later. |
| **Color & Theme Tokens** | Candidate palette mapped into `PanelTheme.as` | Screenshot contrast and color-blind/non-color-cue acceptance. |
| **Segmented 6×32 Grid Widget** | Runtime registered and vector-rendered | Responsive geometry, non-color tier marks, pointer/controller, persistence, and frame-time acceptance. |
| **Choice / Dropdown Popover** | Implemented with dynamic provider labels, transient selections, and an eight-row virtualized overlay | In-game pointer/keyboard/controller interaction and large-list visual acceptance. |
| **Artifact-Safe Slider** | Implemented with inset 6px rail, opaque 20px ring thumb, center marker, and aligned readout | Multi-resolution endpoint/clipping and pointer-drag regression. |
| **Workspace Scrollbar** | Implemented as a proportional position indicator with above/below hidden counts | Add direct thumb/track pointer interaction and complete controller paging acceptance. |
| **Explicit Action Button** | Current rows still use a terminal chevron | Replace the chevron-only treatment with a labeled `RUN` or action-specific button. |
| **Apply / Discard / Stay Modal** | Rejection logic in C++ host | Create `ModalDialog.as` component hooked to bridge close events. |
| **Controller D-Pad Navigation** | B-button Back verified | Wire directional gamepad events to focus manager in C++ host adapter. |
| **Tactile SFX Triggers** | Not implemented; original event names unverified | Discover valid native events and prove menu-volume/spam behavior before adding any bridge surface. |

---

## 10. Multi-Resolution & Display QA Checklist

- [ ] **720p (1280×720)**: Verify text legibility of 13px help text; ensure no clipping on safe borders.
- [ ] **1080p (1920×1080)**: Baseline resolution; verify 1:1 pixel crispness and 12-row ordinary viewport fit.
- [ ] **1440p (2560×1440) & 4K (3840×2160)**: Verify vector scaling without raster artifacting.
- [ ] **21:9 Ultrawide (3440×1440)**: Confirm centered 16:9 canvas pillarboxing with clean background framing.
- [ ] **Gamepad-Only Navigation**: Verify 100% traversal of all controls and modals without mouse interaction.
- [ ] **Keyboard-Only Navigation**: Verify Tab, Arrow keys, Enter, Space, Escape, and shortcuts.
- [ ] **Colorblind Accessibility**: Verify all states and tiers are distinguishable in grayscale mode.
