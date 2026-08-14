[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$processRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repositoryRoot = (Resolve-Path (Join-Path $processRoot '..\..')).Path
$definitionPath = Join-Path $processRoot 'current-process.json'
$definition = Get-Content -Raw -LiteralPath $definitionPath | ConvertFrom-Json

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

Assert-True ($definition.schemaVersion -eq 1) 'Current process schemaVersion must be 1.'
Assert-True ($definition.processVersion -eq 2) 'Current processVersion must be 2.'
Assert-True ($definition.status -ceq 'current') 'Current process must identify itself as current.'
Assert-True (-not [bool]$definition.claims.releaseReady) 'Automation must not claim release readiness.'
Assert-True (-not [bool]$definition.claims.runtimeOrUxVerifiedByAutomation) `
    'Automation must not claim runtime or UX verification.'
foreach ($property in @('launchesGame', 'deploysMod', 'modifiesModOrganizer', 'usesMachineSpecificPaths')) {
    Assert-True (-not [bool]$definition.executionPolicy.$property) `
        "Current process execution policy must keep $property false."
}

$entryPoint = Join-Path $repositoryRoot ([string]$definition.entryPoint)
Assert-True (Test-Path -LiteralPath $entryPoint -PathType Leaf) `
    'Current process entry point is missing.'

$allowedRunners = @(
    'process-definition', 'native-build', 'native-tests', 'sdk-generator-tests',
    'sdk-codegen-check', 'sdk-compile-fixture', 'component-catalog',
    'scaleform-provenance', 'artifact-tooling', 'canonical-role-manifest',
    'package-validation')
$automatedIds = @($definition.automatedGates | ForEach-Object { [string]$_.id })
$manualIds = @($definition.manualGates | ForEach-Object { [string]$_.id })
Assert-True ($automatedIds.Count -eq (@($automatedIds | Sort-Object -Unique)).Count) `
    'Automated gate IDs must be unique.'
Assert-True ($manualIds.Count -eq (@($manualIds | Sort-Object -Unique)).Count) `
    'Manual gate IDs must be unique.'
foreach ($gate in @($definition.automatedGates)) {
    Assert-True ($allowedRunners -contains [string]$gate.runner) `
        "Unknown automated runner: $($gate.runner)"
    if ($gate.PSObject.Properties.Name -contains 'dependsOn') {
        foreach ($dependency in @($gate.dependsOn)) {
            Assert-True ($automatedIds -contains [string]$dependency) `
                "Automated gate $($gate.id) has unknown dependency $dependency."
            $gateIndex = [Array]::IndexOf($automatedIds, [string]$gate.id)
            $dependencyIndex = [Array]::IndexOf($automatedIds, [string]$dependency)
            Assert-True ($dependencyIndex -lt $gateIndex) `
                "Automated gate $($gate.id) depends on a gate that does not run first: $dependency"
        }
    }
}
foreach ($gate in @($definition.manualGates)) {
    Assert-True ($gate.status -ceq 'not_run') `
        "Manual gate $($gate.id) must default to not_run."
    Assert-True ([bool]$gate.requiresHumanObservation) `
        "Manual gate $($gate.id) must require human observation."
}

$trackedDefinitions = @(
    (Join-Path $processRoot 'README.md'),
    (Join-Path $processRoot 'current-process.json'),
    (Join-Path $processRoot 'validate-current.ps1'),
    (Join-Path $processRoot 'legacy\v1\README.md'),
    (Join-Path $processRoot 'legacy\v1\contract.json'))
$definitionText = @($trackedDefinitions | ForEach-Object {
        Assert-True (Test-Path -LiteralPath $_ -PathType Leaf) "Process definition is missing: $_"
        Get-Content -Raw -LiteralPath $_
    }) -join "`n"
foreach ($pattern in @(
        '(?i)c:\\users\\', '(?i)\\onedrive\\', '(?i)[a-z]:\\modlists\\',
        '(?i)sfse_loader\.lnk', '(?i)gate to stars launch\.lnk')) {
    Assert-True ($definitionText -notmatch $pattern) `
        "Machine-specific path pattern appears in tracked process definitions: $pattern"
}

$currentDefinitionText = Get-Content -Raw -LiteralPath $definitionPath
Assert-True ($currentDefinitionText -notmatch '(?i)magenta framebuffer presence sentinel') `
    'The current process contract must not contain the archived magenta sentinel criterion.'
Assert-True ($currentDefinitionText -notmatch '(?i)run-probe|deploy-probe|start-process|sfse_loader') `
    'The current process contract must not launch or deploy through research tooling.'

$legacyScripts = @(
    'new-builder-run.ps1', 'new-phase-prompt.ps1', 'evaluate-phase.ps1',
    'evaluate-builder-run.ps1', 'discard-builder-run.ps1')
foreach ($name in $legacyScripts) {
    $source = Get-Content -Raw -LiteralPath (Join-Path $processRoot $name)
    Assert-True ($source -match 'AllowLegacyV1') "$name does not expose the legacy opt-in."
    Assert-True ($source -match 'Assert-LegacyV1OptIn') "$name does not fail closed through the legacy guard."
}

Write-Host 'Current process definition tests passed.'
