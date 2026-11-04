# Absolute Control Panel Scaleform boundary

> **Status:** Current executable interface. Capability confidence and unfinished product widgets are
> tracked in [`CURRENT-STATE.md`](../docs/CURRENT-STATE.md); this file describes the movie/native
> boundary rather than the full release design.

This directory reserves the source and compiled-asset boundary for the native menu.

`src/AbsoluteControlPanelMenu.as` is the document/root coordinator. It owns the stable native
callback surface, stage listeners, and native command dispatch, while the imported `acp.ui`
package owns the separable menu responsibilities:

- `MenuSelectionState` owns selected module/page/control state and viewport normalization;
- `MenuShellRenderer` owns the sidebar, settings rows, help panel, scrollbar, and footer
  composition;
- `ControlWidgets` owns toggle, slider, choice, action, and binding rendering and semantics;
- `PointerInteraction` owns rendered hit regions, slider drag identity, and wheel regions;
- `PixelTextRenderer` owns the embedded diagnostic glyph set and text drawing; and
- `PanelLayout` and `PanelTheme` are the single sources for geometry and color constants.

These boundaries are intentionally coarse. They match runtime responsibilities that can be
validated independently without imposing a component framework on the pinned Flex compiler.
`dist/AbsoluteControlPanelMenu.swf` is built from source by
`tools/research/build-interface.ps1`; the adjacent build metadata records compiler, target,
PlayerGlobal commit, root-source compatibility hash, deterministic source-tree manifest/hash, and
output hash. Apache Flex embeds varying build metadata, so the pinned inputs and recorded source
tree are the reproducibility authority; byte-identical SWF hashes are not currently claimed.

Run `interface/tests/Test-SourceArchitecture.ps1` after source changes. It protects the public
document-class surface, checks that each extracted responsibility has one owner, caps coordinator
growth, and verifies that build provenance covers every ActionScript source.

The research movie uses only Flash platform drawing, including its tiny diagnostic alphabet;
it contains no Bethesda
assets, fonts, or components. The plugin scans and records plausible root paths in game. A clean
deployed run has proven the visual movie, `_root` bridge object, `ready(1)` callback, active-menu
insertion, and watchdog close.

The earlier magenta framebuffer sentinel has been removed. Current validation uses bridge model
acknowledgements, menu lifecycle evidence, and direct inspection during focused runtime passes.

The sidebar lists pages under system headings such as Flight and On Foot. A page names its heading
with `group` on its descriptor. Known headings come first in a fixed order, and pages without one
follow. If more than one module is loaded, each gets a row and only the active module's pages show.

The sidebar also splits long pages. If a page has more rows than fit, at least two group headers,
no composition and no live components, its headers are listed under it and the settings area shows
one section at a time. Moving through the controls crosses between sections, and clicking a section
jumps to its first control. No shipped page qualifies yet, so `menu-sections` tests this with a
test-only page.

The source-built component foundation renders measured semantic rows for toggles, sliders, choices,
actions, and bindings, plus selected-setting help and a compact input-aware command footer. Thirteen
ordinary rows fit at 1080p.

Pointer button transitions and coalesced held movement are forwarded to
`handlePointerDown(stageX, stageY)`, `handlePointerMove(stageX, stageY)`, and
`handlePointerUp(stageX, stageY)`. The movie resolves those coordinates against the actual
rendered sprites and emits semantic commands; native code keeps no parallel pixel-hitbox table.
Sliders retain their semantic module/page/control identity across model redraws, so dragging keeps
working after each draft write replaces the rendered row. Clicking the track still positions the
value, and held movement continuously updates it until release. Starfield does not deliver a Flash
`MOUSE_WHEEL` event to this menu:
it publishes direction-specific mouse `ButtonEvent`s, `0x800/+120` for up and `0x900/-120` for
down. Native code
deduplicates that delivery, samples the cursor, and forwards
`handlePointerWheel(stageX, stageY, direction)`. The movie still owns region selection: the wheel
changes page over the sidebar and scrolls over the settings, without changing values.

`src/AbsoluteWalkGauge.as` is a second, separate movie: the walk speed gauge, an overlay using the
same menu flags as the game's `HUDMessagesMenu`. It reads the speed and opacity from native getters
in its own `ENTER_FRAME`, because SFSE tasks run on a pool of threads and calling into a movie from
one could race with it. Once first shown it stays open at zero opacity, so a speed change doesn't have
to load a movie each time. The same build script compiles both movies. Its colors are sampled from the
vanilla HUD.

The project must not commit a manually edited opaque SWF as its only source of truth.
