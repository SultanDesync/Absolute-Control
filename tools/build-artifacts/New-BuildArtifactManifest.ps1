[CmdletBinding()]
param(
    [ValidateSet('debug', 'release', 'releasedbg')]
    [string]$Configuration = 'releasedbg',

    [string]$ManifestPath,
    [string]$Version,
    [string[]]$RuntimeVersions,

    [ValidateSet('release', 'research-dev')]
    [string]$ArtifactRole = 'release'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $manifestBasename = if ($ArtifactRole -eq 'release') {
        'AbsoluteControlPanel.artifacts.json'
    } else {
        'AbsoluteControlPanelResearchDev.artifacts.json'
    }
    $ManifestPath = Join-Path $repositoryRoot "build\artifact-manifests\$manifestBasename"
}
Import-Module (Join-Path $PSScriptRoot 'ArtifactManifest.psm1') -Force
$validated = New-AcpBuildArtifactManifest `
    -RepositoryRoot $repositoryRoot `
    -Configuration $Configuration `
    -ManifestPath $ManifestPath `
    -Version $Version `
    -RuntimeVersions $RuntimeVersions `
    -ArtifactRole $ArtifactRole

Write-Host "Wrote canonical artifact manifest: $([IO.Path]::GetFullPath($ManifestPath))"
Write-Host "Artifact role: $ArtifactRole (packageable=$($validated.Manifest.product.packageable))"
Write-Host "Plugin SHA-256: $($validated.Manifest.artifacts.plugin.sha256)"
Write-Host "Interface SHA-256: $($validated.Manifest.artifacts.interface.sha256)"
