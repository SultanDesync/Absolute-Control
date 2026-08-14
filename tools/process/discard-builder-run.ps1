[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [Parameter(Mandatory = $true)]
    [string]$RunDirectory,

    [switch]$AllowLegacyV1
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'legacy\v1\LegacyV1.ps1')
Assert-LegacyV1OptIn -Allowed ([bool]$AllowLegacyV1) -EntryPoint 'discard-builder-run'

$resolvedRun = (Resolve-Path -LiteralPath $RunDirectory).Path
$runPath = Join-Path $resolvedRun 'run.json'
$evaluationPath = Join-Path $resolvedRun 'evaluation.json'
if (-not (Test-Path -LiteralPath $runPath -PathType Leaf)) {
    throw 'run.json is missing; refusing to infer worktree locations.'
}
if (-not (Test-Path -LiteralPath $evaluationPath -PathType Leaf)) {
    $phaseEvaluations = @(Get-ChildItem -LiteralPath $resolvedRun `
            -Filter 'phase-*-evaluation.json' -File -ErrorAction SilentlyContinue)
    if ($phaseEvaluations.Count -eq 0) {
        throw 'No overall or phase evaluation is present; evaluate the candidate before discarding it.'
    }
}

$run = Get-Content -Raw -LiteralPath $runPath | ConvertFrom-Json
if ($run.PSObject.Properties.Name -contains 'worktreeDirectory') {
    $worktreeDirectory = [System.IO.Path]::GetFullPath([string]$run.worktreeDirectory)
    $expectedHost = [System.IO.Path]::GetFullPath((Join-Path $worktreeDirectory 'h'))
    $expectedSubscriber = [System.IO.Path]::GetFullPath((Join-Path $worktreeDirectory 's'))
} else {
    $expectedHost = [System.IO.Path]::GetFullPath((Join-Path $resolvedRun 'host'))
    $expectedSubscriber = [System.IO.Path]::GetFullPath((Join-Path $resolvedRun 'subscriber'))
}
$actualHost = [System.IO.Path]::GetFullPath([string]$run.host.worktree)
$actualSubscriber = [System.IO.Path]::GetFullPath([string]$run.subscriber.worktree)
$comparison = [System.StringComparison]::OrdinalIgnoreCase

if (-not $actualHost.Equals($expectedHost, $comparison)) {
    throw 'Host worktree is not the exact run-local host directory.'
}
if (-not $actualSubscriber.Equals($expectedSubscriber, $comparison)) {
    throw 'Subscriber worktree is not the exact run-local subscriber directory.'
}

foreach ($candidate in @(
        @{ Label = 'host'; Source = [string]$run.host.source; Worktree = $actualHost },
        @{ Label = 'subscriber'; Source = [string]$run.subscriber.source; Worktree = $actualSubscriber })) {
    if (-not (Test-Path -LiteralPath $candidate.Worktree)) {
        Write-Output "$($candidate.Label) worktree already absent: $($candidate.Worktree)"
        continue
    }

    $registered = @(& git -C $candidate.Source worktree list --porcelain) -contains
        "worktree $($candidate.Worktree.Replace('\', '/'))"
    if (-not $registered) {
        # Git commonly reports native Windows separators. Check the canonical path as printed too.
        $registered = @(& git -C $candidate.Source worktree list --porcelain) -contains
            "worktree $($candidate.Worktree)"
    }
    if (-not $registered) {
        throw "$($candidate.Label) directory is not a registered worktree; refusing to remove it."
    }

    $reparsePoints = @(Get-ChildItem -LiteralPath $candidate.Worktree -Recurse -Force `
            -Attributes ReparsePoint -ErrorAction Stop)
    if ($reparsePoints.Count -ne 0) {
        $names = @($reparsePoints | ForEach-Object { $_.FullName }) -join ', '
        throw "Worktree contains filesystem links; remove them explicitly before disposal: $names"
    }

    if ($PSCmdlet.ShouldProcess($candidate.Worktree, 'Remove disposable Git worktree and all candidate changes')) {
        & git -C $candidate.Source worktree remove --force $candidate.Worktree
        if ($LASTEXITCODE -ne 0) {
            throw "Git could not remove the $($candidate.Label) worktree."
        }
        Write-Output "Discarded $($candidate.Label) candidate worktree."
    }
}

Write-Output 'Available run metadata, results, evaluations, and specification snapshot were retained.'
