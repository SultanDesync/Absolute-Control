[CmdletBinding()]
param(
    [string]$Version,
    [string]$ArtifactManifest
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-PackageSha256 {
    param([Parameter(Mandatory = $true)][string]$LiteralPath)
    $stream = [IO.File]::OpenRead($LiteralPath)
    $digest = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($digest.ComputeHash($stream))).Replace('-', '')
    } finally {
        $digest.Dispose()
        $stream.Dispose()
    }
}

$repoRoot = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path
if ([string]::IsNullOrWhiteSpace($ArtifactManifest)) {
    $ArtifactManifest = Join-Path $repoRoot 'build\artifact-manifests\AbsoluteControlPanel.artifacts.json'
}
Import-Module (Join-Path $repoRoot 'tools\build-artifacts\ArtifactManifest.psm1') -Force
$validated = Test-AcpBuildArtifactManifest `
    -RepositoryRoot $repoRoot `
    -ManifestPath $ArtifactManifest `
    -ExpectedConfiguration 'release' `
    -ExpectedArtifactRole 'release' `
    -PassThru
if (-not [bool]$validated.Manifest.product.packageable) {
    throw 'Compatibility packages may only consume a release manifest marked packageable.'
}

$manifestVersion = [string]$validated.Manifest.product.version
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $manifestVersion
} elseif ($Version -cne $manifestVersion) {
    throw "Requested package version '$Version' does not match manifest product version '$manifestVersion'."
}

$dll = $validated.PluginPath
$swf = $validated.MoviePath
$config = Join-Path $repoRoot 'config\AbsoluteControlPanel.ini'
$readme = Join-Path $repoRoot 'tools\build-artifacts\compatibility-test-README.txt'
foreach ($requiredFile in @($dll, $swf, $config, $readme)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required package input is missing: $requiredFile"
    }
}

$packageRoot = Join-Path $repoRoot 'artifacts\packages'
$stageRoot = Join-Path $packageRoot "stage-$Version"
$archive = Join-Path $packageRoot "Absolute-Control-Panel-PauseMenu-Compatibility-Test-$Version.zip"

if (Test-Path -LiteralPath $stageRoot) {
    $resolvedStage = (Resolve-Path -LiteralPath $stageRoot).Path
    $resolvedPackages = [IO.Path]::GetFullPath($packageRoot).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar)
    if (-not $resolvedStage.StartsWith(
            $resolvedPackages + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean staging path outside the package directory: $resolvedStage"
    }
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}

New-Item -ItemType Directory -Path (Join-Path $stageRoot 'SFSE\Plugins') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stageRoot 'Interface') -Force | Out-Null
Copy-Item -LiteralPath $dll -Destination (Join-Path $stageRoot 'SFSE\Plugins\AbsoluteControlPanel.dll')
Copy-Item -LiteralPath $config -Destination (Join-Path $stageRoot 'SFSE\Plugins\AbsoluteControlPanel.ini')
Copy-Item -LiteralPath $swf -Destination (Join-Path $stageRoot 'Interface\AbsoluteControlPanelMenu.swf')
Copy-Item -LiteralPath $readme -Destination (Join-Path $stageRoot 'README.txt')

& (Join-Path $repoRoot 'tools\build-artifacts\Test-PackageContent.ps1') `
    -ArtifactManifest $ArtifactManifest `
    -StageRoot $stageRoot

if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}
Compress-Archive -Path (Join-Path $stageRoot '*') -DestinationPath $archive -CompressionLevel Optimal
& (Join-Path $repoRoot 'tools\build-artifacts\Test-PackageContent.ps1') `
    -ArtifactManifest $ArtifactManifest `
    -ArchivePath $archive
[pscustomobject]@{
    Algorithm = 'SHA256'
    Hash = Get-PackageSha256 -LiteralPath $archive
    Path = $archive
}
