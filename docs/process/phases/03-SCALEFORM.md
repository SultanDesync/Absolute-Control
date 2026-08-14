# Builder phase 03: descriptor-driven native/Scaleform renderer (archived v1)

> Historical only. Its magenta sentinel is not a current product criterion.

This phase begins only after phases 01 and 02 pass. Consume the tested `MenuSession`; do not
duplicate provider validation or transactions in NativeMenuProbe or ActionScript.

## Native bridge

- Serialize `MenuSession::Model` to the exact bounded `applyModel(model)` object in
  [Bridge Protocol v1](../../BRIDGE-PROTOCOL-V1.md).
- Map the exact flat `dispatch(...)` function to `MenuSession::Command`, then publish a fresh
  model after every accepted or rejected command.
- Keep a native Escape/close route operational even if ActionScript throws.
- Remove runtime knowledge of ResearchModule IDs and fixed command names. The synthetic provider
  may remain registered only as an ordinary fixture.

## ActionScript renderer

Build the visible page/control list from `pages[].controls` in descriptor order. Support Toggle,
IntegerSlider, FloatSlider, and Action. Choice and ButtonBinding may be marked unsupported for this
phase but cannot crash. Provide selected-row highlight, scrolling, advanced/restart markers,
generic mouse and W/S/E/A/D input, Apply, Cancel, Close, and the magenta sentinel. Retain the
vector alphabet until a font path is proven.

The runtime native and AS sources must contain none of `toggleFeature`, `incrementLevel`,
`decrementLevel`, `beginBindingCapture`, `responseLevel`, or synthetic control IDs.

## Mechanical proof

Build the SWF with the supplied `build-interface.cmd` wrapper, which uses the maintainer-pinned
tool root without linking it into the disposable worktree, then run the host wrapper. Add source/contract
tests proving `applyModel`, flat dispatch validation, and absence of fixed commands. Update runner
event expectations in the same change. Write `phase-03-result.json` at the run root. Do not commit.
