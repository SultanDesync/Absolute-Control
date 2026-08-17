# AbsoluteHOTAS — UX Design Brief for Absolute Control

> **Status:** First UX direction, ready for style-guide and interaction review
> **Date:** 2026-08-17
> **Authority:** Visual hierarchy, information architecture, interaction behavior, and
> state presentation for hosting `absolute.hotas` inside Absolute Control.

## 1. How to use this brief

This document translates the
[AbsoluteHOTAS feature inventory](../../AbsoluteHOTAS/docs/ABSOLUTE-CONTROL-HOTAS-FEATURE-INVENTORY.md)
into a focused UX direction. Read it with the
[shared Absolute Control design brief](ABSOLUTE-CONTROL-DESIGN-BRIEF.md), which remains
authoritative for the shell, canvas, input model, command bar, typography, and common
components.

This is a UX document. It does not declare engineering status, runtime reliability,
release timing, file locations, supported game versions, or retirement of the legacy
workbench. Device names, bindings, percentages, counts, telemetry, and status copy in
wireframes are illustrative content used to exercise the layout. At runtime, the UI
shows provider-reported values and capabilities.

The UX must not invent a user-facing feature merely to make a mockup interesting. Where
a visualization needs representative data, label it as an example and keep its anatomy
compatible with the feature inventory.

## 2. Product experience

AbsoluteHOTAS should feel like a purpose-built flight-control workbench within Absolute
Control: technically capable, direct, and reassuring under live hardware input. It must
serve two depths without becoming two products:

1. **Get flying** — detect hardware, bind core axes, choose a throttle behavior, bind
   important actions, and return to the game.
2. **Build a cockpit** — tune six-axis behavior, create binding layers, compose macros,
   calibrate multiple devices, and inspect compatibility state.

The governing experience principles are:

- **Behavior before mechanism.** Lead with what the ship will do, then reveal tuning.
- **Bind, observe, tune, verify.** Hardware input and interpreted output stay visually
  connected.
- **Progressive disclosure without concealment.** Advanced detail may collapse, but the
  existence and purpose of major capabilities remain visible.
- **One control model.** Binding Layers are an approachable view of profiles, not a
  separate simplified configuration system.
- **Truthful state.** Active, editing, inherited, unavailable, stale, and disconnected
  are distinct states.
- **Flight semantics over generic settings.** Rotation, translation, thrust, detents,
  aim, and control ownership provide the organizing language.
- **Dense, not cramped.** Prefer aligned rows, compact summaries, and selected-record
  workspaces over universal large cards.

## 3. Suite boundaries in the UI

AbsoluteHOTAS appears as one module in the persistent Absolute Control sidebar. Its
pages do not absorb sibling modules simply because their behavior overlaps in flight.

```text
ABSOLUTE CONTROL
  ├─ AbsoluteHOTAS
  │    Flight axes · throttle · ship buttons · aiming · profiles · macros · devices
  ├─ Absolute Head Tracking
  │    OpenTrack · camera pose · recenter · pose shaping
  └─ AbsoluteZero
       Idle mouse-centering policy

Optional Absolute Flight Runtime
  Reports shared flight-lane ownership and arbitration state when present
```

Presentation rules:

- HOTAS may summarize a sibling module's relevant state and offer **Open module**.
- HOTAS never reproduces Head Tracking or AbsoluteZero settings on its own pages.
- Camera Look is not a HOTAS page.
- Idle mouse centering is not a HOTAS setting.
- Absolute Power remains a separate module, not a HOTAS tab.
- Coordinator state, when available, is explanatory status rather than a duplicate
  settings surface.

### 3.1 Mouse steering language

When HOSAM is active, use direct ownership copy:

> **Pitch and yaw: Starfield mouse steering**
> Your HOTAS pitch/yaw bindings are preserved and become active again when HOSAM is off.

Roll, strafe, throttle, ship buttons, and independent aim remain visually available.
If AbsoluteZero is installed, link to it as the owner of idle centering. Do not imply
that AbsoluteZero owns mouse input or steering.

## 4. Canvas and navigation

AbsoluteHOTAS inherits the shared 1920×1080 logical stage, safe gutter, persistent
module sidebar, selected-control help, and input-aware command bar. It does not introduce
an inner desktop window or a second application frame.

### 4.1 Page architecture

Use one horizontal page-tab row. Short tab labels keep all nine routes scannable; the
workspace title supplies the full page name.

| Group cue | Tab label | Workspace title | Stable ID |
|---|---|---|---|
| Flight Controls | Overview | Setup Overview | `hotas-setup` |
| Flight Controls | Axes | Flight Axes | `hotas-flight-axes` |
| Flight Controls | Buttons | Ship Buttons | `hotas-ship-buttons` |
| Flight Modes | Throttle | Throttle Setup | `hotas-throttle` |
| Flight Modes | Aiming | Aiming & Combat | `hotas-aiming` |
| Advanced | Profiles | Profiles & Layers | `hotas-profiles` |
| Advanced | Macros | Macros | `hotas-macros` |
| Advanced | Devices | Devices & Calibration | `hotas-devices` |
| Advanced | Diagnostics | Plugin & Compatibility | `hotas-diagnostics` |

Use restrained separators or small noninteractive group labels to communicate the three
clusters. Do not add a second clickable navigation level. If measured localization
expansion prevents all tabs fitting, use a horizontal tab viewport with explicit
previous/next overflow controls and a position cue; do not wrap tabs into an unstable
second row.

```text
MODULES            FLIGHT CONTROLS         FLIGHT MODES       ADVANCED
AbsoluteHOTAS      Overview  Axes  Buttons | Throttle  Aiming | Profiles  Macros  Devices  Diagnostics
Absolute Power
Head Tracking      ───────────────────────────────────────────────────────────────────────────────────
AbsoluteZero       FLIGHT AXES
                   Bind, observe, and tune each flight direction.
                   [workspace]
```

### 4.2 Opening and deep links

- Fresh or incomplete setups enter Overview.
- Configured setups may reopen the last HOTAS page used during the current menu session.
- Readiness cards and contextual actions focus the exact destination section after a
  page change.
- Back follows the shared host lifecycle.
- Long pages retain a proportional scrollbar and clear above/below continuation cues.
- Focus restoration returns to the initiating readiness card, row, or action after a
  modal or linked workflow closes.

## 5. HOTAS visual language

### 5.1 Axis icon family

Create six monochrome vector icons at a nominal 24×24 px logical size. They must remain
legible at the host's smallest supported presentation size and must not require an icon
font.

| Axis | Geometry | Semantic rule |
|---|---|---|
| Throttle | Twin rails, diagonal lever, knob | A mechanism moving through a gated range |
| Pitch | Center/axis cue with a vertical-plane curved arrow | Curved arrow means rotation |
| Yaw | Vertical-axis cue with a horizontal-plane curved arrow | Curved arrow means rotation |
| Roll | Forward craft cue with a near-complete circular arrow | Rotation around forward axis |
| Strafe Lateral | Center craft/square with left/right double arrow | Straight arrow means translation |
| Strafe Vertical | Center craft/square with up/down double arrow | Straight arrow means translation |

The icon is a recognition cue, never the only label or status indicator. Source geometry
for the current family is documented in the feature inventory and may be recreated as
host-owned vector paths.

### 5.2 Domain accents

Use the shared Control state tokens for focus, success, warning, error, dirty, disabled,
and stale. HOTAS may add three restrained domain accents:

| Domain | Role | Companion non-color cue |
|---|---|---|
| Thrust | Throttle and speed-related headers/markers | Throttle icon and solid rail |
| Rotation | Pitch, yaw, roll, and aim | Curved-arrow icon family |
| Translation | Lateral and vertical strafe | Straight double-arrow icon family |

Do not assign error/hazard meaning to ordinary negative input, reverse, maximum thrust,
or boost. Those are flight regions, not validation failures. Graph regions use labels,
boundary shapes, and optional patterns in addition to color.

Exact values belong in the shared style-token update. The legacy amber/cyan/purple
mapping is a useful continuity reference, not a hard-coded mandate.

### 5.3 State grammar

The same visual vocabulary appears throughout the module:

| State | Required presentation |
|---|---|
| Bound | Binding value plus **Bound** or equivalent status; never green alone |
| Unbound | Explicit **Not bound**, clear Bind action, neutral/warning treatment based on importance |
| Responding | Live marker or compact activity cue separate from the bound state |
| Inactive by mode | Preserved value remains readable; muted treatment and reason copy |
| Inherited | **From Primary** source label |
| Overridden | **Overridden in [layer]** plus **Use Primary** action |
| Capturing | Listening modal owns focus; source row shows capture state |
| Disconnected | Last known binding retained; device status and recovery link shown |
| Stale telemetry | Last value remains visible but marked stale; no simulated motion |
| Unavailable | Reason and any valid alternative; no decorative disabled control without explanation |
| Dirty | Changed value and page/command-bar state distinguishable from focus and live value |

### 5.4 Range and telemetry anatomy

Live visualizations must distinguish the concepts they display without turning every
sample into a focus target.

For an axis, the visual hierarchy is:

1. range and center;
2. deadzone/saturation regions;
3. current logical input;
4. optional hardware input reference;
5. interpreted/injected output when available; and
6. stale or disconnected state.

For throttle, add labeled behavior regions and captured landmarks. The graph never
invents an aerodynamic penalty region or response-curve editor. It shows the actual
selected throttle behavior.

Provide a compact legend beside or immediately below the graph. The selected-control
help region contains the long explanation.

## 6. Shared interaction patterns

### 6.1 Editing context

Always distinguish what is being edited from what is active in the game.

```text
Editing: Primary controls                         Active in game: Landing Controls
```

On Ship Buttons, the approachable label is **Editing bindings for**. On Profiles &
Layers, expose full profile metadata. Other pages show a compact editing-context strip
without expanding advanced profile management.

Changing the edit target while dirty invokes the host's **Apply / Discard / Stay**
workflow before the selection changes.

### 6.2 DirectInput capture modal

The host modal supplies consistent framing; HOTAS supplies the prompt and reported
state. The UX needs these variants:

```text
CAPTURE INPUT
Binding: Pitch axis

Move the desired axis through a clear range.
Listening to connected flight devices…

[Cancel capture]
```

```text
CAPTURE INPUT
Binding: Combat selector position

Move the selector to the desired position.
The last settled position will be used.

[Cancel capture]
```

States:

- listening;
- candidate detected/settling;
- captured into draft;
- no-input timeout;
- device disconnected;
- cancelled by user;
- cancelled because page/profile/menu changed; and
- provider unavailable.

Success returns focus to the source binding row and visibly marks the page dirty. A
captured result does not imply it has already been applied.

### 6.3 Selected-record workspaces

Profiles, macros, and devices use one populated selector as the authoritative record
choice. Do not pair a permanent left list with a second selected-record dropdown or
Previous/Next controls.

The selector options may show compact secondary metadata:

```text
Combat Layer                 Toggle · 5 overrides
Landing Controls             Selector · Active
Primary controls             Base
```

Selection alone does not dirty the page. Lifecycle actions explicitly name the selected
record.

### 6.4 Destructive and repository operations

Reset, delete, import, clear calibration, and any future detach/materialize operation
use content-specific confirmation. The modal names what changes, what is preserved, and
whether a backup/recovery path exists. Do not use generic “Are you sure?” copy.

## 7. Page specifications

### 7.1 Setup Overview (`hotas-setup`)

**User question:** “Can I fly, and where should I go next?”

Overview is a readiness map, not a score or duplicate editor. It presents five to seven
compact rows/cards with one primary resolving action each.

```text
SETUP OVERVIEW
Editing: Primary controls                         Active in game: [provider value]

DEVICES                    [Detected]             Open Devices
Connected flight hardware is available.

PRIMARY FLIGHT AXES         [4 of 6 bound]         Open Flight Axes
Throttle and pitch respond. Yaw and vertical strafe need attention.

THROTTLE BEHAVIOR           [Standard HOTAS]      Open Throttle Setup
Logical input and commanded output are available.

SHIP ACTIONS                [Bindings present]    Open Ship Buttons
Review weapon, flight-system, and context controls.

PROFILES & LAYERS           [Primary controls]    Open Profiles
One additional binding layer is available.

COMPATIBILITY               [Ready / Attention]   Open Diagnostics
Show only provider-reported actionable state.
```

UX requirements:

- Device and count examples do not prescribe a required hardware setup.
- Do not define a universal essential-action subset without a product decision. A
  neutral bound summary is sufficient for the first design.
- HOSAM may make pitch/yaw truthfully **Mouse steering** rather than unbound.
- Working configurations do not receive false warnings for optional axes/actions.
- The next incomplete task may be emphasized, but no gamification or completion score.
- **Continue configuring** is a page action. Apply/Cancel/Back remain in the shared
  command bar.

### 7.2 Flight Axes (`hotas-flight-axes`)

**User question:** “What is controlling each flight direction, and does it respond the
way I expect?”

Page order:

1. compact flight-control enable/state summary;
2. six-item quick navigator with icon, short label, and state;
3. Thrust section;
4. Rotation section;
5. 6-DOF Translation section;
6. Reverse strategies;
7. Digital fallback controls; and
8. HOSAM ownership card.

The quick navigator communicates the entire page before scrolling:

```text
[Throttle  Bound] [Pitch  Bound] [Yaw  Mouse] [Roll  Bound] [Lat  Unbound] [Vert  Bound]
```

#### Standard axis card

Cards are content-sized. Do not force all variants into a fixed height.

```text
[icon] PITCH                                      BOUND · RESPONDING
Ship rotation around the lateral axis

Input source        [VKB device · Y]              [Rebind] [Clear]
Control mode        Direct ship rotation
Invert              [Off]
Sensitivity         [slider] 1.00
Saturation          [slider] 100%
Center deadzone     [slider] 3.5%

Pitch Down                 Center                         Pitch Up
|--------------------------|-------------------------------|
       hardware marker     logical marker       output marker
[legend]                                              [telemetry fresh]
```

Requirements:

- Throttle, pitch, yaw, roll, lateral strafe, and vertical strafe share the card
  grammar while retaining axis-specific controls.
- Throttle shows its current positional/rate behavior and **Configure Throttle
  Behavior…** without duplicating the full landmark editor.
- Pitch/yaw show Direct, Aim-driven, or Mouse steering ownership.
- HOSAM leaves stored pitch/yaw bindings readable but inactive.
- Live graphs remain a primary verification surface rather than a tiny sparkline.
- Binding, logical input, and interpreted output must be visually distinguishable.

#### Reverse and digital fallbacks

Present reverse as three understandable strategies:

- reverse zone on the main throttle;
- held Reverse button; and
- dedicated Reverse analog axis.

The section states the effective strategy and precedence without implying unsupported
behavior. Digital fallback rows cover:

- Digital Reverse;
- Digital Roll Left;
- Digital Roll Right;
- Digital Strafe Left;
- Digital Strafe Right;
- Digital Strafe Up; and
- Digital Strafe Down.

Roll and strafe strength follow the binding rows.

#### HOSAM card

```text
MOUSE STEERING (HOSAM)                                      [On]
Pitch and yaw currently use Starfield mouse steering.
Your HOTAS bindings are preserved.

Independent aim: [current mode]       Idle centering: [Open AbsoluteZero]
```

### 7.3 Ship Buttons (`hotas-ship-buttons`)

**User question:** “What does each physical control do in flight, menus, and cockpit
contexts?”

Page order:

1. pinned Binding Layer context;
2. Direct Ship Controls;
3. Navigation & Context Controls;
4. Cockpit & Docking Shortcuts;
5. Flight Assist; and
6. Keyboard & Mouse Shortcuts.

```text
Editing bindings for  [Primary bindings ▼]         [+ Add binding layer]
Active in game        Landing Controls
```

#### Binding row

```text
Fire Boosters            VKB device · Button 4       DIRECT       [Rebind]
                         From Primary                 Available
```

Focused-row detail appears in selected-control help or a local disclosure:

- behavior and context;
- Direct, Context, or Keyboard compatibility meaning;
- availability/reason;
- resolved keyboard output/source when relevant;
- selected compatibility method and valid alternative; and
- inherited/overridden ownership.

Do not describe Direct as infallible or zero-delay. Method badges describe the route,
not a quality ranking.

#### Required action groups

| Group | Actions |
|---|---|
| Weapons & Combat | Fire Weapon 1, Fire Weapon 2, Fire Weapon 3 |
| Flight Systems | Fire Boosters, Switch Flight Modes, Ship Action 1, Open Scanner, Repair Ship, Ship Alternate Control, Cruise, Autopilot On / Off |
| Camera | Toggle POV, Zoom Camera In, Zoom Camera Out |
| Navigation & Context | Select / Accept, Back / Cancel, Navigation Up, Navigation Down, Navigation Left, Navigation Right |
| Cockpit & Docking | Undock / Take-Off, Get Up, Exit Ship |

The page does not need to display all 23 rows at once. Section counts, proportional
scroll, and clear continuation communicate the remaining content.

#### Menu control reuse

Use a compact specialist disclosure under Navigation & Context:

- Pitch axis navigates Up/Down;
- Yaw axis navigates Left/Right;
- Primary Weapon selects/accepts;
- vertical/horizontal inversion; and
- engage/release thresholds with explanatory hysteresis help.

#### Flight Assist

Four binding rows: Hold Current Throttle, Full Stop, Cruise 50%, and Cruise Max. Copy
explains that these change HOTAS throttle authority instead of emitting a Starfield key.

#### Keyboard & Mouse Shortcuts

Use a compact repeatable table with controller binding, one explicit output, and
Bind/Clear/Remove. Include Add Shortcut, the menu-navigation preset, and **Build a chord
or sequence…** deep link. Do not call a single shortcut a macro.

### 7.4 Throttle Setup (`hotas-throttle`)

**User question:** “What should this lever/stick do, and how is its travel interpreted?”

#### Behavior selection

Present the six recipes as a compact selectable list or two-column choice set with a
plain-language preview. Do not require six oversized cards.

1. Standard HOTAS throttle
2. Forward throttle with reverse zone
3. Self-centering HOSAS — release to hold
4. Self-centering HOSAS — release toward idle
5. Detents and boost
6. Custom

Selecting a recipe previews its result and affected settings. **Use this behavior** is
an explicit action. Replacing a custom setup invokes a confirmation summary.

#### Logical throttle visualization

The graph reflects the chosen behavior. A representative positional layout is:

```text
LOGICAL THROTTLE                                      Live lever: [provider value]

| Reverse | Stop/Idle |----------- Forward thrust -----------| Full | Boost |
|---------|-----------|---------------------------------------|------|-------|
           ^ zero       ^ optional detent                     ^ boost start

Interpreted region: [provider value]       Commanded throttle: [provider value]
Reverse: [state]                            Boost: [state]
```

Regions that are disabled disappear or become clearly inactive. Boundaries and markers
remain labeled. Reverse is described according to the actual runtime behavior; do not
rename boost as afterburner or introduce an unrelated pitch-penalty band.

#### Positional editor

- idle plateau;
- symmetrical deadzones;
- detent/center position and width;
- guided Capture Center/Detent;
- reverse zone and guided Capture Zero-Thrust;
- dead-stop range;
- boost zone and guided Capture Boost;
- full-thrust plateau; and
- links/summaries for held reverse and dedicated reverse axis.

#### Rate/self-centering editor

- ramp rate;
- decay rate;
- reverse velocity gate;
- Pilot Turn Assist;
- Always, While held, or Toggle activation; and
- conditional activation binding.

Guided capture uses user-language prompts and keeps raw integer landmarks in advanced
detail. Captured markers visually land at the live logical position.

### 7.5 Aiming & Combat (`hotas-aiming`)

**User question:** “Does my aiming input steer the ship, move the reticle independently,
or both?”

Page order:

1. Aim System enable/state;
2. effective mode explanation;
3. analog independent aim;
4. smoothing;
5. digital aim override; and
6. Aim Mode Toggle binding.

```text
AIM SYSTEM                                                        [On]
Current mode: Independent Aim & Steer
Separate aim inputs move the reticle while the primary stick steers the ship.

Aim Yaw       [device · axis]  [Invert]  Sensitivity [slider]      [Rebind]
Aim Pitch     [device · axis]  [Invert]  Sensitivity [slider]      [Rebind]
Smoothing     [slider] [provider-formatted value]

DIGITAL AIM
Aim Left  Aim Right  Aim Up  Aim Down  Aim Center        Aim speed [slider]
Aim Mode Toggle [binding]
```

A compact 2D reticle/input visualization may appear when genuine live data is
available. It must have labeled axes, center, current input and stale/disconnected
states. Do not fabricate motion or express smoothing in invented time units.

HOSAM appears as a concise steering-ownership summary with a link to Flight Axes, not a
second independent setting implementation.

### 7.6 Profiles & Layers (`hotas-profiles`)

**User question:** “Which control set am I editing, how is it activated, and what does it
override?”

Use one authoritative selected-profile dropdown followed by a content-sized detail
workspace.

```text
PROFILE / LAYER     [Landing Controls · Selector · Active ▼]
Editing             Landing Controls
Active in game      Landing Controls

Name                Landing Controls                         [Rename]
Kind                Sparse overlay
Activation          Selector position
Trigger             S-TECS · Position 3                     [Rebind] [Clear]
Flight controls     Parked for this profile
Overrides           6 values

OVERRIDES
Fire Boosters       Button 4               [Use Primary Binding]
Pitch sensitivity   0.65                   [Use Primary Value]
```

Actions are grouped by consequence:

- **Create:** Add Binding Layer, Add general overlay, Export Main as full profile.
- **Activation:** While held, Toggle on/off, Selector position, and Main return/detent
  behavior where applicable.
- **Repository:** Import full profile as Main, Reset Main with backup.
- **Navigation:** Edit this profile's Axes, Buttons, Throttle, Aiming, or Macros.
- **Lifecycle:** Rename and any future delete/detach action only when its complete
  confirmation behavior is defined.

Every value in a sparse overlay shows **From Primary** or **Overridden in [profile]**.
**Use Primary** removes the override; it is not presented as copying a value.

#### Add Binding Layer flow

1. Name the layer.
2. Capture its activation control.
3. Choose While held, Toggle on/off, or Selector position.
4. Open Ship Buttons with the new layer selected.

Use the explanation:

> This layer starts with your Primary bindings. Change only the controls that should
> behave differently.

### 7.7 Macros (`hotas-macros`)

**User question:** “What happens when I press this trigger, in what order, and for how
long?”

Use one authoritative macro selector. The selected macro editor contains name, trigger,
Turbo repeat, validation state, and an ordered sequence.

```text
MACRO              [Example macro · 2 steps ▼]                [+ New Macro]
Name               Example macro
Trigger            [device · button]                         [Rebind] [Clear]
Turbo repeat       [Off]

STEP 1             Navigation Left                            Tap ×2
                   Gap after step: 50 ms                     [Move] [Delete]

STEP 2             Fire Weapon 1 + Fire Weapon 2              Hold 250 ms
                   Gap after step: 50 ms                     [Move] [Delete]

[+ Add step]
```

Step anatomy:

- one or more simultaneous targets joined visually by `+`;
- named ship action or explicit keyboard/mouse target;
- Tap with repetition count, or Hold with duration;
- gap before the next step;
- reorder and delete; and
- add/remove chord target.

The UX displays friendly action names. It must not encode a Direct/Context/Keyboard
route into the macro target label because named actions follow their selected method.
Unknown stored tokens remain visibly identifiable instead of disappearing. Incomplete
draft macros remain editable and are distinguished from runnable macros.

### 7.8 Devices & Calibration (`hotas-devices`)

**User question:** “What hardware can HOTAS see, is it responding, and is its range
calibrated?”

Use one authoritative device selector with compact identity metadata.

```text
DEVICE              [VKB Gladiator · Connected · 4 axes ▼]
Product             VKB Gladiator
Identity            [provider-formatted stable detail]
Capabilities        4 axes · 28 buttons · 1 POV
Status              Connected

AXIS       SAVED MIN      SAVED MAX      CURRENT       ACTIVITY
X          [value]        [value]        [value]       [range meter]
Y          [value]        [value]        [value]       [range meter]
Z          [value]        [value]        [value]       [range meter]
Rx         [value]        [value]        [value]       [range meter]

[Start full calibration]   [Clear saved calibration]
```

Use DirectInput axis names such as X, Y, Z, Rx, Ry, Rz, Slider0, and Slider1 unless a
known binding provides a separate semantic label. Calibration records range extremes;
do not imply that it creates a saved center value.

#### Full-device calibration modal

1. Identify the selected device.
2. Explain that every axis should move through its complete physical range.
3. Show observed min/max expansion and activity for all detected axes.
4. Keep Commit and Cancel persistent.
5. On disconnect, preserve prior values and show a clear recovery state.

Duplicate-name reassignment is a separate guided operation. It explains which two
devices are being distinguished, previews the scope of binding reassignment, and stages
the result as a draft until Apply.

### 7.9 Plugin & Compatibility (`hotas-diagnostics`)

**User question:** “Why is HOTAS active, parked, unavailable, or behaving differently
from expected?”

Organize the page into five sections:

1. **Runtime controls** — Activate, Stop, and menu/open binding where available.
2. **Pilot context** — outside-seat behavior, automatic detection, latch, current state.
3. **Ship-control compatibility** — route availability, selected methods, resolved
   compatibility outputs, refresh action.
4. **Editing and devices** — active profile, editor target, unresolved devices, last
   apply/readback state.
5. **Suite ownership** — relevant Head Tracking, AbsoluteZero, and coordinator state
   with Open Module actions.

```text
PLUGIN & COMPATIBILITY

RUNTIME CONTROLS
Activate HOTAS output        [binding]                       [Rebind]
Stop HOTAS output            [binding]                       [Rebind]
Open Absolute Control        [binding/status]                [Rebind]

PILOT CONTEXT
Outside the pilot seat       [Park flight controls ▼]
Automatic detection          [On]
Context latch                [slider] [provider value]
Current state                In flight / Parked / Suspended / Unknown

COMPATIBILITY
Named ship controls          [provider status]
Keyboard binding cache       [provider status]               [Reload]
Unavailable actions          [count]                         [Review]
```

Normal contextual absence is quiet status, not an error. Actionable incompatibility
uses warning/error treatment and states what the user can do. Detailed rows may expose
per-action method and resolution source without turning the page into a developer log.

Version strings, paths, host capabilities, and support actions are populated from
reported state. Mock values are not product promises. Copy/export support actions appear
only when the shared host design supports them.

## 8. Modal and operation studies

The HOTAS design package must show these complete flows rather than only their idle
buttons.

### 8.1 Apply / Discard / Stay

- Names AbsoluteHOTAS and the affected page/profile.
- Apply shows saving/progress then success or actionable failure.
- Discard states that the current draft is lost.
- Stay returns to the exact control and preserves focus.

### 8.2 Recipe overwrite

- Shows current behavior and selected recipe.
- Summarizes which categories change.
- States that binding and hardware calibration are preserved when that is the reported
  behavior.
- Offers Use Recipe or Keep Current Setup.

### 8.3 Profile operations

- Add Binding Layer guided flow.
- Rename with collision/validation state.
- Import-as-Main summary and backup result.
- Reset Main summary, preserved routing, and recovery result.
- Future delete/detach actions remain absent until their consequences are defined.

### 8.4 Calibration

- Preparing/listening.
- Active motion with observed ranges.
- No movement detected.
- Device disconnected.
- Commit success/readback.
- Cancel with prior values restored.

### 8.5 Capture

- Axis prompt.
- Button/POV prompt.
- Selector-settle prompt.
- Timeout.
- Device loss.
- Cancel and safe return.

## 9. Focus, input, and long-page behavior

Every primary workflow must be complete with controller, keyboard, and mouse.

- The module sidebar, page tabs, workspace, selected-control help, and command bar use
  the shared host focus zones.
- Axis quick navigation is a small set of six focus stops, not a second tab system.
- Graphs are one focus stop each. Their legend/help is read as a unit.
- Binding rows expose one primary Bind/Rebind action; Clear and advanced method actions
  remain reachable without bloating every row.
- Selected-record dropdown changes selection without dirtying the page.
- Macro steps are focus groups; individual targets are entered only while editing a
  step.
- Scroll position follows focus and never hides the active control behind the command
  bar.
- Mouse wheel over the workspace scrolls; it does not alter sliders.
- Input prompts are device-aware. Do not hard-code Xbox, keyboard, or mouse glyphs into
  HOTAS page art.
- Capture may listen to HOTAS hardware, but HOTAS devices do not become menu-navigation
  devices by implication.

At 1280×720 physical output, core labels, values, status, and focus remain legible. At
4K they remain crisp vector geometry. Ultrawide keeps the shared centered 16:9
composition. Long labels wrap or move into selected-control help according to the host
component specification; they do not shrink below the shared minimum type size.

## 10. UX deliverables

The next visual design pass should provide:

1. A 1920×1080 HOTAS shell composition using the shared sidebar, tabs, workspace, help,
   and command bar geometry.
2. High-fidelity page treatments for Overview, Flight Axes, Ship Buttons, Throttle
   Setup, and Aiming & Combat.
3. Selected-record treatments for Profiles, Macros, and Devices.
4. A structured Diagnostics page with normal, warning, unavailable, and terminal
   provider states.
5. The six-axis vector icon family with source geometry or an implementation-neutral
   vector specification.
6. Axis and throttle range-meter anatomy, legends, region patterns, and stale states.
7. Binding-row states for Direct, Context, Keyboard compatibility, inherited,
   overridden, unbound, disconnected, and capturing.
8. Modal studies from section 8.
9. Keyboard/controller focus diagrams for one long axis page, Ship Buttons, the macro
   editor, and calibration.
10. Updated shared tokens or proposed HOTAS domain-token additions, with non-color cues
    and 720p contrast checks.
11. Companion status/link treatments for Absolute Head Tracking and AbsoluteZero that
    preserve the modular boundary.
12. A visual QA sheet for clean/dirty, active/editing, live/stale, 720p/1080p/4K, and
    keyboard/mouse/controller states.

## 11. UX acceptance criteria

A successful direction:

- makes all nine pages discoverable without turning groups into nested navigation;
- communicates the entire six-axis page before the user has scrolled through it;
- keeps live graphs prominent and readable without overwhelming scalar tuning;
- distinguishes bound, responding, inherited, overridden, inactive, stale, and
  unavailable states without color alone;
- makes HOSAM ownership clear while preserving visible pitch/yaw bindings;
- gives the 23 ship actions a scannable hierarchy and truthful route badges;
- keeps Menu Reuse, Flight Assist, and Keyboard & Mouse Shortcuts discoverable;
- makes throttle behavior understandable before exposing landmark mechanics;
- supports independent aim, aim-driven steering, and digital aim without absorbing
  Head Tracking;
- uses one authoritative selected-record control on Profiles, Macros, and Devices;
- makes sparse inheritance and **Use Primary** understandable;
- depicts chords, sequence order, tap/hold, repetition, gaps, and incomplete macro
  drafts accurately;
- calibrates ranges without implying semantic axis roles or a saved center;
- distinguishes normal flight/context absence from actionable compatibility failure;
- provides equivalent keyboard, mouse, and controller paths;
- uses the shared host transaction, focus, help, and command-bar language;
- remains legible on a 720p-scaled composition and crisp at 4K; and
- stays implementable with deterministic vector geometry and bounded visible content.

This brief is ready for visual exploration when the design work preserves those
relationships. Runtime examples may change without invalidating the layout; the page
hierarchy, state grammar, and interaction flows are the design contract.
