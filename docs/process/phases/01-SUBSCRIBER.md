# Builder phase 01: optional subscriber adapter

Work only in the supplied AbsoluteZero worktree. The SLOP host worktree must remain unchanged.
This phase is complete only when AbsoluteZero builds and tests through its existing presets.

## Required implementation

- Vendor the supplied `include/SlopAPI.h` unchanged under a clear SDK path and add an ABI layout
  test. Do not write a compressed duplicate.
- After SFSE post-post-load, look up the already-loaded SLOP DLL and resolve `SLOP_QueryApi` with
  Windows runtime APIs. Validate ABI version, `structSize`, identity, and every function used.
- Put API validation/registration behind a headless seam with this exact callable shape:
  `SlopApi::Result RegisterWith(QueryApiCallback query, const PageDescriptorV1& page) noexcept`,
  where `QueryApiCallback` matches `SLOP_QueryApi`. A null query returns `NotFound`, a query that
  returns null returns `NotReady`, malformed/foreign API identity returns `Rejected`, and the host
  `registerPage` result is forwarded. The exact case-sensitive v1 host identity is `SLOP`, matching
  `MenuApiHost::g_api`; do not invent a lowercase identity in adapter tests. The Windows lookup
  calls this seam; tests do not compile the resolver into a no-op.
- Windows discovery tries the release module `SLOP.dll` first and the temporary research module
  `AbsoluteControlPanelResearch.dll` second so this candidate can be validated before product
  renaming. Both must pass through the same exported query/identity validation. The research alias
  is removed from the eventual SDK example after the host is renamed.
- Missing DLL, export, incompatible API, registration rejection, or any callback error is a normal
  logged state. AbsoluteZero must continue its existing INI, hotkey, and gameplay initialization.
- Register one `Mouse Alignment` page with these controls in order:

| ID | Kind | Range / step | Flags |
|---|---|---|---|
| `enabled` | Toggle | Boolean | none |
| `radius` | FloatSlider | 1–200 / 1 | none |
| `idle-ms` | IntegerSlider | 10–500 / 10 | none |
| `decay-rate` | FloatSlider | 0.5–20 / 0.5 | none |
| `poll-rate-hz` | IntegerSlider | 30–500 / 10 | none |
| `suppress-key` | IntegerSlider | 0–255 / 1 | advanced |
| `diagnostic-log` | Toggle | Boolean | requires restart |

- Reuse `Configuration::Get`, `Apply`, and `Save`. When no edit is active, reads and the first
  accepted draft write refresh from `get`; the first write retains that current configuration as
  the rollback snapshot. Do not capture rollback state only at adapter construction. Validate
  exact kinds, finite values, and ranges before live applying.
  `apply` saves and starts a clean session. `cancel` reapplies the retained configuration.
- Do not add Save as an Action control. Apply and Cancel belong to SLOP.

## Mechanical proof

Every descriptor must include a useful non-empty description. Add tests that cover ABI layout,
every read/write mapping, wrong kind, unknown ID, non-finite and out-of-range rejection without
mutation, an external configuration change before the first edit, apply/save delegation, cancel
rollback, null/absent/incompatible host queries, and successful registration forwarding.
Run the supplied subscriber build wrapper. Write `phase-01-result.json` at the run root with
`status`, `commands`, `changedFiles`, and `blockers`. Do not commit or touch the host worktree.
