[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ModPath,

    [string]$RunId = 'manual',
    [uint32]$OpenDelayMilliseconds = 15000,
    [uint32]$ArmTimeoutMilliseconds = 180000,
    [uint32]$VisibleMilliseconds = 12000,
    [uint32]$MenuFlags = 0x08000713,
    [switch]$RequireArm,
    [switch]$AdvanceTitleWithSendInput,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not (Test-Path -LiteralPath $ModPath -PathType Container)) {
    throw "Mod directory does not exist: $ModPath"
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build-interface.ps1')
    if ($LASTEXITCODE -ne 0) {
        throw 'Interface build failed.'
    }
    Push-Location $repositoryRoot
    try {
        xmake
        if ($LASTEXITCODE -ne 0) {
            throw "Native plugin build failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }
}

$pluginSource = Join-Path $repositoryRoot 'build\windows\x64\releasedbg\AbsoluteControlPanelResearch.dll'
$movieSource = Join-Path $repositoryRoot 'interface\dist\AbsoluteControlPanelMenu.swf'
$pluginDirectory = Join-Path $ModPath 'SFSE\Plugins'
$interfaceDirectory = Join-Path $ModPath 'Interface'
$configPath = Join-Path $pluginDirectory 'AbsoluteControlPanelResearch.ini'

foreach ($requiredFile in @($pluginSource, $movieSource)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required build output is missing: $requiredFile"
    }
}

New-Item -ItemType Directory -Force -Path $pluginDirectory, $interfaceDirectory | Out-Null
Copy-Item -LiteralPath $pluginSource -Destination $pluginDirectory -Force
Copy-Item -LiteralPath $movieSource -Destination $interfaceDirectory -Force

$configLines = @(
    '[Probe]',
    "RunId=$RunId",
    'EnableRegistration=true',
    'AutoOpen=true',
    "RequireArm=$($RequireArm.IsPresent.ToString().ToLowerInvariant())",
    "AdvanceTitleWithSendInput=$($AdvanceTitleWithSendInput.IsPresent.ToString().ToLowerInvariant())",
    "ArmTimeoutMilliseconds=$ArmTimeoutMilliseconds",
    "OpenDelayMilliseconds=$OpenDelayMilliseconds",
    "VisibleMilliseconds=$VisibleMilliseconds",
    ('MenuFlags=0x{0:X8}' -f $MenuFlags)
)
[System.IO.File]::WriteAllLines(
    $configPath, $configLines, [System.Text.UTF8Encoding]::new($false))

Get-FileHash -Algorithm SHA256 -LiteralPath (
    Join-Path $pluginDirectory 'AbsoluteControlPanelResearch.dll'), (
    Join-Path $interfaceDirectory 'AbsoluteControlPanelMenu.swf')
