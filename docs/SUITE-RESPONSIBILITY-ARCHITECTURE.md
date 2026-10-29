# Absolute suite responsibility architecture

> **Status:** Direction for the suite-wide refactor begun 2026-09-18. This
> document defines ownership and compatibility priorities. It does not claim
> that device services or injection have already moved.

See the [transition plan](SUITE-REFACTOR-TRANSITION-PLAN.md) for dependency tracks,
cutover gates and rollback, and the [authority risk register](SUITE-REFACTOR-AUTHORITY-RISKS.md)
for source-confirmed migration hazards.

**Execution update, 2026-09-18:** the current plan is native backend first:
build AC's final services, then adapt subscribers individually. Earlier stages
and adapter priorities below are historical decomposition guidance, not a
requirement to ship transitional facades or keep HOTAS as AC's provider. AC owns
native route/profile persistence; subscribers retain domain configuration.

## Priorities

1. Preserve headless operation, provider persistence, declared API compatibility
   and safe coexistence with other menu systems. Redesign saved device/binding
   and profile records natively; legacy profiles must not require a permanent
   runtime translator. Rebinding may be necessary during migration.
2. Make responsibility boundaries explicit before freezing new menu, device,
   routing, event, or injection contracts.
3. Make ordinary subscriber menus inexpensive to author and test.
4. Add new generic infrastructure to the core without performing a big-bang
   migration of proven gameplay hooks.

## Target ownership

| Layer | Owns | Must not own |
|---|---|---|
| Absolute Control runtime core | Service discovery; low-rate lifecycle/context events; normalized device/channel registry; capture arbitration; canonical binding identity; semantic action registry; routing, transforms, layers, and writer arbitration | Module configuration and gameplay policy; Scaleform-dependent execution |
| Device adapters | DirectInput, HID/Steam Input, tracker acquisition, timestamped normalized samples, and device metadata | Menus, binding policy, semantic routes, or game injection |
| Game-action injection backends | Version gates, researched offsets/hooks, semantic action writes, stale-source neutralization, and action availability | Devices, user profiles, module persistence, or presentation |
| Absolute Control menu | PauseMenu coexistence; engine menu lifecycle; rendering; standard widgets; advanced host-owned instruments; focus/navigation; Apply/Discard/Stay; binding-recorder UX | Continuous device polling, route execution, arbitration, raw game writes, or provider persistence |
| Gameplay modules | Domain configuration, validation, persistence, policy, actions, telemetry, workflows, and declarative menu contributions | Generic device discovery, PauseMenu hooks, shared SWF code, or raw offsets owned by a core backend |
| SDK and integration tooling | One versioned package; ABI-safe helpers; code generation; scaffolding; mocks; contract tests; capability/fallback validation | Runtime policy or module-specific semantics |
| Compatibility adapters | Existing HOTAS Input Bus, mouse-steering, camera-ownership, and Workbench contracts during migration | New generic feature development |

## Current inversions to unwind

- AbsoluteHOTAS owns the generic DirectInput inventory, snapshots, and capture
  service. Absolute Zero, Power, and Head Tracking therefore load a gameplay
  module to obtain shared device services.
- Generic device data, HOTAS profile identity, and pilot context share one Input
  Bus ABI.
- Flight and camera writer ownership is negotiated through pair-specific APIs.
  This is safe for the current pairs but does not scale to FPS, rover, and
  arbitrary semantic game actions.
- Every subscriber repeats host discovery, ABI validation, registration,
  transaction bookkeeping, typed conversion, and binding glue.
- Vendored ABI headers intentionally support different prefixes, but their SDK
  provenance and update policy are not mechanically uniform.

## Migration rules

1. New generic infrastructure belongs in the runtime core.
2. Existing specialized writers stay with their current owners until a semantic
   action path has equivalent runtime evidence, arbitration, neutralization, and
   fallback behavior.
3. Stable saved bindings target semantic device channels and game actions, never
   offsets or a particular injection backend.
4. High-frequency device samples use a bounded data plane; lifecycle, context,
   and availability changes use the event plane; configuration uses the
   transactional command plane.
5. The menu observes and configures runtime services. Opening, closing, or
   failing to load the SWF cannot stop routing or injection.
6. Provider configuration, validation, persistence, and gameplay behavior remain
   provider-owned.
7. Experimental advanced UI does not burden the standard toggle/slider/binding
   subscriber path.

## Menu-host compatibility contract

The player-facing product identity is **Absolute Control**. The intended Pause
Menu label is **ABSOLUTE CONTROL**, distinct from OSF UI's **MOD SETTINGS**.
Existing engine menu names, ABI identifiers, and action tokens remain compatibility
identities until individually migrated and runtime-qualified.

PauseMenu contribution is an optional adapter:

- preserve foreign rows and order;
- add exactly one owned row per completed list generation;
- identify ownership with a stable owner marker plus action token, never label;
- dispatch only when the selected row has the matching owned identity;
- fail closed on malformed lists, duplicate ownership, numeric collision, a
  patched seam, or uncertain hook chaining;
- never make provider registration or configured direct activation depend on
  entry injection; and
- allow only one input-owning presentation at a time.

The current OSF UI and Absolute Control implementations use different hook
seams. The remaining risk is Absolute Control's late Scaleform list rebuild and
one-shot reinsertion behavior, not a present action-number collision. A native
post-build extension seam requires separate research and two-host runtime
qualification.

## First implementation slices

### S1 — standard subscriber kit

- Ship ABI-neutral value codecs, host validation/capability helpers,
  module/page registration rollback, and generic draft transaction mechanics.
- Compile and execute the helper as an installed-SDK consumer fixture.
- Keep device capture, binding syntax, and domain validation provider-owned.

### S2 — common subscriber dogfood

- Migrate Absolute Zero first because its ordinary controls and existing tests
  are representative.
- Generate its immutable descriptor tables from the menu definition.
- Remove duplicated discovery, value conversion, and transaction machinery only
  after behavioral parity tests pass.

### S3 — core service contracts

- Separate normalized device/capture services from HOTAS profile and game-context
  services.
- Define compatibility adapters before moving DirectInput ownership.
- Design native device, binding and profile records before profile integration.
  Optional legacy import is a one-time migration, independent of API facades;
  unknown assignments require explicit resolution or rebinding.
- Define semantic action shapes and arbitration without moving production hooks.

### S4 — research-backed injection

- Map remaining FPS and rover actions in a non-packageable research backend.
- Prove action lifetime, context, neutralization, and simultaneous writes.
- Promote only verified actions behind capability discovery.

### S5 — composition authority

- Make enhanced composition authoritative over control placement so a control is
  rendered once, with the flat page retained only as compatibility fallback.
- Add responsive grid placement for standard controls and host-owned advanced
  instruments after the routing/editor vocabulary is settled.

## Non-goals for the first checkpoint

- No game offsets or injection hooks move.
- No public ABI version changes.
- No existing module loses its standalone configuration route.
- No live/composition experimental contract is frozen.
- No dependency on OSF UI is introduced.
