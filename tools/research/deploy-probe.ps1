[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ModPath,

    [string]$RunId = 'manual',
    [uint32]$OpenDelayMilliseconds = 15000,
    [uint32]$ArmTimeoutMilliseconds = 180000,
    [uint32]$VisibleMilliseconds = 12000,
    [uint32]$OpenHotkey = 0x71,
    [uint32]$MenuFlags = 0x0800071B,
    [bool]$EnablePauseMenuEntry = $false,
    [string[]]$AdditionalPluginFiles = @(),
    [switch]$RequireArm,
    [switch]$AdvanceTitleWithSendInput,
    [switch]$SkipBuild,

    [ValidateSet('debug', 'release', 'releasedbg')]
    [string]$Configuration = 'releasedbg',

    [string]$ArtifactManifest
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($ArtifactManifest)) {
    $ArtifactManifest = Join-Path $repositoryRoot 'build\artifact-manifests\AbsoluteControlPanelResearchDev.artifacts.json'
}
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
        xmake f -y -m $Configuration
        if ($LASTEXITCODE -ne 0) {
            throw "Native plugin configuration failed with exit code $LASTEXITCODE"
        }
        xmake -y AbsoluteControlPanelResearchDev
        if ($LASTEXITCODE -ne 0) {
            throw "Native plugin build failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }
    & (Join-Path $repositoryRoot 'tools\build-artifacts\New-BuildArtifactManifest.ps1') `
        -Configuration $Configuration `
        -ManifestPath $ArtifactManifest `
        -ArtifactRole 'research-dev'
}

$artifactModule = Join-Path $repositoryRoot 'tools\build-artifacts\ArtifactManifest.psm1'
Import-Module $artifactModule -Force
$validatedArtifacts = Test-AcpBuildArtifactManifest `
    -RepositoryRoot $repositoryRoot `
    -ManifestPath $ArtifactManifest `
    -ExpectedConfiguration $Configuration `
    -ExpectedArtifactRole 'research-dev' `
    -PassThru
$pluginSource = $validatedArtifacts.PluginPath
$movieSource = $validatedArtifacts.MoviePath
$pluginDirectory = Join-Path $ModPath 'SFSE\Plugins'
$interfaceDirectory = Join-Path $ModPath 'Interface'
$configPath = Join-Path $pluginDirectory 'AbsoluteControlPanel.ini'

$resolvedAdditionalPlugins = @($AdditionalPluginFiles | ForEach-Object {
    $resolved = (Resolve-Path -LiteralPath $_).Path
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf) -or
        [System.IO.Path]::GetExtension($resolved) -ine '.dll') {
        throw "Additional plugin must be an existing DLL: $_"
    }
    $resolved
})
$destinationNames = @('AbsoluteControlPanelResearchDev.dll') +
    @($resolvedAdditionalPlugins | ForEach-Object { Split-Path -Leaf $_ })
if (@($destinationNames | Select-Object -Unique).Count -ne $destinationNames.Count) {
    throw 'Additional plugin filenames must be unique and cannot replace the Absolute Control Panel host DLL.'
}
$forbiddenAdditionalHosts = @($resolvedAdditionalPlugins | Where-Object {
        (Split-Path -Leaf $_) -iin @(
            'AbsoluteControlPanel.dll',
            'AbsoluteControlPanelResearch.dll',
            'AbsoluteControlPanelResearchDev.dll')
    })
if ($forbiddenAdditionalHosts.Count -gt 0) {
    throw "AdditionalPluginFiles cannot introduce another release, retired, or research host: $($forbiddenAdditionalHosts -join ', ')"
}

foreach ($requiredFile in @($pluginSource, $movieSource) + $resolvedAdditionalPlugins) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required build output is missing: $requiredFile"
    }
}

$conflictingHosts = @(@(
        (Join-Path $pluginDirectory 'AbsoluteControlPanel.dll'),
        (Join-Path $pluginDirectory 'AbsoluteControlPanelResearch.dll')
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
if ($conflictingHosts.Count -gt 0) {
    throw "Research deployment target contains a canonical or retired host DLL: $($conflictingHosts -join ', '). Remove the conflicting host explicitly; deploy-probe never mixes or deletes hosts."
}

New-Item -ItemType Directory -Force -Path $pluginDirectory, $interfaceDirectory | Out-Null
$deployedPlugin = Join-Path $pluginDirectory 'AbsoluteControlPanelResearchDev.dll'
$deployedMovie = Join-Path $interfaceDirectory 'AbsoluteControlPanelMenu.swf'
Copy-Item -LiteralPath $pluginSource -Destination $deployedPlugin -Force
Copy-Item -LiteralPath $movieSource -Destination $deployedMovie -Force
foreach ($additionalPlugin in $resolvedAdditionalPlugins) {
    Copy-Item -LiteralPath $additionalPlugin -Destination $pluginDirectory -Force
}

$configLines = @(
    '[ControlPanel]',
    "RunId=$RunId",
    'EnableRegistration=true',
    'AutoOpen=true',
    "EnablePauseMenuEntry=$($EnablePauseMenuEntry.ToString().ToLowerInvariant())",
    "RequireArm=$($RequireArm.IsPresent.ToString().ToLowerInvariant())",
    "AdvanceTitleWithSendInput=$($AdvanceTitleWithSendInput.IsPresent.ToString().ToLowerInvariant())",
    "ArmTimeoutMilliseconds=$ArmTimeoutMilliseconds",
    "OpenDelayMilliseconds=$OpenDelayMilliseconds",
    "VisibleMilliseconds=$VisibleMilliseconds",
    ('OpenHotkey=0x{0:X2}' -f $OpenHotkey),
    ('MenuFlags=0x{0:X8}' -f $MenuFlags)
)
[System.IO.File]::WriteAllLines(
    $configPath, $configLines, [System.Text.UTF8Encoding]::new($false))

$deployedFiles = @(
    $deployedPlugin
    $deployedMovie
) + @($resolvedAdditionalPlugins | ForEach-Object {
    Join-Path $pluginDirectory (Split-Path -Leaf $_)
})
$deployedPluginHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $deployedPlugin).Hash
$deployedMovieHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $deployedMovie).Hash
if ($deployedPluginHash -ine $validatedArtifacts.Manifest.artifacts.plugin.sha256 -or
    $deployedMovieHash -ine $validatedArtifacts.Manifest.artifacts.interface.sha256) {
    throw 'Deployment copy verification failed: deployed product hashes do not match the canonical artifact manifest.'
}
Get-FileHash -Algorithm SHA256 -LiteralPath $deployedFiles
