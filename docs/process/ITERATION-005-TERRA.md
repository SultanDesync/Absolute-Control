# Builder-process iteration 005: hidden regression rerun

## Result

Both fresh contexts implemented the tightened phases. Project builds and tests passed: 4/4 for
the subscriber and 2/2 for the host. The evaluator fixture was corrected to avoid assuming an
unspecified adapter class or Dispatch return type.

The renderer-neutral host core passed hidden semantic acceptance and earned `advance`. The
subscriber failed actual host compatibility: `RegisterWith` required lowercase `slop`, while the
published `MenuApiHost::g_api` identity is case-sensitive `SLOP`. Its Windows discovery also looked
only for the future `SLOP.dll`, not the current research DLL needed by the isolated game proof.

The entire combined candidate was discarded despite the host phase passing.

## Changes for iteration 006

- Specify the exact published API identity rather than the vague word “identity.”
- Require release-name-first discovery with the temporary research filename as a validation alias.
- Retain the implementation-independent hidden subscriber and host fixtures.
- Rerun both phases from original commits before advancing to Scaleform.
