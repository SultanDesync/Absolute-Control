[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$HostRepository,

    [Parameter(Mandatory = $true)]
    [string]$SubscriberRepository,

    [string]$HostRef = 'HEAD',
    [string]$SubscriberRef = 'HEAD',
    [string]$Model = 'unspecified',
    [string]$RunRoot,
    [string]$WorktreeRoot,
    [switch]$SkipBaselineBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$gitExecutable = (Get-Command git.exe -ErrorAction Stop).Source
$gitRoot = Split-Path -Parent (Split-Path -Parent $gitExecutable)
$gitUnixTools = Join-Path $gitRoot 'usr\bin'
if (Test-Path -LiteralPath $gitUnixTools -PathType Container) {
    $env:PATH = "$gitUnixTools;$env:PATH"
}

function Resolve-Repository {
    param([string]$Path, [string]$Label)

    $resolved = (Resolve-Path -LiteralPath $Path).Path
    & git -C $resolved rev-parse --is-inside-work-tree 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "$Label is not a Git worktree: $resolved"
    }
    return $resolved
}

function Resolve-Commit {
    param([string]$Repository, [string]$Reference)

    $commit = (& git -C $Repository rev-parse "$Reference^{commit}").Trim()
    if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') {
        throw "Could not resolve $Reference in $Repository."
    }
    return $commit
}

$hostSource = Resolve-Repository -Path $HostRepository -Label 'Host repository'
$subscriberSource = Resolve-Repository -Path $SubscriberRepository -Label 'Subscriber repository'
$hostCommit = Resolve-Commit -Repository $hostSource -Reference $HostRef
$subscriberCommit = Resolve-Commit -Repository $subscriberSource -Reference $SubscriberRef

if ([string]::IsNullOrWhiteSpace($RunRoot)) {
    $RunRoot = Join-Path $hostSource 'artifacts\builder-runs'
}
if ([string]::IsNullOrWhiteSpace($WorktreeRoot)) {
    $WorktreeRoot = Join-Path (Split-Path -Parent $hostSource) '.slop-worktrees'
}
$runId = 'builder-{0}' -f [DateTimeOffset]::UtcNow.ToString('yyyyMMdd-HHmmss')
$runDirectory = Join-Path $RunRoot $runId
$worktreeDirectory = Join-Path $WorktreeRoot $runId
$hostWorktree = Join-Path $worktreeDirectory 'h'
$subscriberWorktree = Join-Path $worktreeDirectory 's'
$specDirectory = Join-Path $runDirectory 'spec'

if (Test-Path -LiteralPath $runDirectory) {
    throw "Builder run directory already exists: $runDirectory"
}
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $specDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $worktreeDirectory -Force | Out-Null

$specSources = [ordered]@{
    'BUILDER-RUNBOOK.md' = Join-Path $hostSource 'docs\BUILDER-RUNBOOK.md'
    'BRIDGE-PROTOCOL-V1.md' = Join-Path $hostSource 'docs\BRIDGE-PROTOCOL-V1.md'
    'MODULE-API.md' = Join-Path $hostSource 'docs\MODULE-API.md'
    'process\DISPOSABLE-ITERATIONS.md' = Join-Path $hostSource 'docs\process\DISPOSABLE-ITERATIONS.md'
    'include\SlopAPI.h' = Join-Path $hostSource 'include\SlopAPI.h'
    'builder-result.schema.json' = Join-Path $hostSource 'tools\process\builder-result.schema.json'
    'build-host.ps1' = Join-Path $hostSource 'tools\process\build-host.ps1'
    'build-subscriber.ps1' = Join-Path $hostSource 'tools\process\build-subscriber.ps1'
    'build-host.cmd' = Join-Path $hostSource 'tools\process\build-host.cmd'
    'build-subscriber.cmd' = Join-Path $hostSource 'tools\process\build-subscriber.cmd'
    'phases\01-SUBSCRIBER.md' = Join-Path $hostSource 'docs\process\phases\01-SUBSCRIBER.md'
    'phases\02-HOST-CORE.md' = Join-Path $hostSource 'docs\process\phases\02-HOST-CORE.md'
    'phases\03-SCALEFORM.md' = Join-Path $hostSource 'docs\process\phases\03-SCALEFORM.md'
}
$specManifest = [ordered]@{}
foreach ($name in $specSources.Keys) {
    $source = $specSources[$name]
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required builder input is missing: $source"
    }
    $destination = Join-Path $specDirectory $name
    $destinationParent = Split-Path -Parent $destination
    if (-not (Test-Path -LiteralPath $destinationParent)) {
        New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null
    }
    Copy-Item -LiteralPath $source -Destination $destination
    $specManifest[$name] = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash.ToLowerInvariant()
}

& git -C $hostSource worktree add --detach $hostWorktree $hostCommit
if ($LASTEXITCODE -ne 0) {
    throw 'Could not create the detached host worktree.'
}
& git -C $hostWorktree submodule update --init --recursive
if ($LASTEXITCODE -ne 0) {
    throw 'Could not initialize the detached host dependencies.'
}
& git -C $subscriberSource worktree add --detach $subscriberWorktree $subscriberCommit
if ($LASTEXITCODE -ne 0) {
    throw 'Could not create the detached subscriber worktree. The host worktree was retained for inspection.'
}
& git -C $subscriberWorktree submodule update --init --recursive
if ($LASTEXITCODE -ne 0) {
    throw 'Could not initialize the detached subscriber dependencies.'
}

$candidateTools = Join-Path $hostWorktree '.tools'
New-Item -ItemType Directory -Path $candidateTools -Force | Out-Null
foreach ($toolDirectory in @('apache-flex-sdk-4.16.1', 'playerglobal-repo')) {
    $sourceTool = Join-Path (Join-Path $hostSource '.tools') $toolDirectory
    if (-not (Test-Path -LiteralPath $sourceTool -PathType Container)) {
        throw "Pinned interface toolchain is missing: $sourceTool"
    }
    New-Item -ItemType Junction -Path (Join-Path $candidateTools $toolDirectory) `
        -Target $sourceTool | Out-Null
}

$run = [ordered]@{
    schemaVersion = 1
    runId = $runId
    model = $Model
    createdUtc = [DateTimeOffset]::UtcNow.ToString('o')
    worktreeDirectory = $worktreeDirectory
    host = [ordered]@{
        source = $hostSource
        worktree = $hostWorktree
        baseline = $hostCommit
    }
    subscriber = [ordered]@{
        source = $subscriberSource
        worktree = $subscriberWorktree
        baseline = $subscriberCommit
    }
    runbook = Join-Path $specDirectory 'BUILDER-RUNBOOK.md'
    specification = [ordered]@{
        directory = $specDirectory
        sha256 = $specManifest
    }
    result = Join-Path $runDirectory 'builder-result.json'
}
[System.IO.File]::WriteAllText(
    (Join-Path $runDirectory 'run.json'),
    ($run | ConvertTo-Json -Depth 6),
    [System.Text.UTF8Encoding]::new($false))

$resultTemplate = [ordered]@{
    schemaVersion = 1
    runId = $runId
    model = $Model
    baselines = [ordered]@{
        host = $hostCommit
        subscriber = $subscriberCommit
    }
    documentsRead = @()
    interventions = @()
    gates = [ordered]@{
        baseline = 'not_run'
        subscriberApi = 'not_run'
        subscriberHostAbsence = 'not_run'
        hostModel = 'not_run'
        dynamicSwf = 'not_run'
        genericCommands = 'not_run'
        transactionTests = 'not_run'
        hostBuild = 'not_run'
        subscriberBuild = 'not_run'
        inGame = 'not_run'
        privacy = 'not_run'
    }
    commands = @()
    changedFiles = [ordered]@{ host = @(); subscriber = @() }
    assumptions = @()
    blockers = @()
    reproducible = $false
}
[System.IO.File]::WriteAllText(
    (Join-Path $runDirectory 'builder-result.template.json'),
    ($resultTemplate | ConvertTo-Json -Depth 8),
    [System.Text.UTF8Encoding]::new($false))

$preflightPath = Join-Path $runDirectory 'preflight.json'
$preflight = [ordered]@{
    schemaVersion = 1
    host = if ($SkipBaselineBuild) { 'not_run' } else { 'pending' }
    subscriber = if ($SkipBaselineBuild) { 'not_run' } else { 'pending' }
    error = ''
}
function Write-Preflight {
    [System.IO.File]::WriteAllText(
        $preflightPath,
        ($preflight | ConvertTo-Json -Depth 4),
        [System.Text.UTF8Encoding]::new($false))
}
Write-Preflight
if (-not $SkipBaselineBuild) {
    try {
        & (Join-Path $hostSource 'tools\process\build-host.ps1') `
            -Repository $hostWorktree
        $preflight.host = 'passed'
        Write-Preflight

        & (Join-Path $hostSource 'tools\process\build-subscriber.ps1') `
            -Repository $subscriberWorktree
        $preflight.subscriber = 'passed'
        Write-Preflight
    } catch {
        if ($preflight.host -eq 'pending') { $preflight.host = 'failed' }
        elseif ($preflight.subscriber -eq 'pending') { $preflight.subscriber = 'failed' }
        $preflight.error = $_.Exception.Message
        Write-Preflight
        throw
    }
}

$prompt = @"
You are running builder-process evaluation $runId with model label $Model.

Read only this entry point first:
$($run.runbook)

The files beside that runbook are the immutable specification snapshot for this run. Resolve its
document links within that directory. Repository copies may be older and the source repository
may contain later process edits; neither overrides the snapshot.

Work only in these disposable detached worktrees:
- SLOP host: $hostWorktree
- AbsoluteZero subscriber: $subscriberWorktree

Run metadata and the result template are in:
$runDirectory

The evaluator already ran the clean baseline builds recorded in preflight.json. Reuse
build-host.ps1 and build-subscriber.ps1 from the specification directory after edits. If the
tool sandbox denies a prepared dependency cache, request the narrow build permission instead of
changing source or substituting another toolchain.

Follow the runbook through every independent gate. Do not stop at subscriber registration,
registry enumeration, a plan, or a partial static renderer. Do not edit the source repositories,
commit, push, open a PR, or search for private environment values. Copy the result template to
builder-result.json and maintain it as you work. Your final report must match that file. All
candidate code will be discarded after evaluation; do not attempt to promote it.
"@
[System.IO.File]::WriteAllText(
    (Join-Path $runDirectory 'BUILDER-PROMPT.md'),
    $prompt,
    [System.Text.UTF8Encoding]::new($false))

Write-Output $runDirectory
