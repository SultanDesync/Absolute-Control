[CmdletBinding(DefaultParameterSetName = 'Stage')]
param(
    [Parameter(Mandatory = $true)]
    [string]$ArtifactManifest,

    [string]$RepositoryRoot,

    [Parameter(Mandatory = $true, ParameterSetName = 'Stage')]
    [string]$StageRoot,

    [Parameter(Mandatory = $true, ParameterSetName = 'Archive')]
    [string]$ArchivePath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Join-Path $PSScriptRoot '..\..'
}
$repositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
Import-Module (Join-Path $PSScriptRoot 'ArtifactManifest.psm1') -Force
$validated = Test-AcpBuildArtifactManifest `
    -RepositoryRoot $repositoryRoot `
    -ManifestPath $ArtifactManifest `
    -ExpectedArtifactRole 'release' `
    -PassThru
if (-not [bool]$validated.Manifest.product.packageable) {
    throw 'Package validation refuses manifests marked non-packageable.'
}

$expected = [ordered]@{
    'Interface/AbsoluteControlPanelMenu.swf' = [string]$validated.Manifest.artifacts.interface.sha256
    'README.txt' = $null
    'SFSE/Plugins/AbsoluteControlPanel.dll' = [string]$validated.Manifest.artifacts.plugin.sha256
    'SFSE/Plugins/AbsoluteControlPanel.ini' = $null
}

function Get-StreamSha256 {
    param([Parameter(Mandatory = $true)][IO.Stream]$Stream)
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($algorithm.ComputeHash($Stream))).Replace('-', '')
    } finally {
        $algorithm.Dispose()
    }
}

$observed = @{}
if ($PSCmdlet.ParameterSetName -eq 'Stage') {
    $root = (Resolve-Path -LiteralPath $StageRoot).Path
    $rootPrefix = $root.TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    foreach ($file in Get-ChildItem -LiteralPath $root -File -Recurse) {
        if (-not $file.FullName.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Package file escaped the staging root: $($file.FullName)"
        }
        $relative = $file.FullName.Substring($rootPrefix.Length).Replace('\', '/')
        $observed[$relative] = [pscustomobject]@{
            Size = $file.Length
            Hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash
        }
    }
} else {
    if (-not (Test-Path -LiteralPath $ArchivePath -PathType Leaf)) {
        throw "Package archive is missing: $ArchivePath"
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $ArchivePath).Path)
    try {
        foreach ($entry in $archive.Entries) {
            if ([string]::IsNullOrEmpty($entry.Name)) { continue }
            $name = $entry.FullName.Replace('\', '/').TrimStart('/')
            $stream = $entry.Open()
            try {
                $observed[$name] = [pscustomobject]@{
                    Size = $entry.Length
                    Hash = Get-StreamSha256 -Stream $stream
                }
            } finally {
                $stream.Dispose()
            }
        }
    } finally {
        $archive.Dispose()
    }
}

$observedNames = @($observed.Keys | Sort-Object)
$expectedNames = @($expected.Keys | Sort-Object)
if (($observedNames -join "`n") -cne ($expectedNames -join "`n")) {
    throw "Package contents are not canonical. Expected [$($expectedNames -join ', ')]; observed [$($observedNames -join ', ')]."
}
foreach ($name in $expectedNames) {
    if ($observed[$name].Size -le 0) {
        throw "Package entry is empty: $name"
    }
    if ($null -ne $expected[$name] -and $observed[$name].Hash -ine $expected[$name]) {
        throw "Package entry hash does not match the canonical artifact manifest: $name"
    }
    if ($name -match 'AbsoluteControlPanelResearch') {
        throw "Package contains a retired legacy product filename: $name"
    }
}

Write-Host "Validated canonical package contents: $($expectedNames.Count) files."
