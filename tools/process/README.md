# Builder-process tools

Use the `.cmd` entry points on Windows. They provide a deterministic, non-interactive PowerShell
invocation even when the machine policy blocks unsigned local `.ps1` files.

```text
new-builder-run.cmd       Create detached worktrees and an immutable specification snapshot.
evaluate-builder-run.cmd  Check the builder's result claims and record a discard disposition.
discard-builder-run.cmd   Remove both evaluated candidate worktrees and retain run evidence.
build-host.cmd             Run the pinned SLOP build and tests in an explicit worktree.
build-subscriber.cmd       Load VS/Ninja/vcpkg and run the subscriber build and tests.
new-phase-prompt.cmd        Create a bounded clean-context handoff for one passing work unit.
evaluate-phase.cmd          Run project builds plus evaluator-owned semantic acceptance tests.
```

Every run is created beneath the ignored `artifacts/builder-runs` directory unless `-RunRoot` is
specified. A run is evidence about the builder process, not a source branch or release candidate.
Run creation initializes pinned submodules and requires both clean baseline builds unless the
caller deliberately supplies `-SkipBaselineBuild` for a tooling smoke test. Evidence remains in
the ignored repository artifacts directory; disposable worktrees use a short sibling path to
stay below Windows/CommonLibSF generated-path limits.
