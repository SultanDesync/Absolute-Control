# Archived disposable builder process v1

**Status: historical and opt-in.** This contract produced the retained
`artifacts/builder-runs` experiments during the SLOP proof-of-concept. It is preserved so those
runs can be interpreted or deliberately reproduced; it is not an Absolute Control Panel product
gate.

The archived implementation remains at the root of `tools/process` because historical run
snapshots and relative paths refer to those names. Its run creation, phase prompts, evaluators,
and disposal command now require `-AllowLegacyV1`. The opt-in does not make their conclusions
current.

Notable v1 assumptions include SLOP naming, two-repository detached candidates, synthetic source
markers, and a magenta framebuffer sentinel in the Scaleform phase. The sentinel established only
that a diagnostic color reached a rendered frame. It never proved callback delivery, transaction
semantics, persistence, input ownership, compatibility, or stability, and it is not a v2/current
requirement.

Use `tools/process/validate-current.cmd` for the maintained product gates. Do not copy v1 phase
cards or result statuses into a current release report.
