# Scaleform boundary

This directory reserves the source and compiled-asset boundary for the native menu.

`src/AbsoluteControlPanelMenu.as` defines the intended root object and the smallest C++ bridge.
It is design input, not yet a reproducible SWF. `AbsoluteControlPanelMenu.swf` is intentionally
absent and ignored until research Gate R1 records:

1. the compiler and exact version;
2. the command needed to build from source;
3. any Bethesda font/component dependencies and their redistribution status;
4. the root path observed by CommonLibSF; and
5. a clean-room deploy and load result.

The project must not commit a manually edited opaque SWF as its only source of truth.
