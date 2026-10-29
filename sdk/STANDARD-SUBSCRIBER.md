# Standard subscriber path

> **Status:** First refactor checkpoint. This path targets ordinary settings
> pages made from Boolean toggles, numeric sliders, choices, and binding
> recorders. Advanced live instruments and semantic composition remain separate,
> capability-gated APIs.

An ordinary subscriber owns its configuration type, domain validation,
persistence, binding meaning, and gameplay behavior. Absolute Control owns menu
rendering, navigation, focus, dirty-state presentation, Apply/Cancel commands,
and capture presentation. The SDK should make the ABI plumbing between those
responsibilities routine.

## Authoring surface

The menu-definition JSON is the presentation-neutral source for module, page,
section, and control descriptors. `menu_codegen.py` validates it and produces
immutable descriptor arrays, stable control IDs, callback wiring, and older-host
fallback arrays.

The provider supplies only domain callbacks. It must not copy Scaleform code,
PauseMenu hooks, coordinates, colors, or host lifecycle logic into its adapter.

## Subscriber kit

`AbsoluteControlSubscriberKit.h` is a header-only implementation aid. It is not
a second runtime ABI and passes only `AbsoluteControlPanelAPI.h` records across
the DLL boundary. The first checkpoint provides:

- bounded text copying and typed Boolean/integer/float/string value codecs;
- size-gated host validation and capability queries;
- safe `isOpen` and input-capture queries;
- fail-closed module/page registration with descriptor preflight and a detailed
  rollback result for partial failure; and
- `DraftTransaction<T>` for the common live-apply, persist-on-Apply,
  restore-on-Cancel lifecycle.

Domain-specific parsing and cross-field validation remain in the subscriber:

```cpp
using Transaction = AbsoluteControlSubscriberKit::DraftTransaction<Config>;

Transaction transaction({
    &Configuration::Get,
    &Configuration::Apply,
    &Configuration::Save
});

auto result = transaction.Mutate([&](Config& proposed) {
    if (controlId == "sensitivity") {
        double value{};
        if (!AbsoluteControlSubscriberKit::ReadFloat(
                input, 0.05, 10.0, value)) return false;
        proposed.sensitivity = static_cast<float>(value);
        return true;
    }
    return false;
});
```

The helper intentionally does not own device polling, binding syntax, semantic
game actions, or input injection. Those boundaries move to AC's native services
as the contracts become executable; compatibility adapters are not required.
See the [native backend plan](../docs/SUITE-REFACTOR-TRANSITION-PLAN.md).

Zero, Power and Head Tracking now vendor this exact header in their `include`
directories and use `RegisterModulePagesDetailed` for menu registration. Update
the canonical header here first and synchronize those copies when changing the
helper. Their domain callbacks and specialized configuration transactions remain
module-owned. Adoption of this menu helper does not complete their native
binding, context or output migration.

### Registration limits in v1

`RegisterModulePagesDetailed` checks that every page names the supplied module
and that page IDs are nonempty and unique within the supplied batch before it
calls the host. A `Duplicate` result from either module or page registration is
always returned as a failure. In particular, a duplicate module causes no page
registration calls, because the v1 ABI cannot prove that the existing module is
owned by this provider instance.

If page registration fails after the module was accepted, the helper calls
`unregisterModule`. The returned `ModulePagesRegistrationResult` preserves both
the original registration result and the rollback result. Check
`PartialRegistrationMayRemain()` before assuming cleanup succeeded. The older
`RegisterModulePages` convenience wrapper remains available and returns only
the original registration result; use the detailed form whenever recovery or
diagnostics matter.

This v1 flow is best-effort, not atomic. An `Ok` module result is the only
ownership evidence available, and the ABI provides no registration token or
compare-and-unregister operation. Consequently rollback cannot be made safe
against a foreign caller concurrently changing the same IDs. Module IDs and
page IDs therefore must be unique across every provider loaded in the process,
not merely within one DLL or one registration call. Providers should treat IDs
as process-owned stable names (for example, reverse-DNS-prefixed names), avoid
concurrent registration for the same IDs, and treat rejected rollback as a
possibly live partial registration.

## Acceptance target

A representative standard integration is three float sliders, five Boolean
toggles, and two binding recorders. It should require:

1. one declarative menu file;
2. one small domain-binding adapter;
3. the provider's existing load/apply/save functions; and
4. generated headless contract tests.

It should contain no handwritten Win32 host discovery, ABI member-offset checks,
descriptor string copying, generic transaction bookkeeping, or Scaleform code.

This checkpoint does not yet satisfy that complete target. Host discovery,
generated typed binding stubs, subscriber scaffolding, and first-party migration
remain subsequent slices.
