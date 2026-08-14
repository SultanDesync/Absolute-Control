# Technical debt register

> **Status:** Closing audit of the current implementation. “Resolved” means addressed in source
> and automated checks; it does not imply new Starfield runtime evidence.

## Resolved architecture findings

- **Artifact identity and stale deploy selection:** canonical and ResearchDev targets now have
  distinct manifests, hashes, roles, destinations, and packageability. Deploy/package consumers
  fail closed on the retired `AbsoluteControlPanelResearch.dll` and mixed hosts.
- **Host readiness:** the query table remains discoverable during initialization;
  `registerModule`, `registerPage`, and `requestRefresh` return `NotReady` until exact runtime
  validation, lifecycle-hook installation, and menu-factory retention succeed. Runtime rejection
  is terminal and returns `Rejected` for those mutations.
- **Provider lifetime:** callback leases count calls outside registry/provider locks. A successful
  draft write holds a transaction token; `unregisterModule` returns retryable `Rejected` while any
  callback or dirty transaction is active and succeeds only after the provider can be retired.
- **Public API drift:** `AbsoluteControlPanelAPI.h` is the single type/constant/callback authority.
  `SlopAPI.h` aliases those types and exposes only the exact legacy table prefix and export.
- **Research leakage:** product and research source sets are explicit and tested at source and
  binary level. DirectInput, SendInput/mailbox automation, the synthetic provider, and live
  components are absent from the canonical target.
- **Registry/model capacities:** one host authority admits at most 32 modules, 32 pages, 128
  controls per page, and 512 controls total; the session uses the same constants and capacity tests
  exercise full graphs.
- **Refresh/stale commands:** provider refreshes advance a dedicated revision consumed by the open
  bridge; model publications carry a per-session generation; current ActionScript echoes that
  generation and stale commands are rejected without provider mutation. Slider motion keeps only
  the latest pending value and emits at most one write per movie frame.
- **Native and ActionScript monoliths:** bootstrap, compatibility, UI integration, Scaleform,
  input, diagnostics, and research responsibilities now have explicit source ownership. The SWF is
  divided into orchestration, bridge, selection, pointer, widget, shell, layout/theme, and slider
  coordination modules, with source-boundary tests.
- **Bridge-root failure:** a loaded movie without the required bridge root records a runtime fault
  and queues an explicit canonical-host hide; it no longer relies on a ResearchDev watchdog.
- **Control-ID scopes:** the raw ABI's page-local uniqueness is intentional because each page may
  carry distinct context/callbacks. The generator's module-wide uniqueness is also intentional for
  its one shared `ProviderCallbacks` set and module-wide ID parser. Both rules are documented.
- **Interface provenance:** dist metadata and artifact manifests enumerate and hash all ten current
  ActionScript sources; helper changes can no longer pass a root-file-only check.
- **Detached/infinite workers and synchronous evidence writes:** polling uses cooperative,
  restartable services with callback invalidation; process-lived ownership is explicit. Evidence
  file I/O is performed by a bounded asynchronous single-writer sink with drop/error statistics.
- **Process drift:** `tools/process/validate-current.cmd` is the maintained automated product
  validator. Disposable builder v1 and its magenta sentinel are archived and require explicit
  opt-in; they cannot be cited as current product evidence.
- **Abnormal dirty-session teardown:** the `Session` destructor now performs one idempotent
  provider rollback while its transaction lease still blocks unregistration, then releases state.

## Remaining defects and current risks

| Priority | Debt | Current consequence / exit condition |
|---|---|---|
| P1 | Runtime/controller support is not connected. `ControlPanelMenu` rejects gamepad events even though router/prompt scaffolding exists. | Do not claim controller navigation. Route and suppress controller input, reseed held state, then complete controller-only and device-transition regressions with no stuck/ghost axis. |
| P1 | Mouse/controller/HOTAS binding capture is absent; only keyboard plus Ctrl/Alt/Shift is implemented. | Capability flags can describe devices the current adapter cannot capture. Either reject unsupported registrations/capture requests or implement bounded device-specific capture, cancellation, timeout, conflict, and reseed behavior. |
| P2 | SDK generator capacity rules are not sourced mechanically from the C++ host and do not reject a definition exceeding the 512-control total. | A definition can generate successfully and then be partly rejected page-by-page at runtime. Add a shared machine-readable limits source or cross-check and a total-control validation test. |
| P2 | Current full model publication rereads and serializes all registered controls after commands/refresh. | Bounded but potentially expensive at the 512-control limit. Measure game-thread cost, then add dirty-page/delta publication only if evidence warrants it. |
| P2 | `MenuApiHost.cpp`, `ScaleformMenuBridge.cpp`, `PauseMenuIntegration.cpp`, and ResearchDev support remain sizeable integration units. | Their responsibilities are now coherent, so splitting is not automatic. Add behavior seams if runtime-version work or tests repeatedly require edits across unrelated regions. ResearchSupport remains intentionally quarantined from release. |
| P2 | Legacy SLOP names remain in namespace/event/action IDs and a compatibility export. | They are no longer ABI authority or package identity. Define a migration window before removing the adapter; rename internal research terminology opportunistically without breaking evidence interpretation. |
| P2 | `CancelCallback` cannot report rollback failure, while dirty close assumes cancellation succeeded once a lease is acquired. | ABI evolution must either accept this v1 limitation or add result-bearing rollback/teardown semantics in a new version. |
| P3 | Async evidence drops and I/O failures are observable only through statistics, not automatically summarized in the log. | Capture statistics at controlled shutdown/test boundaries; avoid recursive logging from the sink. |

## Required manual evidence still open

The current automated process passed; it did **not** run Starfield or judge UX. Before a release
claim, record at least:

- PauseMenu and F2 first-open, close/reopen, 25-cycle lifecycle, bridge-root failure, and crash/dump
  monitoring on the canonical DLL/SWF pair;
- two-direction wheel behavior in workspace/sidebar/overflowing tabs, keyboard-only navigation,
  slider drag/coalescing, Apply/Cancel/dirty close, and persisted provider read-back;
- controller-only navigation, input-device transitions, focus loss/alt-tab, and absence of leaked,
  ghost, or stuck input after controller support exists;
- mouse and controller/HOTAS binding capture only after implementation, including cancel, clear,
  timeout, held input, conflicts, and reseed;
- missing/corrupt/old/new SWF, incompatible runtime/ABI, provider callback failure, disappearing
  provider, save failure, reload/read-back failure, and abnormal menu teardown;
- 720p, 1080p UI-scale variants, 1440p, 4K, 21:9/32:9, long/pseudo-localized strings, complete
  glyph coverage, and accessibility/readability review; and
- isolated and representative heavy/UI-mod profiles using artifact hashes from the canonical
  manifest.

Live/compound components, choice labels/dropdowns, text/numeric editors, localization, public SDK
packaging, and Nexus release readiness remain unsolved product work.
