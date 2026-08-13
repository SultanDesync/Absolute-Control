# Builder-process iteration 002: clean-context baseline

## Setup

- Builder class: mid-tier Terra model, medium reasoning.
- Context: no conversation history or previous candidate source.
- Workspace: detached host and subscriber worktrees at recorded commits.
- Inputs: six immutable specification files with recorded SHA-256 hashes.
- Evaluator intervention: none; the builder recorded one environment intervention.

## Result

The evaluator scored the run 35/100. Privacy passed. All candidate source was discarded after
evaluation and the ignored result/evaluation files were retained.

The builder again produced a plausible fail-optional AbsoluteZero adapter and a partial copied
host model, but did not replace the fixed native/ActionScript renderer. It reported the dynamic
SWF and generic-command gates as failed rather than claiming completion.

## Process failures

- Detached worktrees did not initialize the pinned CommonLibSF submodule.
- The builder had to rediscover Visual Studio environment initialization.
- Ninja was installed inside Visual Studio but absent from `PATH`.
- The installed vcpkg root was not supplied, and its registry cache required a sandbox permission.
- A misleading `xmake test` invocation selected the parent project until `-P` was used.
- Environment recovery consumed the context needed for the native/Scaleform rewrite.

## Changes for iteration 003

- Initialize all pinned submodules when creating disposable worktrees.
- Resolve Visual Studio, its bundled Ninja, and its vcpkg installation mechanically.
- Run both clean baseline builds before dispatching a builder.
- Supply exact build wrappers that always target the detached worktrees.
- Treat environment preparation as harness responsibility and preserve builder context for the
  descriptor-driven bridge and SWF work.
- Compile disposable candidates from a short worktree root so generated CommonLibSF/PCH paths do
  not exceed Windows path limits; retain only their evaluation evidence under repository artifacts.
