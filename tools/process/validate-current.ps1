[CmdletBinding()]
param(
    [string]$Repository,
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($Repository)) {
    $Repository = Join-Path $PSScriptRoot '..\..'
}
$repositoryRoot = (Resolve-Path -LiteralPath $Repository).Path
$definitionPath = Join-Path $PSScriptRoot 'current-process.json'
$definition = Get-Content -Raw -LiteralPath $definitionPath | ConvertFrom-Json
if ($definition.status -cne 'current') {
    throw 'The selected process definition is not current.'
}

if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $reportDirectory = Join-Path $repositoryRoot 'artifacts\process'
    New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
    $ReportPath = Join-Path $reportDirectory 'current-validation.json'
} else {
    $reportParent = Split-Path -Parent ([IO.Path]::GetFullPath($ReportPath))
    if (-not (Test-Path -LiteralPath $reportParent -PathType Container)) {
        New-Item -ItemType Directory -Path $reportParent -Force | Out-Null
    }
}

$results = [System.Collections.Generic.List[object]]::new()

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable exited with code $LASTEXITCODE."
    }
}

function Invoke-CheckedScript {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [object[]]$Arguments = @()
    )

    $childArguments = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $Path) + @($Arguments)
    Invoke-CheckedCommand 'powershell.exe' $childArguments
}

function Invoke-ProcessGate {
    param([Parameter(Mandatory = $true)][object]$Gate)

    $started = [DateTimeOffset]::UtcNow
    $errorText = ''
    $status = 'passed'
    Write-Host "==> $($Gate.id): $($Gate.description)"
    $dependencies = if ($Gate.PSObject.Properties.Name -contains 'dependsOn') {
        @($Gate.dependsOn)
    } else {
        @()
    }
    $blockedBy = @($dependencies | Where-Object {
            $dependencyId = [string]$_
            $dependency = @($results | Where-Object { $_.id -eq $dependencyId })
            $dependency.Count -ne 1 -or $dependency[0].status -ne 'passed'
        })
    if ($blockedBy.Count -ne 0) {
        $status = 'blocked'
        $errorText = "Prerequisite gates did not pass: $($blockedBy -join ', ')"
        $results.Add([ordered]@{
                id = [string]$Gate.id
                status = $status
                durationMilliseconds = 0
                error = $errorText
            })
        Write-Warning "$($Gate.id) blocked: $errorText"
        return
    }
    try {
        Push-Location $repositoryRoot
        try {
            switch ([string]$Gate.runner) {
                'process-definition' {
                    Invoke-CheckedScript (Join-Path $PSScriptRoot 'tests\Test-CurrentProcess.ps1')
                }
                'native-build' {
                    Invoke-CheckedCommand 'xmake' @('f', '-y', '-m', 'release')
                    Invoke-CheckedCommand 'xmake' @('-y')
                }
                'native-tests' {
                    Invoke-CheckedCommand 'xmake' @('test')
                }
                'sdk-generator-tests' {
                    Invoke-CheckedCommand 'python' @('-m', 'unittest', 'discover', '-s', 'sdk/tests', '-v')
                }
                'sdk-codegen-check' {
                    Invoke-CheckedCommand 'python' @(
                        'sdk/tools/menu_codegen.py', 'generate',
                        'sdk/examples/absolute-head-tracking.menu.json',
                        'sdk/examples/generated/AbsoluteHeadTrackingMenu.generated.h', '--check')
                }
                'sdk-compile-fixture' {
                    Invoke-CheckedCommand 'xmake' @('f', '-P', 'sdk/tests/compile', '-y', '-m', 'release')
                    Invoke-CheckedCommand 'xmake' @('-P', 'sdk/tests/compile', '-y')
                    Invoke-CheckedCommand 'xmake' @('run', '-P', 'sdk/tests/compile', 'generated_menu_compile_test')
                }
                'component-catalog' {
                    Invoke-CheckedCommand 'python' @('catalog/validate_catalog.py')
                }
                'scaleform-provenance' {
                    Invoke-CheckedScript (Join-Path $repositoryRoot 'tools\build-artifacts\Test-InterfaceProvenance.ps1')
                }
                'artifact-tooling' {
                    Invoke-CheckedScript (Join-Path $repositoryRoot 'tools\build-artifacts\tests\Test-ArtifactTooling.ps1')
                }
                'canonical-role-manifest' {
                    Invoke-CheckedScript `
                        (Join-Path $repositoryRoot 'tools\build-artifacts\New-BuildArtifactManifest.ps1') `
                        -Arguments @('-Configuration', 'release', '-ArtifactRole', 'release')
                    Invoke-CheckedScript `
                        (Join-Path $repositoryRoot 'tools\build-artifacts\Test-BuildArtifactManifest.ps1') `
                        -Arguments @(
                            '-ExpectedConfiguration', 'release',
                            '-ExpectedArtifactRole', 'release')
                }
                'package-validation' {
                    Invoke-CheckedScript (Join-Path $repositoryRoot 'tools\package-compatibility-test.ps1')
                }
                default {
                    throw "Unknown current-process runner: $($Gate.runner)"
                }
            }
        } finally {
            Pop-Location
        }
    } catch {
        $status = 'failed'
        $errorText = $_.Exception.Message
        Write-Error -ErrorAction Continue "$($Gate.id) failed: $errorText"
    }
    $finished = [DateTimeOffset]::UtcNow
    $results.Add([ordered]@{
            id = [string]$Gate.id
            status = $status
            durationMilliseconds = [Math]::Round(($finished - $started).TotalMilliseconds)
            error = $errorText
        })
}

foreach ($gate in @($definition.automatedGates)) {
    Invoke-ProcessGate $gate
}

$incomplete = @($results | Where-Object { $_.status -ne 'passed' })
$manual = @($definition.manualGates | ForEach-Object {
        [ordered]@{
            id = [string]$_.id
            status = 'not_run'
            requiresHumanObservation = $true
            description = [string]$_.description
        }
    })
$report = [ordered]@{
    schemaVersion = 1
    processVersion = [int]$definition.processVersion
    generatedUtc = [DateTimeOffset]::UtcNow.ToString('o')
    automatedStatus = if ($incomplete.Count -eq 0) { 'passed' } else { 'failed' }
    runtimeStatus = 'not_run'
    uxStatus = 'not_run'
    releaseReady = $false
    claims = $definition.claims
    automatedGates = @($results)
    manualGates = $manual
}
[System.IO.File]::WriteAllText(
    [IO.Path]::GetFullPath($ReportPath),
    (($report | ConvertTo-Json -Depth 8) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false))

Write-Host "Current-process report: $([IO.Path]::GetFullPath($ReportPath))"
Write-Host "Automated status: $($report.automatedStatus)"
Write-Host 'Runtime/UX status: not_run (manual validation remains required).'
if ($incomplete.Count -ne 0) { exit 1 }
