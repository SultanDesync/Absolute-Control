# Checkpoint 05: subscriber menu integration

Date: 2026-09-18 (local); runtime evidence uses UTC run identifiers.

The requested first module pass covers Zero, Power and Head Tracking's AC menu
integration. HOTAS remains deferred for its separate device/profile design review.
This checkpoint does not claim completion of B2-B4 or native gameplay migration.

## Changes

- All three subscribers vendor the canonical `AbsoluteControlSubscriberKit.h`
  and use its detailed module/page registration helper.
- Duplicate registrations cannot establish ownership. Partial page failures
  roll back the module created by that attempt. Rejected rollback and terminal
  registration errors prevent retries against ambiguous state; `NotReady` with
  successful rollback remains retryable.
- Head Tracking registers its complete four-page set before optional live
  presentation, and repeated lifecycle registration is idempotent. It no longer
  reports success with a missing Bindings page.
- Optional live-channel duplicates are failures rather than evidence of
  ownership. Zero discovers live presentation from its selected menu host.
- Zero retains its rollback snapshot when Discard cannot restore configuration,
  allowing a later retry. Its tests cover this failure path.
- Zero and Head Tracking discover only the current product and ResearchDev
  host names; their discovery decisions no longer accept `Duplicate` as success.
- AC emits page identity, active state, control count and generation only after
  the SWF acknowledges a model. The research runner can open the nine allow-listed
  pages through the public page-opening API without editing configuration.

## Validation

- Zero: Debug CTest, 3/3 passed, including registration and transaction tests;
  production DLL built successfully.
- Power and Head Tracking: production DLL builds passed.
- AC: all current-process automated gates passed, including 18 native tests,
  SDK helper/code-generation fixtures, catalogue, SWF provenance and packaging.
- After adding research-only page commands: ResearchDev rebuilt and the two
  release/research source and binary boundary checks passed again.
- Smoke evidence verifier: 9 fixture cases passed, including negative cases for
  stale runs, missing/inactive pages, rejected/unacknowledged publications,
  mismatched generations, duplicate active-page records and empty controls.
  Existing runner timing contracts also passed.
- The three vendored helper copies were hash-checked against AC's canonical file.

Runtime run `subscriber-menu-smoke-20260919-004443` passed in the isolated test
profile, with AC ResearchDev, Zero, Power, Head Tracking and OSF UI V; HOTAS was
not part of the enabled cohort. Each page became active and its controls reached
an acknowledged SWF model:

| Module | Page | Controls | Generation |
|---|---|---:|---:|
| Zero | Mouse Alignment | 8 | 2 |
| Zero | Bindings | 3 | 3 |
| Power | Presets | 21 | 4 |
| Power | Automation / Cheats | 3 | 5 |
| Power | Diagnostics | 19 | 6 |
| Head Tracking | Axes | 20 | 7 |
| Head Tracking | General | 5 | 8 |
| Head Tracking | Aim Restriction | 2 | 9 |
| Head Tracking | Bindings | 29 | 10 |

Local evidence resides under
`artifacts/research-runs/subscriber-menu-smoke-20260919-004443/`, including
`subscriber-menu-smoke.json`, exported events and nine page screenshots. The
runner requested normal shutdown and observed process exit. Existing deployment
files were backed up before replacement; the updated test cohort remains in the
isolated profile. No profile enablement changes or menu setting edits were made.

The first run reached the menu but failed an overly strict verifier assumption:
AC intentionally publishes controls only for the active page. The corrected
runner visits each page and checks its own acknowledged generation, rather than
counting registration or combining unacknowledged data.

## Remaining work

Native capture, calibration, bindings/profiles, context/action routing and output
services remain incomplete. Existing subscriber input clients, legacy binding
formats, fallback frontends and specialized gameplay backends have not yet been
removed. These pages loading does not prove that controller capture, Apply and
Discard in game, persistence, live tracking, or gameplay outputs work through the
new AC runtime. Power's Automation page remains a preview. Continue implementing
native AC services and move subscriber runtime ownership as each service becomes
functional; do not add an interim HOTAS compatibility provider.
