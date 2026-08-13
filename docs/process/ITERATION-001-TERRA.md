# Builder-process iteration 001: Terra baseline

## Setup

- Builder class: mid-tier Terra model, medium reasoning.
- Context: no conversation history; two repository locations and seven ordered documents.
- Workspace: live shared repositories.
- Intended result: generic host renderer plus first external AbsoluteZero subscriber.
- User interventions: zero.
- Evaluator interventions after dispatch: three—one worktree-preservation warning and two
  reminders that registration/enumeration did not satisfy the objective.

## Observed result

| Gate | Result |
|---|---|
| Locate and understand public API | Passed |
| Preserve host-optional module behavior | Passed |
| Construct AbsoluteZero descriptors/callbacks | Passed |
| Build AbsoluteZero | Passed after developer-environment recovery |
| Subscriber tests | Passed |
| Add registry enumeration seam | Partial |
| Generic native bridge | Not implemented |
| Dynamic ActionScript renderer | Not implemented |
| In-game validation | Not run |
| Truthful completion report | Passed |

The implementation changes were removed after evaluation. They were experimental evidence, not
a candidate patch.

## What the documents conveyed successfully

- SLOP is a host and modules retain configuration ownership.
- The ABI must remain C-compatible and host-optional.
- AbsoluteZero is the smaller first subscriber.
- Privacy, fail-closed behavior, and existing tests matter.
- The fixed synthetic UI was not the final renderer.

## Missing process information

- No single builder entry point or phase order.
- No exact native/ActionScript model schema.
- No exact command payload or validation rules.
- Apply/cancel/close transaction semantics were underdetermined.
- “Dynamic rendering” lacked a mechanical completion test.
- The correct Visual Studio developer-environment startup was not an early baseline gate.
- Live shared worktrees made experimental output harder to distinguish from user work.

## Changes for iteration 002

- Use detached disposable worktrees at recorded commits.
- Give the builder only `BUILDER-RUNBOOK.md` as its entry point.
- Fix bridge and transaction choices in `BRIDGE-PROTOCOL-V1.md`.
- Require a machine-readable result and per-gate statuses.
- Prohibit stopping at registration, enumeration, or a plan.
- Score autonomy and reporting separately from code completion.
- Invoke process tooling through policy-safe Windows wrappers and snapshot all builder inputs.
