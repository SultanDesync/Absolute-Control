# Absolute suite architecture review — 4 September 2026

> **Subsequent product decision, 2026-09-04:** The author chose to retain Absolute Control and its
> specialized native menus, use an **CONTROL PANEL** PauseMenu entry, and prioritize compatible
> injection over an OSF migration trial. The original assessment below is retained as a review
> record; its proposed next-step OSF trial is superseded by
> [the native entry compatibility work](PAUSE-MENU-COMPATIBILITY.md).

**Recommendation: keep the four independently installable gameplay mods, consolidate shared implementation and frontend support, and retain Absolute Control as a suite-focused native editor for the next release. Do not commit to an immediate OSF UI migration or expand Absolute Control into a general MCM platform. Make an OSF UI custom-view trial the next architectural experiment.**

This recommendation assumes the primary product is the flight-control suite. Building a general configuration platform for unrelated Starfield mods would be a separate product commitment with its own justification and maintenance budget.

The review covers current local source and build definitions for AbsoluteHOTAS, Absolute Power, Absolute Head Tracking, AbsoluteZero, Absolute Control, and legacy Absolute Workbench; suite packaging and SDK contracts; and OSF UI's published release and current upstream API. Several local repositories contain pre-existing uncommitted work. Findings describe the inspected working trees, with released upstream features distinguished from development features. This was a source and architecture review: no new build, game session, performance measurement, or compatibility certification was performed. Only this review document was added.

**The gameplay boundaries mostly match real user needs.**

| Component | Independent purpose | Decision |
|---|---|---|
| AbsoluteHOTAS | Device polling, flight axes, throttle, ship actions, profiles and macros | Keep standalone; remain the current owner of shared device polling. |
| Absolute Head Tracking | OpenTrack cockpit camera, manual trim and bounded off-axis aim | Keep standalone. Mouse and gamepad users can want this without HOTAS. |
| Absolute Power | Ship-power allocation, presets, keyboard activation and persistence | Keep standalone. Its policy and native backend are useful without either flight-input mod. |
| AbsoluteZero | Mouse alignment and locked-reticule steering | Keep standalone distribution, but consolidate shared flight-writer implementation and tests with HOTAS. |
| Absolute Control | Configuration, transactions, navigation and specialized live instruments | Keep one optional editor host; narrow its roadmap to the suite. |
| Absolute Workbench | Earlier shared ImGui editor | Retire from the supported current line after remaining useful workflows have a supported replacement. Preserve old releases for rollback. |

These are functional boundaries, not arbitrary DLL splits. A single gameplay DLL would couple updates and runtime failures across unrelated features, make optional installation less meaningful, and still need internal boundaries for camera, input and power ownership. Fewer DLLs would not automatically remove the difficult ownership rules.

The shared menu is optional for gameplay but is required for the current HOTAS in-game editor. That is an acceptable, clearly describable dependency. Standalone must mean a mod loads, reads its configuration and executes its supported behavior without the menu; it need not mean every mod carries a second renderer.

Power already demonstrates the correct separation: its runtime owns configuration and activation, while its frontend API accepts generation-stamped drafts and returns structured save outcomes. Zero's subscriber likewise owns live preview, save and rollback. Preserve those responsibilities under any future UI. See [Power architecture](../../Absolute%20Power/docs/ARCHITECTURE.md), [Power frontend API](../../Absolute%20Power/include/AbsolutePowerFrontendAPI.h), and [Zero transactions](../../AbsoluteZero-Ship-Control/src/ControlPanelSubscriber.cpp).

**The costly boundary is shared input ownership, especially HOTAS and Zero.**

Zero installs its own writer when standalone. When HOTAS is present, it negotiates the HOTAS mouse-steering API and uses bounded accumulator operations instead of installing a second hook. HOTAS then yields pitch and yaw; paired locked-reticule/VirtualJoy behavior is restricted. This is already a closely coordinated flight-control subsystem despite separate products. See [Zero startup](../../AbsoluteZero-Ship-Control/src/Main.cpp), [compatibility client](../../AbsoluteZero-Ship-Control/src/HotasMouseCompatibility.cpp), and [documented paired behavior](../../AbsoluteZero-Ship-Control/README.md).

Consolidate the common writer validation, accumulator access and authority policy into one maintained source package, with standalone and paired tests. Keep feature policy inside each mod. A shared static library or source package removes code drift, but does not enforce one process-wide hook owner by itself: retain explicit owner negotiation, or designate one runtime owner if the architecture later changes. Do not solve this by allowing two linked copies to install the same hook.

The Input Bus is another useful shared service, not a reason to merge Power or Head Tracking into HOTAS. It publishes copied device/context data and offers one physical capture session; consumers retain action and binding semantics. The bus deliberately does not arbitrate camera or flight writers. See [Input Bus contract](../../AbsoluteHOTAS/docs/ABSOLUTE-INPUT-BUS-V1.md).

Keep those service distinctions clear. Do not create a mandatory new “Absolute Core” DLL merely to reduce repository count. Extract an input runtime only when supporting joystick bindings without the HOTAS flight stack becomes a concrete product requirement. Such an extraction must also account for the bus's HOTAS-derived profiles, calibration and validated game context; it is more than moving DirectInput polling into another project.

**Consolidate maintenance and support paths before consolidating products.**

1. **Finish frontend retirement.** HOTAS already excludes its ImGui renderer and embedded tracker from the shipping target. Head Tracking still compiles ImGui/MinHook/D3D12/DXGI and retains standalone/Workbench selection. Power and Zero retain legacy host integration. Inventory any remaining fallback-only functionality, close that gap, then remove obsolete support branches from the current release line. Keep INI configuration, intrinsic hotkeys and older release archives. See [HOTAS build](../../AbsoluteHOTAS/xmake.lua), [Head Tracking build](../../Absolute%20Head%20Tracking/xmake.lua), and [host selection](../../Absolute%20Head%20Tracking/src/WorkbenchHost.cpp).
2. **Package common source and contracts once.** Use versioned, pinned SDK/helper packages for discovery, size/capability checks and reusable transaction plumbing. Preserve deliberate older ABI consumers: different header hashes in the [integration registry](../sdk/integration-registry.json) are not automatically defects. Compatibility tests should establish which versions work; replacing every header on every release defeats independent modules.
3. **Coordinate releases through the existing manifest.** The [suite FOMOD and exact component manifest](SUITE-RELEASE-PACKAGING.md) already provide a unified install without a runtime merger. Extend that integration workflow. A monorepo is optional organizational work, not a prerequisite for shared code or a good player experience.
4. **Declare one current configuration route.** The separate Tauri configurator remains another schema/writer maintenance surface. Unless an out-of-game editor is an explicit supported product requirement, mark its supported HOTAS versions and stop treating it as an implicitly current editor. An active external editor would need the same configuration ownership and migration contract.
5. **Repair the status record.** `CURRENT-STATE.md` still says Head Tracking is incompatible with HOTAS because of the embedded tracker; the current HOTAS build excludes that tracker. The debt register says dirty navigation only errors and close silently cancels, while [MenuSession.cpp](../src/MenuSession.cpp) implements Apply/Discard/Stay decisions. These are documentation contradictions, not evidence that the implemented features are missing. Reconcile the status pages to a named release and keep runtime evidence separately dated.

**Absolute Control's implementation is useful, but its platform scope is larger than the suite needs.**

The existing host has valuable boundaries: copied descriptors, guarded callbacks, provider-owned persistence, capability negotiation, isolated runtime mappings, and a native Scaleform renderer. Keeping gameplay out of the menu means a renderer replacement remains possible. Those choices were sound.

However, maintaining configuration, live-component and semantic-composition APIs, a definition language and generator, a component catalogue, external SDK support, and hundreds-of-mod scalability is a substantial second product. The native `src`/`include` tree contains approximately 12,500 nonblank lines including research code, plus approximately 5,900 nonblank ActionScript lines. HOTAS's main subscriber alone is approximately 3,500 nonblank lines and Power's approximately 2,200, before related helpers. These counts include comments and are maintenance-surface indicators, not complexity or performance scores.

There is also concrete suite-specific knowledge inside the nominally generic renderer: [LiveSurfaceInputRouter](../interface/src/acp/ui/LiveSurfaceInputRouter.as) routes directly to `absolute.hotas/hotas-throttle`, and [MenuShellRenderer](../interface/src/acp/ui/MenuShellRenderer.as) recognizes HOTAS's throttle channel. This is reasonable in a suite editor, but weakens the premise that the host is already a clean general-purpose SDK. Choose one intention. For this suite, retain the useful component system, make new behavior capability/data driven where practical, and pause unrelated third-party framework expansion. Existing compatible integrations should keep working; there is no benefit in breaking them or deleting tested capacity controls.

**OSF UI is the strongest external candidate, but migration is a custom frontend project.**

Upstream activity was verified through the GitHub connector: the latest commit inspected is `dde2b48d7a9fdfa0ada4629d9ce26f7c9f631e41`, dated 1 September 2026. The latest published release returned by GitHub is **1.5.0, published 30 July 2026**. This establishes recent maintenance, not long-term support capacity or proven compatibility with this suite. [Current development checkpoint](https://github.com/ozooma10/osf-ui/commit/dde2b48d7a9fdfa0ada4629d9ce26f7c9f631e41), [published release](https://github.com/ozooma10/osf-ui/releases/tag/v1.5.0).

| Option | Fit and cost | Decision |
|---|---|---|
| Keep expanding the bespoke general MCM | Preserves current work but commits the project to broader SDK, renderer and ecosystem maintenance | Reject as the default suite strategy. |
| Keep Absolute Control focused on Absolute | Preserves specialized controls and native rendering with the smallest immediate disruption | Choose for the next release. |
| Move to ordinary OSF settings schemas | Convenient scalar settings, but changes persistence ownership and does not supply the suite's specialized editors | Reject as a direct conversion. |
| Build an Absolute custom view on OSF UI | Can retain native backends while transferring general rendering/input-host maintenance upstream | Trial next; migrate only after the trial passes. |
| Merge all gameplay mods | Reduces packaging boundaries while widening dependency and regression scope | Reject. |

OSF UI provides HTML views, native messaging, shared settings, localization and diagnostics through an out-of-process WebView2 helper. Its runtime requires WebView2 in addition to SFSE and Address Library. Version 1.5.0 documents native request/reply support and removal of conflicting swap-chain Present hooks; it still uses graphics composition, so it is not equivalent to Absolute Control's native Scaleform rendering path. Overlay and frame-generation compatibility needs measurement rather than assuming either host is universally safer. [Upstream README](https://github.com/ozooma10/osf-ui/blob/main/README.md), [1.5.0 release notes](https://github.com/ozooma10/osf-ui/releases/tag/v1.5.0).

A custom view can route intents back to the existing providers and render returned state. Do not duplicate the INIs into OSF's host-owned settings store, introduce bidirectional synchronization between two authoritative stores, or put gameplay activation hotkeys under the menu host. Power's imports/custom overlays and stale-generation checks must survive unchanged in meaning. The flight plots, power grid, profile/macro workflows, head-pose instruments and physical capture UX would still be Absolute-maintained components. Upstream owns the platform; it does not take ownership of those application workflows. [Released native API](https://github.com/ozooma10/osf-ui/blob/v1.5.0/docs/native-plugin-api.md), [released custom-view guide](https://github.com/ozooma10/osf-ui/blob/v1.5.0/docs/authoring-views.md).

The release/development distinction is material. The current [Views service header](https://github.com/ozooma10/osf-ui/blob/dde2b48d7a9fdfa0ada4629d9ce26f7c9f631e41/sdk/OSFUI_Views.h) adds per-view shown/frame/hidden callbacks, relative-pointer delivery and open preflight. The [1.5.0 native header](https://github.com/ozooma10/osf-ui/blob/v1.5.0/sdk/OSFUI_API.h) lacks those service methods. The released custom-view guide also says the overlay toggle always closes natively even when a view handles Back. Therefore normal HTML Cancel handling alone is insufficient evidence that live previews and physical capture are unwound on every close or renderer failure. This does not prove migration is impossible on 1.5.0; it makes lifecycle cleanup a trial requirement and the upstream release baseline an explicit decision.

The [existing OSF compatibility note](OSF-UI-COMPATIBILITY.md) already proposes an extension/adapter direction. It also records visual-design overlap and pending two-host runtime acceptance. Those are relevant collaboration and presentation considerations, but neither proves technical interchangeability nor makes an untested migration safe. No upstream contact was made during this review.

Other candidates do not currently displace OSF UI in this comparison. The [Starfield Community Patch MCM repository](https://github.com/Starfield-Community-Patch/starfield-mcm) has only two May 2024 commits and no releases in the API results inspected. [UIPlayNice](https://github.com/Ideki/UIPlayNiceExample) addresses coexistence of UI additions; the [Gameplay Options helper](https://github.com/ProfX66/Starfield-Gameplay-Options-for-UI) reads and tracks native settings. Neither supplies the complete custom configuration platform this suite would need.

**Proposed sequence and migration exit criteria.**

First, reconcile status documents and declare the supported frontend matrix. Then retire legacy editor paths after their remaining useful workflows are covered, consolidate shared flight-writer source/tests, and keep the existing standalone/FOMOD release arrangement. Preserve the current configuration formats throughout.

Next, build one isolated OSF custom-view trial using a Power preset transaction and a HOTAS axis/capture workflow. A toggle-only Zero page would demonstrate connectivity but would not test the expensive parts of migration. Use thin adapters to existing backend operations; extract only the presentation-independent transaction/session code needed for the trial. Avoid building a general translator for all three Absolute UI APIs.

Require evidence for:

- Power edit, Apply, Cancel, stale draft and save failure, preserving imports and custom overlays.
- Live axis telemetry, physical button/POV/axis capture, clear/cancel/timeout and device reconnection.
- Native close, Back, focus loss and renderer failure, with no retained capture or uncommitted preview left active.
- Keyboard, mouse and controller operation; ultrawide behavior; cold/warm open latency, UI/game frame impact and memory use compared with the current host on the same setup.
- Host absence and incompatible host versions, leaving gameplay and configuration usable.
- A supported, pinned upstream release containing the hooks required by the chosen implementation, plus a credible reduction in Absolute-owned platform maintenance without maintaining a permanent private OSF fork.

If these pass, migrate the specialized Absolute frontend to OSF UI, coordinate reusable platform improvements upstream, and retire Absolute Control's renderer/shell after remaining journeys reach parity. A temporary transition can support both, but only one should own an editing/capture session; dual-host support should have a retirement point.

If the trial requires extensive private lifecycle or rendering patches, keep the suite-focused native host. The current evidence justifies selective consolidation now and a bounded migration trial next; it does not justify sacrificing independent gameplay mods or replacing a functioning specialized editor on the assumption that an external framework will be cheaper.
