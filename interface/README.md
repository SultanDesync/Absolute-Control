# SLOP Scaleform boundary

This directory reserves the source and compiled-asset boundary for the native menu.

`src/AbsoluteControlPanelMenu.as` defines the root object and the smallest C++ bridge.
`dist/AbsoluteControlPanelMenu.swf` is built from source by
`tools/research/build-interface.ps1`; the adjacent build metadata records compiler, target,
PlayerGlobal commit, source hash, and output hash. Apache Flex embeds varying build metadata, so
the pinned inputs and recorded source hash are the reproducibility authority; byte-identical SWF
hashes are not currently claimed.

The research movie uses only Flash platform drawing, including its tiny diagnostic alphabet;
it contains no Bethesda
assets, fonts, or components. The plugin scans and records plausible root paths in game. A clean
deployed run has proven the visual movie, `_root` bridge object, `ready(1)` callback, active-menu
insertion, and watchdog close.

A 96x96 opaque `#FF00FF` block is a deliberate R1 smoke-test sentinel. The research harness
counts near-magenta pixels and records the count and bounding box, giving visual population a
cheap binary oracle that does not require general-purpose screenshot interpretation.

The project must not commit a manually edited opaque SWF as its only source of truth.
