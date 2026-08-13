# Local vanilla interface references

This directory is a local, read-only workspace for interface files extracted from
the installed copy of Starfield. Bethesda-owned binaries are research inputs only:
they must not be committed, redistributed, deployed as part of SLOP, or modified in
place.

Preserve each file's path from `Starfield - Interface.ba2`. The initial targets are
the SWFs backing `MainMenu` and `PauseMenu`, followed by any shared interface SWFs
they import. The repository ignores every file in this directory except this note.
