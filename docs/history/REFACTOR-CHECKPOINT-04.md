# Refactor checkpoint 04: AC-owned native device backend

2026-09-18. First implementation of B1 in the revised
[native-backend-first plan](SUITE-REFACTOR-TRANSITION-PLAN.md).

## Implemented

AC now starts an owned device worker at SFSE post-post-load, before calling the
menu bootstrap. Menu configuration, relocation validation, factory registration
and SWF availability do not control this worker. The initial runtime remains
inside the AC plugin. Both product and ResearchDev builds include the backend;
they must not be loaded together.

`BackendRuntime` owns its provider object and worker. Initialization and polling
run on the same worker; Stop cancels the wait, joins the worker and completes
provider cleanup before returning. Partial startup and polling exceptions stop
the provider, clear the data store and report `Degraded` with a copied failure
detail. Explicit Stop/Start is supported for harnesses; no automatic legacy
fallback or device-owner transfer occurs.

`DirectInputProvider` uses DirectInput directly, with background nonexclusive
cooperation against a visible process-owned top-level window. It enumerates
attached game controllers once per second and polls at an approximately 8 ms
interval. Device identity is the instance GUID; names are display metadata.
Actual enumerated standard axes, buttons and POVs produce native channel IDs.
Raw POV angles and derived cardinal Button channels are separate observations.
Driver-reported axis ranges normalize physical values to [-1,1] when available;
otherwise values retain provider units. This is not saved user calibration.

The provider retains disconnected identities, attempts reacquisition and reopens
failed handles at the enumeration interval. Connection epochs change on observed
reconnection; first/reconnected held buttons establish a baseline without new
presses. Enumeration revision changes when the attached identity set changes,
including same-count replacement. Source sequence, sample/receipt timestamps
and 64-bit cumulative digital counters are published with every device frame.
The timestamp records completion of the device read, not a hardware clock.

`DeviceStore` implements the native copied data plane with synchronized reads,
generation validation, atomic inventory replacement and coherent frame copies.
It retains at most 16 device identities, 256 channels/device and eight frames per
device. Capacity overflow fails explicitly. Frames with duplicate/older sequence,
backward time, mismatched descriptors, nonfinite values or malformed counters
are rejected. Reads assess freshness without refreshing the timestamp or
mutating stored frames. Insufficient output capacity returns the required count
without publishing partial records. No read acquires hardware or calls provider
code; samples never enter Scaleform or the lifecycle journal.

## Deployment and lifetime boundary

This is a native development cohort, not an interoperable upgrade of the old
suite. Startup refuses device acquisition when `AbsoluteHOTAS.dll` is loaded,
or when both AC hosts are loaded. It logs the rejected pairing and continues the
menu path. The worker repeats the pairing check before acquisition. That check
will need a native subscriber contract when the redesigned HOTAS is introduced;
it does not attempt to negotiate or delegate to the old Input Bus. Existing
saved files are not read or modified by this backend.

Runtime DLL hot unload remains unsupported. SFSE exposes no shutdown message in
the current header. The production singleton therefore has process lifetime and
does not join its worker from a DLL-detach/static destructor under loader lock.
Explicit Stop is available before teardown and is tested. Process termination
reclaims the singleton and OS resources. A future unload contract must stop and
drain it before unloading code.

`NativeBackend().Devices()` exposes internal read operations and
`NativeBackend().Failure()` exposes an asynchronous failure detail. This
checkpoint does not export/freeze a new subscriber ABI, install the draft as a
public SDK, render a device configuration page, or advertise missing services.
The earlier `HeadlessCore` remains an authority design harness rather than a
second production device owner.

## Validation and remaining work

The executable backend test uses the production store and worker with mock
providers. It covers no-menu startup, concurrent copy-out, stable copies,
buffer capacity, invalid publication, bounded history, stale reads, same-count
replacement, old connection rejection, initialization/poll failures, restart
and Stop waiting for an in-flight poll. Source guards ensure the plugin starts
the worker before presentation, links the production provider and does not
include the legacy-token codec in its runtime source list.

Validation passed: both product and ResearchDev plugins built with MSVC in
`releasedbg`, and all 18 native xmake tests passed, including the new backend
test and binary/source boundary checks. Digital policy fixtures exercise
reconnect baselines, releases, diagonal POV overlap and invalid POV values.
`git diff --check` passed. No DLL deployment or game run has been performed.
DirectInput acquisition, window selection, hotplug, hardware
counter behavior, power/suspend and device-driver failure still require live
qualification; mock tests do not establish those results.

Next: finish B1 diagnostics and device qualification, then build native capture,
saved calibration, AC-owned binding/route/profile records, context, arbitration
and output backends. These services are not implemented by this checkpoint.
Subscriber adaptation follows the native backend build, one module at a time.
