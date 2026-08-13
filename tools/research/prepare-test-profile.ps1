[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ManifestPath,

    [switch]$CreateTestMod,
    [switch]$EnableRequiredMods
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedManifest = (Resolve-Path -LiteralPath $ManifestPath).Path
$manifest = Get-Content -Raw -LiteralPath $resolvedManifest | ConvertFrom-Json
foreach ($property in @('modPath', 'profileModListPath')) {
    if ([string]::IsNullOrWhiteSpace([string]$manifest.$property)) {
        throw "Manifest is missing $property."
    }
}

$modPath = [System.IO.Path]::GetFullPath([string]$manifest.modPath)
$profileModListPath = [System.IO.Path]::GetFullPath([string]$manifest.profileModListPath)
$modsRoot = Split-Path -Parent $modPath
$testModName = Split-Path -Leaf $modPath
if ([string]::IsNullOrWhiteSpace($testModName) -or $testModName -in @('.', '..')) {
    throw 'The test mod path must name one specific directory beneath the MO2 mods directory.'
}

if (-not (Test-Path -LiteralPath $modsRoot -PathType Container)) {
    throw "MO2 mods directory does not exist: $modsRoot"
}
if (-not (Test-Path -LiteralPath $profileModListPath -PathType Leaf)) {
    throw "MO2 profile modlist does not exist: $profileModListPath"
}

if (-not (Test-Path -LiteralPath $modPath -PathType Container)) {
    if (-not $CreateTestMod) {
        throw "SLOP test mod does not exist: $modPath. Re-run with -CreateTestMod."
    }
    New-Item -ItemType Directory -Path $modPath | Out-Null
}

$requiredMods = @($testModName)
if ($manifest.PSObject.Properties.Name -contains 'requiredProfileMods') {
    $requiredMods = @($manifest.requiredProfileMods | ForEach-Object { [string]$_ })
    if ($requiredMods -notcontains $testModName) {
        $requiredMods += $testModName
    }
} else {
    $requiredMods = @('SFSE', 'Address Library for SFSE Plugins', $testModName)
}
$requiredMods = @($requiredMods | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_)
    } | Select-Object -Unique)

foreach ($name in $requiredMods) {
    if ($name.IndexOfAny([System.IO.Path]::GetInvalidFileNameChars()) -ge 0 -or
        $name.Contains('\') -or $name.Contains('/')) {
        throw "Invalid MO2 mod name in requiredProfileMods: $name"
    }
    $directory = Join-Path $modsRoot $name
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        throw "Required MO2 mod is not installed beneath the configured mods directory: $name"
    }
}

$lines = [System.Collections.Generic.List[string]]::new()
foreach ($line in [System.IO.File]::ReadAllLines($profileModListPath)) {
    $lines.Add($line)
}

$changed = [System.Collections.Generic.List[string]]::new()
$missing = [System.Collections.Generic.List[string]]::new()
foreach ($name in $requiredMods) {
    $enabledLine = "+$name"
    $disabledLine = "-$name"
    if ($lines.IndexOf($enabledLine) -ge 0) {
        continue
    }
    $disabledIndex = $lines.IndexOf($disabledLine)
    if (-not $EnableRequiredMods) {
        $missing.Add($name)
        continue
    }
    if ($disabledIndex -ge 0) {
        $lines[$disabledIndex] = $enabledLine
    } else {
        $lines.Add($enabledLine)
    }
    $changed.Add($name)
}

if ($missing.Count -gt 0) {
    throw "Required MO2 mods are not enabled: $($missing -join ', '). Re-run with -EnableRequiredMods while MO2 is closed."
}

if ($changed.Count -gt 0) {
    $runningMo2 = Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -in @('ModOrganizer', 'ModOrganizer2')
    } | Select-Object -First 1
    if ($runningMo2) {
        throw 'Mod Organizer is running. Close it before the harness changes the named test profile.'
    }

    $timestamp = [DateTimeOffset]::UtcNow.ToString('yyyyMMdd-HHmmss')
    $backup = "$profileModListPath.slop-backup-$timestamp"
    Copy-Item -LiteralPath $profileModListPath -Destination $backup
    $temporary = "$profileModListPath.slop-new-$timestamp"
    try {
        [System.IO.File]::WriteAllLines(
            $temporary, $lines, [System.Text.UTF8Encoding]::new($false))
        Move-Item -LiteralPath $temporary -Destination $profileModListPath -Force
    } finally {
        if (Test-Path -LiteralPath $temporary) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }
}

[pscustomobject][ordered]@{
    modPath = $modPath
    profileModListPath = $profileModListPath
    requiredMods = $requiredMods
    enabledByHarness = @($changed)
    ready = $true
}
