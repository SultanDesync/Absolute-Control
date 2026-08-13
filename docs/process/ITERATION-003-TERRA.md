# Builder-process iteration 003: prepared monolithic task

## Result

Both clean baselines passed before dispatch. The builder independently reran both and reported
baseline, host build, subscriber build, and privacy as passed. The evaluator scored 53/100.

The builder made no source changes. It audited and truthfully reported that the subscriber,
generic host model, command transactions, and descriptor-driven SWF were absent. Both disposable
worktrees were removed.

## Finding

Environment discovery was no longer the limiting factor. One mid-tier context still treated the
full subscriber + host core + native bridge + ActionScript rewrite as an audit instead of an
implementation task. Repeating the same monolithic prompt would measure the same failure.

## Changes for iteration 004

- Split the work into independently testable subscriber, host-core, and Scaleform phases.
- Give each fresh context a single worktree ownership boundary and exact mechanical proof.
- Run subscriber and host-core phases independently; advance to Scaleform only when both pass.
- Keep the combined candidate disposable until the final integration and in-game gates pass.
