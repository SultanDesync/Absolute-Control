[CmdletBinding()]
param(
    # Defaults resolve below: Windows PowerShell 5.1 has no $PSScriptRoot in param defaults under -File.
    [string]$Suite = '',
    [string]$ConfigPath = '',
    # Enable the test mod and disable other Absolute Control hosts in the profile (MO2 must be closed).
    [switch]$PrepareProfile,
    # Check config, suite, profile and build output without launching anything.
    [switch]$DryRun,
    # Leave Starfield running after the suite instead of closing it.
    [switch]$KeepGameRunning,
    [string]$Mode = 'releasedbg'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $Suite) { $Suite = Join-Path $PSScriptRoot 'suites\menu-regression.json' }
if (-not $ConfigPath) { $ConfigPath = Join-Path $PSScriptRoot 'test.local.json' }
Import-Module (Join-Path $PSScriptRoot 'AbsoluteTest.psm1') -Force
Import-AbsoluteTestWindow

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$config = Read-AbsoluteTestConfig $ConfigPath
$suiteDefinition = Read-TestSuite $Suite
$runId = '{0}-{1:yyyyMMdd-HHmmss}' -f $suiteDefinition.name, (Get-Date)
$artifactRoot = if ($config.artifactRoot) { [string]$config.artifactRoot } else { Join-Path $repoRoot 'artifacts\test-runs' }
$runDirectory = Join-Path $artifactRoot $runId
New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null
$context = New-TestContext -Config $config -RunId $runId -RunDirectory $runDirectory
Write-RunnerEvent $context 'run_started' ("suite={0} tests={1} profile={2} dry_run={3}" -f
    $suiteDefinition.name, @($suiteDefinition.tests).Count, $config.profile, [bool]$DryRun)

# ---- preflight
$problems = New-Object System.Collections.Generic.List[string]
if (-not (Test-Path -LiteralPath $config.mo2Executable -PathType Leaf)) { $problems.Add("MO2 not found: $($config.mo2Executable)") }
if (Get-Process -Name ModOrganizer -ErrorAction SilentlyContinue) {
    $problems.Add('Mod Organizer is running. Close it; the runner launches the test profile itself.')
}
if (Get-Process -Name $config.processName -ErrorAction SilentlyContinue) { $problems.Add('Starfield is already running.') }
$stray = Join-Path $config.mo2BaseDirectory 'overwrite\SFSE\Plugins\AbsoluteControlPanel.ini'
if (Test-Path -LiteralPath $stray) { $problems.Add("A stray AbsoluteControlPanel.ini in MO2 overwrite would shadow the test config: $stray") }
$buildDll = Join-Path $repoRoot "build\test-hooks\$Mode\AbsoluteControlPanel.dll"
if (-not (Test-Path -LiteralPath $buildDll -PathType Leaf)) {
    $problems.Add("Test-hooks build missing: $buildDll. Run: xmake build -y AbsoluteControlPanelTestHooks")
}
$profileProblems = @(Test-ProfileReadiness $config)
if ($profileProblems.Count -gt 0 -and $PrepareProfile -and -not $DryRun -and
    -not (Get-Process -Name ModOrganizer -ErrorAction SilentlyContinue)) {
    $changes = Set-ProfileForTesting -Config $config -BackupDirectory $runDirectory
    Write-RunnerEvent $context 'profile_prepared' ($changes -join '; ')
    $profileProblems = @(Test-ProfileReadiness $config)
}
foreach ($problem in $profileProblems) {
    $hint = if ($PrepareProfile) { '' } else { ' (rerun with -PrepareProfile to fix)' }
    $problems.Add("$problem$hint")
}

if ($DryRun -or $problems.Count -gt 0) {
    Write-Host ''
    Write-Host "Suite:   $($suiteDefinition.name) ($(@($suiteDefinition.tests).Count) tests)"
    foreach ($test in @($suiteDefinition.tests)) {
        $repeat = if ($test.PSObject.Properties.Name -contains 'repeat') { $test.repeat } else { 1 }
        Write-Host ("  - {0} x{1}: {2}" -f $test.id, $repeat, $test.description)
    }
    Write-Host "Profile: $($config.profile)   Test mod: $($config.testModName)"
    Write-Host "Run dir: $runDirectory"
    if ($problems.Count -gt 0) {
        Write-Host ''
        Write-Host 'Preflight problems:' -ForegroundColor Red
        foreach ($problem in $problems) { Write-Host "  - $problem" -ForegroundColor Red }
        Write-RunnerEvent $context 'preflight_failed' ($problems -join ' | ')
        exit 2
    }
    Write-Host ''
    Write-Host 'Preflight passed. Rerun without -DryRun to launch the game.' -ForegroundColor Green
    exit 0
}

# ---- run
$deployed = Publish-TestHooksBuild -Config $config -RepoRoot $repoRoot -RunId $runId -Mode $Mode
Write-RunnerEvent $context 'deployed' ("dll={0}" -f $deployed['SFSE\Plugins\AbsoluteControlPanel.dll'])
$results = @()
$fatal = ''
$shutdown = 'not_started'
$dumps = @()
try {
    Start-GameSession $context
    $results = @(Invoke-TestSuite -Context $context -Suite $suiteDefinition)
} catch {
    $fatal = $_.Exception.Message
    Write-RunnerEvent $context 'run_failed' $fatal
    try { [void](Save-GameScreenshot -Context $context -Path (Join-Path $runDirectory 'run-failure.png')) } catch { }
} finally {
    $shutdown = Stop-GameSession -Context $context -KeepGameRunning:$KeepGameRunning
    $dumps = @(Save-RunArtifacts $context)
}
if ($results.Count -eq 0) {
    $results = @(@($suiteDefinition.tests) | ForEach-Object {
        [pscustomobject]@{ id = $_.id; status = 'not_run'; iterations = 0; seconds = 0; message = 'game session did not start' }
    })
}
$summary = Write-TestReport -Context $context -Suite $suiteDefinition -Results $results -Shutdown $shutdown `
    -Deployed $deployed -Dumps $dumps -FatalError $fatal
Write-Host ''
Write-Host ("{0}: passed {1}, failed {2}, not run {3}. Report: {4}" -f $suiteDefinition.name, $summary.passed,
    $summary.failed, $summary.notRun, (Join-Path $runDirectory 'report.md'))
if ($fatal -or $summary.failed -gt 0) { exit 1 }
exit 0
