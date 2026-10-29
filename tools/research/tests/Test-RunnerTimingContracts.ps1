$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw "Assertion failed: $Message" }
}

$researchRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$runnerPath = Join-Path $researchRoot 'run-probe.ps1'
$inputPath = Join-Path $researchRoot 'invoke-runtime-input.ps1'

foreach ($path in @($runnerPath, $inputPath)) {
    $parseErrors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile(
        $path, [ref]$null, [ref]$parseErrors)
    Assert-True ($parseErrors.Count -eq 0) "PowerShell parser accepts $path"
}

$runner = Get-Content -Raw -LiteralPath $runnerPath
$input = Get-Content -Raw -LiteralPath $inputPath

Assert-True ($runner.Contains(
    "Invoke-ResearchInput -CommandId 3 -Command 'confirm_continue'")) `
    'runner uses the guarded Continue confirmation command'
Assert-True ($runner.Contains("'research_input_rejected'")) `
    'runner fails fast on native input rejection'
Assert-True ($runner.Contains('[int]$PollMilliseconds = 100')) `
    'generic polling defaults to 100 ms'
Assert-True (-not $runner.Contains('Start-Sleep -Milliseconds 1500')) `
    'runner has no fixed mailbox startup sleep'
Assert-True ($runner.Contains("-PollMilliseconds 250 -Condition")) `
    'pixel checks retain a bounded 250 ms capture cadence'
Assert-True ($runner.Contains("} else { 60 }")) `
    'pause diagnostics default to frame 60'
Assert-True ($runner.Contains("-DetailContains 'name=HUDMenu '")) `
    'Pause input remains gated by HUD admission'

Assert-True ($input.Contains("'confirm_continue', 'pause'")) `
    'standalone runtime input allow-list includes guarded Continue confirmation'
Assert-True ($input.Contains("'research_input_rejected'")) `
    'standalone runtime input fails fast on native rejection'
Assert-True ($input.Contains('.StartsWith("id=$CommandId ")')) `
    'standalone evidence matching requires an exact command-ID token'

Write-Host 'Research runner timing contracts passed.'
