# OSF UI provenance and compatibility

> **Status:** Source and architecture audit against OSF UI commit
> `dde2b48d7a9fdfa0ada4629d9ce26f7c9f631e41` (2026-09-01). Co-install behavior is
> being hardened under the 2026-09-04 native-host decision; an in-game two-host
> acceptance run on the revised entry is still required.

[OSF UI](https://github.com/ozooma10/osf-ui) is prior work for a shared Starfield mod-settings
experience. Its working implementation predates Absolute Control: OSF UI's off-screen web renderer
and bridge landed in June 2026, its schema-driven multi-mod settings work followed that month, and
its tagged releases began in July. Absolute Control's repository began on 2026-08-12.

## Provenance conclusion

Before Absolute Control began, OSF UI already used a similar mod-settings journey and information
architecture:

- a new PauseMenu entry opening one shared mod-settings host;
- a left-hand installed-mod rail and right-hand detail pane;
- per-mod pages exposed as top tabs;
- typed setting rows, key capture, diagnostics/system health, and controller navigation; and
- a dark, fine-lined Starfield-adjacent palette led by cyan with amber status accents.

The native implementation is not byte-for-byte OSF UI code. OSF UI renders out of process with
WebView2 and composites through D3D12; it discovers JSON schemas and owns setting persistence.
Absolute Control registers a native Scaleform menu; SFSE providers supply bounded descriptors and
retain validation, transaction, and persistence ownership. A file/hash and identifier scan found
no identical product source files. That narrow result is useful for code provenance, but it does
not establish an independently created UX and must not be used to dismiss the design overlap.

These similarities and acknowledged influences do not establish that the architectures are
interchangeable or that an external dependency would reduce support risk. The product decision
on 2026-09-04 retains Absolute Control and its specialized native menus. The project position is:

1. credit OSF UI and its contributors prominently;
2. do not market the current visual/interaction design as an original clean-room design;
3. use the distinct **CONTROL PANEL** entry and preserve each project's identity; and
4. prove coexistence through runtime tests while retaining independent native rendering and
   provider-owned configuration. Upstream migration or wholesale redesign is not a release prerequisite.

## Co-install contract

The integration is designed for both hosts to be installed. This contract concerns coexistence;
it does not provide API interchangeability:

- OSF UI retains its `MOD SETTINGS` PauseMenu action and `OSFUI_FocusMenu` capture menu.
- Absolute Control uses its distinct `CONTROL PANEL` action and
  `AbsoluteControlPanelMenu` native menu.
- Absolute Control's late, additive PauseMenu composition copies every existing entry before
  appending its own, so the OSF UI row is preserved regardless of OSF UI's earlier insertion.
- The action IDs are disjoint: OSF UI currently uses `100`; Absolute Control uses
  `0x534C4F50`.
- A namespaced ownership marker distinguishes Absolute Control's row from a foreign row with the
  same numeric action. Such collisions reject insertion; activation requires our selected row.
- Failure to install the optional PauseMenu lifecycle hook no longer rejects the native host.
  No foreign hook is overwritten or blindly chained. Provider opens and configured recovery
  hotkeys remain available after the core menu compatibility and factory checks pass.
- A recovery-hotkey or provider request to open Absolute Control is rejected while
  `OSFUI_FocusMenu` is active. This prevents two input owners from being opened on top of each
  other. Normal PauseMenu selection remains available after OSF UI closes.
- Absolute Control does not load OSF UI code, link its SDK, write its settings files, register its
  exports, or claim its view/settings protocol. Missing or future OSF UI versions therefore cannot
  make Absolute Control a hard dependency or corrupt OSF UI state.

The active-menu guard uses OSF UI's current engine menu name because the public OSF UI Views service
does not expose a general "presentation active" query. If that internal name changes, the guard
degrades to the hosts' ordinary independent behavior; it does not call an unknown ABI.

## What interoperability does not mean

An OSF UI JSON settings schema is not automatically an Absolute Control provider, and an Absolute
Control page descriptor is not automatically an OSF UI view. Their ownership models differ:
OSF UI persists schema values, while Absolute Control delegates draft, Apply, Cancel, validation,
and persistence to the provider. Silently translating either direction would change transaction
semantics and could create two authorities for one setting.

A mod can support both hosts safely by keeping one canonical configuration backend and treating
each UI integration as an optional adapter. Each adapter must observe external changes, avoid
writing during initial synchronization, and leave gameplay functional when its host is absent.

## Compatibility roadmap

Retain the native host, its specialized instruments and its provider contracts. Prioritize a
distinct entry, preservation of other entries, independent activation, and input ownership. The
[PauseMenu compatibility design](PAUSE-MENU-COMPATIBILITY.md) distinguishes implemented hardening
from a future native action-list integration that still needs research and runtime qualification.

OSF UI remains an independent peer. An optional adapter could be reconsidered for a demonstrated
user need, but neither migration nor a new upstream dependency is planned. “Compatible” means
co-installable with guarded input ownership after the required acceptance run.

## Required two-host acceptance run

Before calling the combination runtime-verified:

1. Install OSF UI and Absolute Control together on Starfield 1.16.244.
2. Open PauseMenu and confirm both `MOD SETTINGS` and `CONTROL PANEL` appear exactly once.
3. Open and close OSF UI, then open and close Absolute Control; verify input, cursor, audio, pause,
   ultrawide geometry, and return behavior each time.
4. While OSF UI is open, trigger Absolute Control's opt-in recovery hotkey and a provider open
   request; require both to be rejected without changing the active UI.
5. Repeat in the opposite order. OSF UI should treat Absolute Control's modal menu as a game-menu
   gate and remain closed.
6. Repeat the cycle at least 25 times and inspect both logs plus the crash/dump delta.

Until that run passes, the accurate label is **designed for coexistence and build-tested**, not
runtime-certified.
