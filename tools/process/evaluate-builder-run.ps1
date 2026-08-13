[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RunDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedRun = (Resolve-Path -LiteralPath $RunDirectory).Path
$run = Get-Content -Raw -LiteralPath (Join-Path $resolvedRun 'run.json') |
    ConvertFrom-Json
$resultPath = Join-Path $resolvedRun 'builder-result.json'
if (-not (Test-Path -LiteralPath $resultPath -PathType Leaf)) {
    throw 'builder-result.json is missing.'
}
$result = Get-Content -Raw -LiteralPath $resultPath | ConvertFrom-Json

$errors = [System.Collections.Generic.List[string]]::new()
$notes = [System.Collections.Generic.List[string]]::new()
$requiredProperties = @(
    'schemaVersion', 'runId', 'model', 'baselines', 'documentsRead',
    'interventions', 'gates', 'commands', 'changedFiles', 'assumptions',
    'blockers', 'reproducible')
foreach ($property in $requiredProperties) {
    if ($result.PSObject.Properties.Name -notcontains $property) {
        $errors.Add("result missing property: $property")
    }
}

if ($result.schemaVersion -ne 1) { $errors.Add('result schemaVersion must be 1') }
if ($result.runId -ne $run.runId) { $errors.Add('result runId does not match run.json') }
if ($result.baselines.host -ne $run.host.baseline) {
    $errors.Add('reported host baseline does not match run.json')
}
if ($result.baselines.subscriber -ne $run.subscriber.baseline) {
    $errors.Add('reported subscriber baseline does not match run.json')
}

function Get-RepositoryAudit {
    param(
        [string]$Path,
        [string]$Baseline,
        [string]$Label
    )

    $head = (& git -C $Path rev-parse HEAD).Trim()
    if ($head -ne $Baseline) {
        $script:errors.Add("$Label contains commits; expected detached baseline $Baseline")
    }
    $tracked = @(& git -C $Path diff --name-only)
    $untracked = @(& git -C $Path ls-files --others --exclude-standard)
    $names = @($tracked + $untracked | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Sort-Object -Unique)
    return [ordered]@{ head = $head; changedFiles = $names }
}

$hostAudit = Get-RepositoryAudit -Path $run.host.worktree `
    -Baseline $run.host.baseline -Label 'host worktree'
$subscriberAudit = Get-RepositoryAudit -Path $run.subscriber.worktree `
    -Baseline $run.subscriber.baseline -Label 'subscriber worktree'

if ((Compare-Object @($result.changedFiles.host) @($hostAudit.changedFiles))) {
    $errors.Add('reported host changedFiles does not match Git')
}
if ((Compare-Object @($result.changedFiles.subscriber) @($subscriberAudit.changedFiles))) {
    $errors.Add('reported subscriber changedFiles does not match Git')
}

$allowedGateStates = @('passed', 'failed', 'blocked', 'not_run')
$requiredGates = @(
    'baseline', 'subscriberApi', 'subscriberHostAbsence', 'hostModel',
    'dynamicSwf', 'genericCommands', 'transactionTests', 'hostBuild',
    'subscriberBuild', 'inGame', 'privacy')
$passedGates = 0
foreach ($gate in $requiredGates) {
    if ($result.gates.PSObject.Properties.Name -notcontains $gate) {
        $errors.Add("result missing gate: $gate")
        continue
    }
    $state = $result.gates.$gate
    if ($allowedGateStates -notcontains $state) {
        $errors.Add("invalid gate state for ${gate}: $state")
    } elseif ($state -eq 'passed') {
        $passedGates++
    }
}

function Read-SourceText {
    param([string]$Root, [string]$RelativePath)

    $path = Join-Path $Root $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return '' }
    return Get-Content -Raw -LiteralPath $path
}

$native = Read-SourceText $run.host.worktree 'src\NativeMenuProbe.cpp'
$actionScript = Read-SourceText $run.host.worktree `
    'interface\src\AbsoluteControlPanelMenu.as'
$registryHeader = Read-SourceText $run.host.worktree 'include\MenuApiHost.h'
$subscriberSources = @($subscriberAudit.changedFiles | Where-Object {
        $_ -match '\.(cpp|cxx|cc|h|hpp)$'
    } | ForEach-Object { Read-SourceText $run.subscriber.worktree $_ }) -join "`n"

if ($result.gates.hostModel -eq 'passed' -and
    ($native -notmatch 'applyModel' -or $registryHeader -notmatch 'Pages|Snapshot')) {
    $errors.Add('hostModel reported passed but generic model/enumeration markers are absent')
}
if ($result.gates.dynamicSwf -eq 'passed' -and $actionScript -notmatch 'applyModel') {
    $errors.Add('dynamicSwf reported passed but ActionScript has no applyModel entry point')
}
if (($result.gates.dynamicSwf -eq 'passed' -or
     $result.gates.genericCommands -eq 'passed') -and
    (($native + $actionScript) -match
        'toggleFeature|incrementLevel|decrementLevel|beginBindingCapture')) {
    $errors.Add('generic host gates reported passed while fixed synthetic commands remain')
}
if ($result.gates.subscriberApi -eq 'passed' -and
    ($subscriberSources -notmatch 'SLOP_QueryApi' -or
     $subscriberSources -notmatch 'GetProcAddress' -or
     $subscriberSources -notmatch 'registerPage')) {
    $errors.Add('subscriberApi reported passed but runtime registration markers are absent')
}

$privacyPatterns = @(
    '(?i)c:\\users\\', '(?i)\\onedrive\\', '(?i)[a-z]:\\modlists\\',
    '(?i)sfse_loader\.lnk', '(?i)gh[pousr]_[A-Za-z0-9]{20,}',
    '(?i)-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----')
foreach ($entry in @(
        @{ root = $run.host.worktree; files = $hostAudit.changedFiles },
        @{ root = $run.subscriber.worktree; files = $subscriberAudit.changedFiles })) {
    foreach ($relative in $entry.files) {
        if ($relative -match '\.(dll|pdb|lib|exp|ilk|swf|png|jpg|jpeg|bin)$') {
            continue
        }
        $text = Read-SourceText $entry.root $relative
        foreach ($pattern in $privacyPatterns) {
            if ($text -match $pattern) {
                $errors.Add("privacy pattern found in changed file: $relative")
                break
            }
        }
    }
}

$interventionPenalty = 0
foreach ($intervention in @($result.interventions)) {
    if ($intervention.category -in @('scope', 'design', 'user')) {
        $interventionPenalty += 4
    } else {
        $interventionPenalty += 1
    }
}
$completionScore = [Math]::Round(70.0 * $passedGates / $requiredGates.Count)
$autonomyScore = [Math]::Max(0, 20 - $interventionPenalty)
$reportingScore = if ($errors.Count -eq 0) { 10 } else { 0 }
$score = $completionScore + $autonomyScore + $reportingScore

$evaluation = [ordered]@{
    schemaVersion = 1
    runId = $run.runId
    score = $score
    completion = $completionScore
    autonomy = $autonomyScore
    reporting = $reportingScore
    passedGates = $passedGates
    totalGates = $requiredGates.Count
    interventionCount = @($result.interventions).Count
    valid = $errors.Count -eq 0
    disposition = 'phase_decision_required'
    dispositionReason = 'Promote independently accepted SLOP product phases; discard rejected or unverified candidate work.'
    errors = @($errors)
    notes = @($notes)
    host = $hostAudit
    subscriber = $subscriberAudit
}
[System.IO.File]::WriteAllText(
    (Join-Path $resolvedRun 'evaluation.json'),
    ($evaluation | ConvertTo-Json -Depth 8),
    [System.Text.UTF8Encoding]::new($false))

$evaluation | ConvertTo-Json -Depth 8
if ($errors.Count -ne 0) { exit 1 }
