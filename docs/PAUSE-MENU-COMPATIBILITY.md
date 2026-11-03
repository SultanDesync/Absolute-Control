# Absolute Control entry and coexistence

The 2026-09-04 product decision retains Absolute Control's native Scaleform host, specialized
instruments and provider-owned transactions. The PauseMenu entry is now **ABSOLUTE CONTROL**. It names
the destination explicitly; the project remains **Absolute Control**. Existing exports, action ID
and internal menu names remain stable.

## Implemented hardening

- PauseMenu injection is optional. Core runtime/relocation validation and native factory retention
  still gate host readiness. A conflicting lifecycle callsite disables that integration without
  rejecting the host or modifying/chaining the foreign hook. Provider page opens and an explicitly
  configured recovery hotkey remain available; no new global key is silently assigned.
- Snapshot the existing visible rows in order and append only Absolute Control's row. Existing
  row objects and members are passed through. The operation is bounded to 128 existing rows.
- Our row carries `_absoluteControlPanelEntry=true` and action `0x534C4F50`. A numeric match alone
  does not establish ownership. A foreign collision, duplicate owned row or malformed owned
  identity rejects insertion without replacing the list.
- Handle actions represented as uint, nonnegative int or integral Number. Reject fractional,
  non-finite and out-of-range values. Activation requires our action and our selected row;
  unrelated events continue through existing listeners.
- Snapshot with `GetDataForEntry` without moving selection; native append initialization preserves
  the prior index, which is checked after publication. Readiness
  retries are bounded. A rejected list/collision ends work for that menu open; successful injection
  also stops, so there is no permanent reinsertion loop competing with another mod.
- Retain the PauseMenu underlay and balanced audio lease, optional recovery-key policy, and OSF
  input-ownership guard. The host does not replace `pausemenu.swf` or load OSF UI.

For a missing PauseMenu entry, the current manual recovery configuration is:

```ini
; AbsoluteControlPanel_Custom.ini
[ControlPanel]
OpenHotkey=0x71
```

This explicitly binds F2. Choose an unused key; the shipping default stays `0x00` (unbound).
The direct-open path has a separate ultrawide acceptance requirement.

## Current limits

This remains a guarded adapter for the known Starfield 1.16.244 PauseMenu. It relies on the existing
Scaleform list path, selected-entry access, native row data shape, `InitializeEntries`, and capture
event. The appended row still derives its data
shape from an existing row. A later third-party rebuild can remove our entry after the one-shot
operation. Different custom PauseMenu structures can reject injection. These failures should not
disable provider registration or configured direct activation.

The ownership marker survived the combined runtime test below. Label fit and input behavior still
need qualification across supported layouts; policy tests do not establish universal compatibility.

## Next injection design to qualify

Investigate an additive operation at the native action-list publication boundary, after the original
builder has produced its actions, rather than repeatedly rebuilding the displayed list. Preserve
every existing action and its order, add one owned action per rebuild, and dispatch only that action.
Prefer a verified event/list-extension seam that permits multiple consumers.

Do not simply move onto OSF UI's Quit-row callsite: both projects would then compete for the same
hook. Do not accept an arbitrary patched target as callable merely because its pointer is executable.
Any chaining design needs a verified calling convention, original-call semantics and load-order tests.
No new native offsets or hook are introduced by this checkpoint.

## Co-installed runtime diagnosis (2026-09-18)

ResearchDev's bounded read-only inventory distinguishes entry data from visible clips. It uses
the public `GetDataForEntry` and `GetClipByIndex(index, false)` accessors without changing
selection or rebuilding the list. Samples occur before/after AC publication and at movie frames
1, 2, 10, 60, 180 and 600, with an 80-snapshot process cap. These diagnostics are excluded from
the canonical plugin.

With the installed OSF UI 1.6.0 and AC Research host both confirmed loaded, the initial combined
run showed 11 stock entries, 12 after AC publication, and 13 after OSF publication. Both owned AC
action `0x534C4F50` and OSF action `100` remained in the data. The list reported `itemsShown=12`,
`scrollPosition=0`, `maxScrollPosition=1`. The OSF row's clip contained `MOD MENUS` but was marked
`visible=false`, `itemIndex=-1`. The screenshot showed AC as the bottom visible row. This is
viewport overflow in that observed session, not evidence that either injector erased the other.

The vanilla scrolling-container update hides incomplete rows outside its container height;
keyboard selection scrolls an offscreen entry into view. A second co-installed run confirmed this:
12 bounded Down commands from index 0 revealed and selected `MOD MENUS` directly below
`ABSOLUTE CONTROL`, with the first stock row scrolled out. The subsequent accept completed and
admitted `OSFUI_FocusMenu`; the input arrived after the helper timeout, so timeout alone must not
be interpreted as cancellation. Full overlay UX and repeated-session tests remain separate proof
obligations. Do not add an injection delay or replace OSF's
entry based on the earlier visual-only assumption of an injection race. A discoverable overflow
or layout policy is the relevant design question for this observed overflow.

## Optional presentation adapter

`PauseMenuPresentation` is a separate component inside AC, using the existing movie-owned advance
callback rather than a second injection DLL. It observes the live entry count even after AC's
one-shot insertion has completed, so later OSF or other rows contribute to the required height.
It does not republish rows, reorder entries, replace a SWF or delay injection.

The adapter uses the vanilla public `borderHeight` setter, which recomputes the container rectangle
and refreshes the list. It measures row pitch, requests enough height for the count, never shrinks
an existing larger viewport, and reserves the lower 12% of the stage for the footer. Unsupported
geometry fails optional. Scrolling remains available when entries exceed the cap. The stage-based
cap is conservative; validation against other aspect ratios and custom menu replacements remains
necessary. In the 3440x1440 combined test, height grew from approximately 428 to 461.2 local units;
all 13 entries rendered and `maxScrollPosition` became zero.

The default leaves AC's text in the native menu colors. Purple remains unvalidated: a mismatched
highlight/text inversion state confounded the readability test, so it is not categorically rejected.
The optional, off-by-default purple experiment tints AC's glyph container alone.
The field's `textColor` alone is insufficient because vanilla timeline transforms override its
effective color. Only marker-and-action-owned AC rows qualify. The previous label transform is
restored only while it still matches AC's applied tint; pooled clips do not carry the tint onto
foreign entries. Backgrounds, icons and third-party labels are not themed.

`PauseMenuMatchFont` is a disabled experimental column-widening adapter, **not accepted for use**.
It produced decoupled highlight/text inversion even after moving Fill resizing to the inner shape.
The default leaves native row geometry and native long-label shrink behavior untouched. The
remaining experiment (for diagnosis only) permits modest widening for **ABSOLUTE CONTROL**.
It briefly measures the owned field at no more than 125% of its current width, using the existing
native shrink mode, then restores it before applying the measured layout. Text formatting, text
contents, glyph scales and row heights are unchanged. Text fields, the static inner fill shape
and `Border_mc` hit areas grow together to absolute minimum widths; new clips receive the
same width without cumulative growth. The animated `Fill_mc` parent is not resized: direct width
writes froze its selection color while the text continued inverting. Only a single-child,
single-frame, unmasked, axis-aligned fill with near-zero inner left/top bounds qualifies. Unknown
fill structures skip width adaptation. The list rectangle/positions are refreshed only when a
field or border grows.
The adapter rejects unrecognized scaled/rotated row geometry and bounds the column to the left
40% of the stage. It never shrinks a wider existing layout. This is a guarded vanilla-layout
adapter, not a universal replacement-menu layout engine.

At 3440x1440 with OSF UI 1.6.0 installed, native-color run
`pause-inventory-20260918-193037` measured field width 180 -> 206 and row width
213.95 -> 239.95 (about 12% column growth). Both AC and the stock reference row rendered
`textHeight=20.9`, `defaultTextFormat.size=22`, and glyph scales of 1. All 13 entries were
visible with `maxScrollPosition=0`. The unselected screenshot confirms native white text.
Three automated build/teardown cycles completed, followed by another open; cycle 5 retained the
same geometry and all 13 entries. A subsequent navigation command (id 32) timed out, so the
final selected-AC visual check is not claimed. No accept was issued. The per-run overwrite
comparison showed zero file-count and byte growth.
**That build failed selection validation:** the user observed the highlight remaining on Quicksave
while Photo Mode's text inverted, confirmed in a capture. It is not a successful width acceptance
result. The subsequent fix stops writing the timeline-owned fill parent's width and targets only
its static inner shape; that fix requires a fresh-instance selection test.
Fresh run `pause-inventory-20260918-193751` also failed that test: Quicksave's highlight remained
white while another row's text inverted. The hypothesis that avoiding only the Fill parent was
sufficient is rejected. Width adaptation is now off by default along with purple styling.

Rollback run `pause-inventory-20260918-194101`, Research DLL SHA-256
`6F0622E1F3E932B7026518B52AB7A33141483E33964525DC4498E02FF209DEF7`, disables both
width adaptation and purple styling while retaining viewport auto-fit. The user confirmed the
highlight/text synchronization was fixed. This is the retained working baseline. Native long-label
shrink remains in effect; full-size-label widening is unresolved and must not be described as
complete. The controlled rollback isolates the regression to the width-adaptation path, not to a
proven single property: avoiding only the Fill parent had not fixed it.
These observations do not qualify other aspect ratios, arbitrary replacement menus or controller/
mouse hit-testing; the research build remains separate from release acceptance.

These startup-only options may be overridden in `AbsoluteControlPanel_Custom.ini`:

```ini
[ControlPanel]
PauseMenuAutoFit=true
PauseMenuMatchFont=false
PauseMenuPurpleEntry=false
```

Only viewport auto-fit defaults to true; font matching and purple default to false. All remain subordinate to the
existing PauseMenu integration being enabled.
They can be disabled independently for compatibility testing. This does not add a separate
distributable compatibility plugin or a new injection hook.

Installed-version behavior must not be inferred from a newer OSF source checkout: the installed
1.6.0 logs report the Scaleform `PopulateMainList` path, while the newer reference source uses a
native action-list hook. Compatibility claims must identify which implementation was tested.

Promote a replacement only after it works with the stock menu, OSF UI, representative menu
replacements, both load orders and repeated list rebuilds. Until then, retain this bounded adapter
and independent activation. Renaming the entry alone does not establish compatibility.

## Title-screen MainMenu entry

`EnableMainMenuEntry=true` enables an independent **ABSOLUTE CONTROL** title-screen entry. It
uses the same owned action/marker and bounded row-preserving insertion as PauseMenu, but separate
movie identity, listener, retry and completion state. At SFSE PostPostLoad, startup validates the
runtime and installs the existing lifecycle hook before MainMenu creation, with no additional offset.
Factory registration, providers and input services remain at PostDataLoad. The early title row is
disabled until the native host is ready; activation independently checks that readiness. A queued
main-thread attachment at PostDataLoad remains as a catch-up path for an already-open MainMenu.

Injection waits for a visible, interactive MainPanel, not for Continue or a loaded save. Same-movie
load/settings returns re-arm on interactivity transitions. A census detects removal of the owned
entry after publication; at most four such reinsertions are allowed per interactive period to avoid
an unbounded fight with a foreign rebuilder. Identity conflicts stop that attempt. Native row order
and data are retained. The title entry uses the public row `Configure("shrink", false)` text option
and `SetEntryText` refresh. The title field auto-grows (measured 201.2 wide, text offset 17, row width
213.95), defeating shrink and extending past the row edge. Only the owned label field is constrained:
`autoSize=none`, width derived from the native border bounds and symmetric padding (179.95 here).
Row geometry, fill/hit-area sizing and native selection transforms remain untouched. Run
`main-menu-entry-20260918-201020` measured field width 179.95, text width 175.95 and text height 18.2
before and after selection. Screenshots confirm the full label fits in both states and the white
highlight follows AC with black text. The guarded research selection command reported
`selected=true activation=false`. This is a local title-screen validation, not a general UI-overhaul
compatibility claim. Research DLL SHA-256:
`6ECB63F24021D7438E92C9156777EB9D2AF5AB197E7CD54EC3630E2D7C360E41`.

Live run `main-menu-entry-20260918-200506` inserted on the first eligible advance at timestamp
1789761931242, 15,698 ms before host readiness at 1789761946940. Native startup republished its list;
the owned entry was restored on the next eligible advance with selection preserved. The user accepted
the early-visible, temporarily disabled behavior. Per-run overwrite comparison found zero growth.

The displayed session claims an explicit `MainMenu` origin. It retains the title underlay, skips
the gameplay PauseMenu audio-mode lease, and reveals the resident title on accepted close. It must
never synthesize a MainMenu Show if the underlay disappeared during a load transition. Pause-origin
recovery remains distinct. Hotkey/provider guards against opening over MainMenu remain unchanged;
the owned title entry is the deliberate pre-save route. Unregistered providers are allowed to be
absent; subscriber actions still need their own domain-readiness checks.

The research runner's `mainMenuDiagnosticsOnly` mode stops before Continue and any load-confirmation
input. `select_main_entry` is a bounded, two-second, research-only selection command: it validates
the interactive MainMenu and owned identity, selects only that row, and never activates it.

## Validation before deployment or release claims

Automated checks cover ownership/collision decisions, numeric action decoding, disabled and foreign
activation, optional entry-failure isolation, the native build and the repository's existing gates.
On 2026-09-04, `tools/process/validate-current.cmd` passed all gates: native release build, 13/13
native tests, 9 SDK generator tests, deterministic codegen/compile fixture, catalogue, SWF provenance,
artifact fixtures, manifest and package validation. The report is
`artifacts/process/current-validation.json`; runtime and UX remain `not_run`. No deployment or game
launch was performed for this checkpoint.

In Starfield, verify stock and two-host cases with both load orders: exactly one distinct row,
unchanged foreign rows/order/selection, our marker surviving list initialization, correct keyboard/
mouse/controller activation, no clipping of the longer label, dirty-close rollback and capture release,
pause/audio return, 21:9/32:9 behavior, and 25 open/close cycles with no crash or stuck input. Also
exercise a conflicting lifecycle hook: the foreign hook must remain untouched and provider/hotkey
opens must still reach the registered host. Keep this build out of release claims until those pass.
