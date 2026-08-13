[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ModPath,

    [string]$RunId = 'manual',
    [uint32]$OpenDelayMilliseconds = 15000,
    [uint32]$ArmTimeoutMilliseconds = 180000,
    [uint32]$VisibleMilliseconds = 12000,
    [uint32]$MenuFlags = 0x08000713,
    [string[]]$AdditionalPluginFiles = @(),
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

$resolvedAdditionalPlugins = @($AdditionalPluginFiles | ForEach-Object {
    $resolved = (Resolve-Path -LiteralPath $_).Path
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf) -or
        [System.IO.Path]::GetExtension($resolved) -ine '.dll') {
        throw "Additional plugin must be an existing DLL: $_"
    }
    $resolved
})
$destinationNames = @('AbsoluteControlPanelResearch.dll') +
    @($resolvedAdditionalPlugins | ForEach-Object { Split-Path -Leaf $_ })
if (@($destinationNames | Select-Object -Unique).Count -ne $destinationNames.Count) {
    throw 'Additional plugin filenames must be unique and cannot replace the SLOP host DLL.'
}

foreach ($requiredFile in @($pluginSource, $movieSource) + $resolvedAdditionalPlugins) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required build output is missing: $requiredFile"
    }
}

New-Item -ItemType Directory -Force -Path $pluginDirectory, $interfaceDirectory | Out-Null
Copy-Item -LiteralPath $pluginSource -Destination $pluginDirectory -Force
Copy-Item -LiteralPath $movieSource -Destination $interfaceDirectory -Force
foreach ($additionalPlugin in $resolvedAdditionalPlugins) {
    Copy-Item -LiteralPath $additionalPlugin -Destination $pluginDirectory -Force
}

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

$deployedFiles = @(
    Join-Path $pluginDirectory 'AbsoluteControlPanelResearch.dll'
    Join-Path $interfaceDirectory 'AbsoluteControlPanelMenu.swf'
) + @($resolvedAdditionalPlugins | ForEach-Object {
    Join-Path $pluginDirectory (Split-Path -Leaf $_)
})
Get-FileHash -Algorithm SHA256 -LiteralPath $deployedFiles
