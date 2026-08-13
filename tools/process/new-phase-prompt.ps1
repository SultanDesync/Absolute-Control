[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RunDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateSet('01', '02', '03')]
    [string]$Phase,

    [string]$Model = 'unspecified'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedRun = (Resolve-Path -LiteralPath $RunDirectory).Path
$run = Get-Content -Raw -LiteralPath (Join-Path $resolvedRun 'run.json') | ConvertFrom-Json
$preflight = Get-Content -Raw -LiteralPath (Join-Path $resolvedRun 'preflight.json') |
    ConvertFrom-Json
if ($preflight.host -ne 'passed' -or $preflight.subscriber -ne 'passed') {
    throw 'Both clean baselines must pass before creating a phase prompt.'
}

$cards = @{
    '01' = '01-SUBSCRIBER.md'
    '02' = '02-HOST-CORE.md'
    '03' = '03-SCALEFORM.md'
}
$card = Join-Path $run.specification.directory (Join-Path 'phases' $cards[$Phase])
if (-not (Test-Path -LiteralPath $card -PathType Leaf)) {
    throw "Immutable phase card is missing: $card"
}
$promptPath = Join-Path $resolvedRun "PHASE-$Phase-PROMPT.md"
$prompt = @"
You are executing SLOP builder phase $Phase in run $($run.runId), model label $Model.

Read this immutable phase card first and follow only its linked specification as needed:
$card

Disposable worktrees:
- SLOP host: $($run.host.worktree)
- AbsoluteZero subscriber: $($run.subscriber.worktree)

Run root for the required phase result:
$resolvedRun

Both clean baseline builds passed before dispatch. Use the .cmd build wrappers in the immutable
specification directory. Work through the implementation and mechanical proof; an audit-only
report is not completion. Do not commit, push, promote, edit either source repository, consult
another candidate, or search for private environment values.

For phase 03, build the SWF without creating links inside the disposable worktree:
$($run.specification.directory)\build-interface.cmd -Repository $($run.host.worktree) -ToolRoot $($run.host.source)\.tools
"@
[System.IO.File]::WriteAllText(
    $promptPath, $prompt, [System.Text.UTF8Encoding]::new($false))
Write-Output $promptPath
