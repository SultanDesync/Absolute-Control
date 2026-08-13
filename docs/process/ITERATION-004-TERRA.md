# Builder-process iteration 004: phased implementation with independent acceptance

## Result

Two fresh mid-tier contexts independently implemented the optional AbsoluteZero subscriber and
the renderer-neutral SLOP host session. The subscriber build passed 3/3 project tests; the host
build passed 2/2. Repository ownership boundaries were respected.

Neither phase advanced. Evaluator-owned fixtures, written after reviewing the candidates, rejected
both implementations:

- subscriber acceptance did not compile because runtime discovery had no headless `RegisterWith`
  seam and the alleged host-absence test compiled registration into a no-op;
- host acceptance failed because Action availability incorrectly called `readValue` and marked the
  row unavailable.

Review also identified stale subscriber rollback capture, dirty-page command bypasses, missing
ButtonBinding writes and read-kind validation, and dirty Close clearing state without rollback.

## Finding

Phased prompts successfully changed the model from auditing to implementing, but builder-authored
tests reproduced the implementation's assumptions. Passing project tests is not a sufficient
promotion gate.

## Changes for iteration 005

- Retain evaluator-owned acceptance fixtures as hidden regression authorities.
- Specify a testable optional-host registration seam and first-edit rollback timing.
- Apply dirty-page ownership to every addressed command, not only page navigation.
- Specify Action and ButtonBinding semantics plus read-kind validation.
- Require evaluator acceptance before dispatching the Scaleform phase.
