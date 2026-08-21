[CmdletBinding()]
param(
    [string]$Version,
    [string]$OutputRoot,
    [switch]$AllowDirty
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$versionPath = Join-Path $repositoryRoot 'sdk\VERSION'
$declaredVersion = (Get-Content -Raw -LiteralPath $versionPath).Trim()
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $declaredVersion
}
if ($Version -notmatch '^[0-9A-Za-z][0-9A-Za-z.-]*$') {
    throw "Invalid SDK version: $Version"
}
if ($Version -cne $declaredVersion) {
    throw "Requested SDK version $Version differs from sdk/VERSION $declaredVersion."
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repositoryRoot "sdk\releases\v$Version"
}
$outputPath = [IO.Path]::GetFullPath($OutputRoot)
$sdkReleaseRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'sdk\releases'))
$sdkReleasePrefix = $sdkReleaseRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
if (-not $outputPath.StartsWith($sdkReleasePrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "SDK beta output must remain beneath sdk/releases: $outputPath"
}
if (Test-Path -LiteralPath $outputPath) {
    throw "SDK beta output already exists; refusing to overwrite it: $outputPath"
}

function Invoke-GitText {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    $output = & git -c "safe.directory=$repositoryRoot" -C $repositoryRoot @Arguments 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed."
    }
    return (($output | Out-String).Trim())
}

function Resolve-RepositoryChild {
    param([Parameter(Mandatory = $true)][string]$RelativePath)
    if ([IO.Path]::IsPathRooted($RelativePath)) {
        throw "SDK package paths must be relative: $RelativePath"
    }
    $full = [IO.Path]::GetFullPath((Join-Path $repositoryRoot $RelativePath))
    $prefix = $repositoryRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "SDK package path escapes the repository: $RelativePath"
    }
    return $full
}

function Resolve-StagingChild {
    param(
        [Parameter(Mandatory = $true)][string]$StagingRoot,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )
    if ([IO.Path]::IsPathRooted($RelativePath)) {
        throw "SDK archive paths must be relative: $RelativePath"
    }
    $full = [IO.Path]::GetFullPath((Join-Path $StagingRoot $RelativePath))
    $prefix = $StagingRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "SDK archive path escapes staging: $RelativePath"
    }
    return $full
}

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Text
    )
    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

$definitionPath = Join-Path $repositoryRoot 'sdk\package-files.json'
$definition = Get-Content -Raw -LiteralPath $definitionPath | ConvertFrom-Json
if ($definition.schemaVersion -ne 1 -or [string]$definition.sdk.version -cne $Version -or
    @($definition.files).Count -lt 1) {
    throw 'Unsupported, empty, or version-mismatched SDK package definition.'
}

$status = Invoke-GitText -Arguments @('status', '--porcelain', '--untracked-files=no')
$dirty = -not [string]::IsNullOrWhiteSpace($status)
if ($dirty -and -not $AllowDirty) {
    throw 'Absolute-Control has tracked changes; commit them before producing a beta kit.'
}
$commit = Invoke-GitText -Arguments @('rev-parse', 'HEAD')

$temporaryBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$workRoot = [IO.Path]::GetFullPath((Join-Path $temporaryBase (
    'absolute-control-sdk-' + [Guid]::NewGuid().ToString('N'))))
$temporaryPrefix = $temporaryBase.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
if (-not $workRoot.StartsWith($temporaryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to create SDK staging outside the system temporary directory.'
}
$stagingRoot = Join-Path $workRoot 'stage'
$releaseBuild = Join-Path $workRoot 'release'
New-Item -ItemType Directory -Path $stagingRoot | Out-Null
New-Item -ItemType Directory -Path $releaseBuild | Out-Null

try {
    $manifestFiles = @()
    $destinations = @{}
    foreach ($file in @($definition.files)) {
        $sourceRelative = ([string]$file.source).Replace('\', '/')
        $destinationRelative = ([string]$file.destination).Replace('\', '/').TrimStart('/')
        if ($destinations.ContainsKey($destinationRelative)) {
            throw "Duplicate SDK archive destination: $destinationRelative"
        }
        $destinations[$destinationRelative] = $true
        $source = Resolve-RepositoryChild -RelativePath $sourceRelative
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "SDK source file is missing: $source"
        }
        $destination = Resolve-StagingChild -StagingRoot $stagingRoot `
            -RelativePath $destinationRelative
        $destinationParent = Split-Path -Parent $destination
        if (-not (Test-Path -LiteralPath $destinationParent)) {
            New-Item -ItemType Directory -Path $destinationParent | Out-Null
        }
        Copy-Item -LiteralPath $source -Destination $destination
        $manifestFiles += [ordered]@{
            path = $destinationRelative
            size = (Get-Item -LiteralPath $destination).Length
            sha256 = Get-Sha256 -Path $destination
        }
    }

    $registry = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot `
        'sdk\integration-registry.json') | ConvertFrom-Json
    $manifest = [ordered]@{
        schemaVersion = 1
        sdk = [ordered]@{
            id = [string]$definition.sdk.id
            name = [string]$definition.sdk.name
            version = $Version
            channel = [string]$definition.sdk.channel
            publicRelease = [string]$definition.sdk.publicRelease
            sourceCommit = $commit
            dirty = $dirty
        }
        hostBaseline = $registry.hostBaseline
        apiBaseline = $registry.apiBaseline
        files = $manifestFiles
    }
    $manifestText = ($manifest | ConvertTo-Json -Depth 10) + [Environment]::NewLine
    $embeddedManifest = Join-Path $stagingRoot 'AbsoluteControlSDK.manifest.json'
    Write-Utf8NoBom -Path $embeddedManifest -Text $manifestText
    $externalManifest = Join-Path $releaseBuild 'AbsoluteControlSDK.manifest.json'
    Write-Utf8NoBom -Path $externalManifest -Text $manifestText

    $readme = @"
Absolute Control Integration SDK $Version
PRIVATE BETA - PUBLIC RELEASE COMING SOON

This kit targets Absolute Suite $($registry.hostBaseline.suiteVersion).
Start with sdk/README.md and sdk/BETA-ACCESS.md.

Configuration ABI: $($registry.apiBaseline.configuration.abiVersion)
Live API: ABI $($registry.apiBaseline.liveComponents.abiVersion) (experimental)
Composition API: ABI $($registry.apiBaseline.semanticComposition.abiVersion), $($registry.apiBaseline.semanticComposition.advertisedMilestone) (experimental)

Keep configuration ownership and a host-absent fallback in the subscriber plugin. Query every API
dynamically and capability-check optional tails. Report this SDK version and the host plugin version
with integration issues.
"@
    Write-Utf8NoBom -Path (Join-Path $stagingRoot 'README.txt') -Text $readme
    Write-Utf8NoBom -Path (Join-Path $releaseBuild 'README.txt') -Text $readme

    $archiveName = "Absolute-Control-SDK-v$Version-Private-Beta.zip"
    $archivePath = Join-Path $releaseBuild $archiveName
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archiveStream = [IO.File]::Open($archivePath, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $archive = [IO.Compression.ZipArchive]::new(
            $archiveStream, [IO.Compression.ZipArchiveMode]::Create, $false)
        try {
            $rootPrefix = $stagingRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
            foreach ($file in @(Get-ChildItem -LiteralPath $stagingRoot -File -Recurse |
                    Sort-Object FullName)) {
                $relative = $file.FullName.Substring($rootPrefix.Length).Replace('\', '/')
                $entry = $archive.CreateEntry($relative, [IO.Compression.CompressionLevel]::Optimal)
                $entry.LastWriteTime = [DateTimeOffset]::new(
                    2000, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
                $input = [IO.File]::OpenRead($file.FullName)
                $output = $entry.Open()
                try {
                    $input.CopyTo($output)
                } finally {
                    $output.Dispose()
                    $input.Dispose()
                }
            }
        } finally {
            $archive.Dispose()
        }
    } finally {
        $archiveStream.Dispose()
    }

    $outputParent = Split-Path -Parent $outputPath
    if (-not (Test-Path -LiteralPath $outputParent)) {
        New-Item -ItemType Directory -Path $outputParent | Out-Null
    }
    Move-Item -LiteralPath $releaseBuild -Destination $outputPath
    $finalArchive = Join-Path $outputPath $archiveName
    Write-Host "Created Absolute Control SDK beta: $finalArchive"
    Write-Host "Archive SHA-256: $(Get-Sha256 -Path $finalArchive)"
    Write-Host "Manifest SHA-256: $(Get-Sha256 -Path (Join-Path $outputPath 'AbsoluteControlSDK.manifest.json'))"
} finally {
    if (Test-Path -LiteralPath $workRoot) {
        $resolvedWork = [IO.Path]::GetFullPath($workRoot)
        if (-not $resolvedWork.StartsWith($temporaryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clean unexpected SDK staging path: $resolvedWork"
        }
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}
