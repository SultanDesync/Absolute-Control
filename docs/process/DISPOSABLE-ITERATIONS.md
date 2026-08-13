# Disposable builder iterations

SLOP is the product. The builder harness is an experiment for discovering a repeatable way to
produce that product and its subscriber integrations. These two streams must not share an
implicit notion of progress.

## Stable product baseline

The source branch contains only reviewed facts that survived an iteration:

- the versioned provider C ABI and its layout tests;
- proven native-menu lifecycle and bridge constraints;
- reproducible build, launch, and evidence procedures;
- mechanical completion gates; and
- concise records of failed approaches and their evidence.

An agent's implementation is not a new baseline merely because it compiles, looks plausible, or
implements one phase. Builder agents cannot promote their own output.

## Disposable candidate rule

Every evaluation begins at recorded commits in two detached worktrees and with a fresh agent
context. The agent receives an immutable snapshot of the runbook and protocol, not conversation
history or another candidate's source changes.

Candidate code remains disposable until its bounded phase passes project tests and independent
acceptance. An accepted SLOP host/API phase is cumulative product work: a maintainer promotes that
reviewed diff into the active SLOP branch and it becomes the baseline for subsequent host phases.
The next agent is allowed to build on this accepted product history.

Subscriber integrations are judged and promoted independently. A subscriber failure does not
invalidate an accepted host phase, and a host failure does not require deleting an independently
accepted subscriber patch. Unaccepted candidate changes, ignored build output, and agent context
are still discarded; retain the machine-readable result, evaluator report, and iteration note.

Promotion is a separate maintainer action. Host-core changes require their mandatory headless and
independent semantic gates. Native UI, invocation, packaging, or player-facing changes additionally
require the relevant isolated in-game scenarios and privacy validation before release claims.

## Iteration loop

1. Convert the previous failure into a specification, fixture, oracle, or automated decision.
2. Create clean detached worktrees and snapshot the exact builder inputs.
3. Start a builder with no inherited conversation or previous candidate context.
4. Let it complete every independent gate and write `builder-result.json`.
5. Evaluate claims against Git state and mechanical source/test evidence.
6. Record the smallest process defect that explains each failure.
7. Promote each independently accepted product phase; remove the remaining candidate worktrees.
8. Change the harness for generalized failures and continue from the newly accepted product
   baseline where applicable.

The target is not zero judgment everywhere. The target is to automate routine technical choices
and surface the few decisions that genuinely change the public API, module behavior, or player
experience.
