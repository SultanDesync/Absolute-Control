[CmdletBinding()]
param(
    [string]$ManifestPath,
    [string]$ExpectedConfiguration,
    [ValidateSet('release', 'research-dev')][string]$ExpectedArtifactRole = 'release',
    [switch]$PassThru
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $manifestBasename = if ($ExpectedArtifactRole -eq 'release') {
        'AbsoluteControlPanel.artifacts.json'
    } else {
        'AbsoluteControlPanelResearchDev.artifacts.json'
    }
    $ManifestPath = Join-Path $repositoryRoot "build\artifact-manifests\$manifestBasename"
}
Import-Module (Join-Path $PSScriptRoot 'ArtifactManifest.psm1') -Force
$arguments = @{
    RepositoryRoot = $repositoryRoot
    ManifestPath = $ManifestPath
    ExpectedArtifactRole = $ExpectedArtifactRole
    PassThru = $PassThru
}
if (-not [string]::IsNullOrWhiteSpace($ExpectedConfiguration)) {
    $arguments.ExpectedConfiguration = $ExpectedConfiguration
}
Test-AcpBuildArtifactManifest @arguments
