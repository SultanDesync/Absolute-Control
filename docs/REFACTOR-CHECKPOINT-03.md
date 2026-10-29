# Refactor checkpoint 03: device vocabulary and legacy token fixtures

2026-09-18. Internal R2 design/codec slice following the
[headless authority harness](REFACTOR-CHECKPOINT-02.md). No provider, subscriber,
configuration file or public ABI is migrated by this checkpoint.

## Executable legacy mapping

`LegacyHotasToken` decodes Input Bus tokens into separate physical and channel
identities. It accepts the producer's brace GUID identity and preserves the
original saved string byte-for-byte, even when interpretation fails. The
canonical identity uses uppercase GUID text; it is not written back to old
consumers, whose current device matching can be case-sensitive. Device names,
enumeration indices, HOTAS INI `BindingRef` syntax and arbitrary opaque identity
strings remain unresolved rather than being assigned a guessed physical device.

The codec has explicit `HotasCapture` and `ZeroPower` dialects because source
inspection found conflicting interpretations:

| Token suffix | HOTAS capture / Head Tracking | Zero / Power |
|---|---|---|
| `@button:1` | button ordinal 0 | button ordinal 0 |
| `@pov:0:up` | POV ordinal 0 | POV ordinal 0 |
| `@pov:1:right` | POV ordinal 1 | POV ordinal 0 |
| `@pov:3:left` | POV ordinal 3 | POV ordinal 2 |
| `@pov:4:N` | rejected | POV ordinal 3 |
| `@pov:129` | rejected | POV ordinal 0, up |
| `@axis:0x30` | scalar ordinal 0 | rejected |

An unspecified dialect returns `AmbiguousDialect` when both parsers accept the
text but disagree. It accepts a unique interpretation when only one dialect
accepts the text; that is not proof of its originating subscriber. The caller
must supply the known subscriber dialect for migration. In particular, all
12 producer-form directions on POV ordinals 1–3 are ambiguous. Saved Zero/Power
bindings must preserve their existing interpretation; fresh captures can use
the producer's explicit `BindingV1.kind/controlId` as source evidence. Correcting
an existing binding's behavior is a separate subscriber-owned migration.

`EncodeCapture` produces new HOTAS producer-format text. It is not a generic
serialization method for existing Zero/Power configurations. Their persistence
path must retain `Decoded.original`; decoding and re-encoding in the wrong
dialect would recreate the same bug. Canonical channel IDs (`button/0`,
`axis/0`, `pov/0/direction/up`) are provider-local and always paired with the
provider and physical identity. Two identical products do not share identity.

The new parser is deliberately bounded to 255 bytes, matching NUL-terminated
`BindingV1.bindingText[256]`. Some legacy consumers accept broader inputs,
including opaque device names or 256-character text. Unsupported inputs are
retained but cannot enter the canonical registry through this codec.

## Provider-neutral DeviceService draft

`include/runtime/DeviceServiceDraft.h` is a compilable POD copy-out contract
sketch. It has no exported query, production implementation or public SDK
installation. The internal `draftVersion` is not a stable runtime ABI promise.

The draft separates provider identity, persistent physical identity, channel
identity and display label. Provider epoch, enumeration revision and connection
epoch are distinct. Per-device frames carry sequence, source time, arrival time
and explicit validity. The lifecycle journal remains separate from samples.

Proposed shape interpretation (to qualify before freezing the ABI):

| Shape | Components of `value` |
|---|---|
| Button | one Boolean value (0 or 1), optional cumulative edge counters |
| Scalar | one value with declared units |
| Vector | 2–4 values with declared units |
| RelativeDelta | 1–4 increments since the previous observed sample; consume once per sequence |
| Contact | active, x, y, pressure; each contact slot has its own channel ID |
| Pov | angle in radians, -1 when centered; digital directions are separate Button channels |
| Pose | position xyz in metres, then quaternion xyzw; coordinate convention still a freeze gate |

Raw and published values are separate. Calibration bounds and an optional
policy revision preserve the existing transform provenance. A HOTAS mirror
must mark values `LegacyPublished`, copy raw/calibrated values and bounds, and
leave the revision unknown until it can observe a real revision. It must not
calibrate an already calibrated value a second time. Per-component raw units
and mixed-unit pose/contact conventions still need provider qualification.

Copy-out function rules for a future implementation:

- Every output buffer is caller-owned; the service copies one coherent snapshot
  under its own synchronization. No pointer is retained and no callback invoked.
  Reads must not acquire devices or perform blocking provider I/O.
- `copyDevices` lists known devices including disconnected entries while they
  remain registered. A changed identity set increments enumeration revision even
  if its count stays the same.
- `copyChannels` and `copyLatest` compare the expected provider/physical identity,
  provider epoch, enumeration revision and connection epoch exactly. Mismatch
  returns `StaleGeneration`; zero is unknown, not a wildcard.
- `required` is mandatory. A null records pointer is allowed only with zero
  capacity. Insufficient capacity returns `BufferTooSmall` and the required
  count, with no partial records or frame publication. Non-buffer errors leave
  output records unchanged. Readers retry after enumeration changes.
- Struct sizes, draft version, capabilities and required function pointers are
  validated before a handle is retained. Public layout sizes, calling convention,
  maximum records, handle lifetime and lock-time bounds remain freeze gates.

Source sequence increases within a provider epoch; readers reject duplicate or
older sequences. Epoch/connection changes rebase digital baselines. Counters
retain their source width: HOTAS counters are modulo 32 bits, not inferred
64-bit totals. Unsigned differences preserve observed counts across one wrap;
multiple wraps and the ordering of press/release events cannot be recovered.
First observations and reconnections must not invent presses for held buttons.

Unknown, disconnected and stale differ from a valid zero sample. Freshness uses
an explicit consumer age limit in one monotonic nanosecond clock. HOTAS v1 has
no acquisition timestamp or connection epoch: a mirror may record receipt time,
but must leave unavailable source metadata unknown. Repeated reads of one
sequence cannot refresh its age. Stale data cannot authorize capture, routing
or a fallback provider. A connection epoch inferred from observed disconnects
does not prove that unobserved disconnect/reconnect cycles never happened.

## Fixture provenance and validation

The checked-in corpus contains 42 source-derived cases, plus exhaustive loops
for 128 buttons, 16 POV directions and 8 axes. Tests cover round trips, legacy
aliases, the 12 ambiguity cases, exact original-text preservation, distinct
physical identities, malformed input and numeric overflow. The draft records'
POD/layout checks compile with the test. All checks remain active in optimized
builds. No test opens DirectInput or requires a sibling checkout.

[Fixture provenance](../tests/fixtures/legacy-hotas-provenance.json) records the
audited source paths and byte hashes. These fixtures are not live or recorded
hardware traces. Source anchors:

- [HOTAS producer](../../AbsoluteHOTAS/src/InputBus.cpp), `CommitCaptureLocked`;
- [Head Tracking parser](<../../Absolute Head Tracking/src/InputBusBinding.cpp>);
- [Zero parser](../../AbsoluteZero-Ship-Control/src/InputBusClient.cpp);
- [Power parser](<../../Absolute Power/src/JoystickShortcut.cpp>).

Run with `xmake test 'legacy_hotas_token_test/*'`. MSVC `releasedbg` validation
passed all four selected xmake tests: the codec, headless core, native
decomposition and release/research boundary. The normal installed compiler/cache
was used; no plugin build, deployment or game run was needed.

## Remaining R2 work

Subsequent design direction: the new runtime will use native device/binding and
profile records, with no enduring HOTAS profile translator. The codec remains
useful for source comparison and a possible one-time importer; it does not
define the target saved format. Design native record ownership, profile
inheritance/activation and rebinding outcomes before production integration.

Implement bounded coherent frame storage and a copied-frame HOTAS shadow
adapter, using these contracts to expose missing metadata honestly. Record real
traces for reconnect, identical devices, edge stalls and profile/calibration
changes. Verify subscriber-specific POV behavior before choosing a saved-binding
migration. R0 production instrumentation, R1 production startup and M1 in-game
coexistence gates remain open. No input or output authority has moved.
