[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9A-Za-z][0-9A-Za-z.-]*$')]
    [string]$BundleVersion,

    [string]$SiblingRoot,
    [string]$OutputRoot,
    [switch]$AllowDirty,
    [switch]$AllowStaleArtifacts
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$releaseExcludesPath = (Resolve-Path -LiteralPath (Join-Path $repositoryRoot 'packaging\suite\empty-excludes')).Path
if ([string]::IsNullOrWhiteSpace($SiblingRoot)) {
    $SiblingRoot = Split-Path -Parent $repositoryRoot
}
$siblingRootPath = (Resolve-Path -LiteralPath $SiblingRoot).Path
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repositoryRoot "releases\v$BundleVersion"
}
$outputPath = [IO.Path]::GetFullPath($OutputRoot)
$repositoryPrefix = $repositoryRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
if (-not $outputPath.StartsWith($repositoryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Default release tooling writes only beneath the Absolute-Control repository: $outputPath"
}
if (Test-Path -LiteralPath $outputPath) {
    throw "Release output already exists; refusing to overwrite it: $outputPath"
}

$definitionPath = Join-Path $repositoryRoot 'packaging\suite\suite-components.json'
$definition = Get-Content -Raw -LiteralPath $definitionPath | ConvertFrom-Json
if ($definition.schemaVersion -ne 1 -or @($definition.components).Count -lt 1) {
    throw 'Unsupported or empty suite component definition.'
}

function Invoke-GitText {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )
    # Keep inaccessible or machine-local global ignore files from contaminating
    # porcelain output. Release dirtiness is repository-local by definition.
    $result = & git -c "safe.directory=$Repository" -c "core.excludesFile=$releaseExcludesPath" -C $Repository @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed for $Repository`: $($result -join [Environment]::NewLine)"
    }
    return (($result | Out-String).Trim())
}

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Resolve-ChildPath {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )
    if ([IO.Path]::IsPathRooted($RelativePath)) {
        throw "Suite source/destination paths must be relative: $RelativePath"
    }
    $full = [IO.Path]::GetFullPath((Join-Path $Root $RelativePath))
    $prefix = $Root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Suite path escapes its root: $RelativePath"
    }
    return $full
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

function Expand-Template {
    param(
        [Parameter(Mandatory = $true)][string]$TemplatePath,
        [Parameter(Mandatory = $true)][string]$Destination,
        [Parameter(Mandatory = $true)][hashtable]$Tokens
    )
    $text = Get-Content -Raw -LiteralPath $TemplatePath
    foreach ($key in $Tokens.Keys) {
        $text = $text.Replace("@$key@", [string]$Tokens[$key])
    }
    if ($text -match '@[A-Z][A-Z0-9_]+@') {
        throw "Template contains unresolved tokens: $TemplatePath"
    }
    Write-Utf8NoBom -Path $Destination -Text $text
}

$temporaryBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$workRoot = Join-Path $temporaryBase ("absolute-suite-" + [Guid]::NewGuid().ToString('N'))
$workFull = [IO.Path]::GetFullPath($workRoot)
$temporaryPrefix = $temporaryBase.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
if (-not $workFull.StartsWith($temporaryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to create suite staging outside the system temporary directory.'
}
$stagingFull = Join-Path $workFull 'stage'
$releaseBuild = Join-Path $workFull 'release'
New-Item -ItemType Directory -Path $stagingFull | Out-Null
New-Item -ItemType Directory -Path $releaseBuild | Out-Null

try {
    $manifestComponents = @()
    $componentLines = @()
    foreach ($component in @($definition.components)) {
        $componentRepository = Resolve-ChildPath -Root $siblingRootPath -RelativePath ([string]$component.repository)
        if (-not (Test-Path -LiteralPath (Join-Path $componentRepository '.git'))) {
            throw "Component repository is missing: $componentRepository"
        }
        $status = Invoke-GitText -Repository $componentRepository -Arguments @('status', '--porcelain')
        $dirty = -not [string]::IsNullOrWhiteSpace($status)
        if ($dirty -and -not $AllowDirty) {
            throw "Component repository is dirty: $($component.name) ($componentRepository)"
        }
        $commit = Invoke-GitText -Repository $componentRepository -Arguments @('rev-parse', 'HEAD')
        $commitEpoch = [long](Invoke-GitText -Repository $componentRepository -Arguments @('show', '-s', '--format=%ct', 'HEAD'))
        $commitTime = [DateTimeOffset]::FromUnixTimeSeconds($commitEpoch)
        $manifestFiles = @()

        foreach ($file in @($component.files)) {
            $source = Resolve-ChildPath -Root $componentRepository -RelativePath ([string]$file.source)
            if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
                throw "Component artifact is missing: $source"
            }
            $extension = [IO.Path]::GetExtension($source)
            if ($extension -in @('.dll', '.swf') -and
                (Get-Item -LiteralPath $source).LastWriteTimeUtc -lt $commitTime.UtcDateTime) {
                if (-not $AllowStaleArtifacts) {
                    throw "Component artifact predates its source commit; rebuild before packaging: $source"
                }
                Write-Warning "Allowing artifact older than commit for development packaging: $source"
            }
            $installRelative = ([string]$component.installGroup).TrimEnd('\', '/') + '/' +
                ([string]$file.destination).Replace('\', '/').TrimStart('/')
            $destination = Resolve-ChildPath -Root $stagingFull -RelativePath $installRelative
            $destinationParent = Split-Path -Parent $destination
            if (-not (Test-Path -LiteralPath $destinationParent)) {
                New-Item -ItemType Directory -Path $destinationParent | Out-Null
            }
            Copy-Item -LiteralPath $source -Destination $destination
            $manifestFiles += [ordered]@{
                path = $installRelative
                installedPath = ([string]$file.destination).Replace('\', '/')
                size = (Get-Item -LiteralPath $destination).Length
                sha256 = Get-Sha256 -Path $destination
            }
        }

        $manifestComponents += [ordered]@{
            id = [string]$component.id
            name = [string]$component.name
            version = [string]$component.version
            repository = [string]$component.repository
            commit = $commit
            dirty = $dirty
            required = [bool]$component.required
            installGroup = [string]$component.installGroup
            files = $manifestFiles
        }
        $role = if ([bool]$component.required) { 'required' } else { 'optional' }
        $componentLines += "- $($component.name) $($component.version) ($role)"
    }

    $fomodTokens = @{ BUNDLE_VERSION = $BundleVersion }
    Expand-Template `
        -TemplatePath (Join-Path $repositoryRoot 'packaging\suite\fomod\info.xml.in') `
        -Destination (Join-Path $stagingFull 'fomod\info.xml') `
        -Tokens $fomodTokens
    Expand-Template `
        -TemplatePath (Join-Path $repositoryRoot 'packaging\suite\fomod\ModuleConfig.xml.in') `
        -Destination (Join-Path $stagingFull 'fomod\ModuleConfig.xml') `
        -Tokens $fomodTokens

    $manifest = [ordered]@{
        schemaVersion = 1
        bundle = [ordered]@{
            id = [string]$definition.bundle.id
            name = [string]$definition.bundle.name
            version = $BundleVersion
            format = 'fomod-v5'
            author = [string]$definition.bundle.author
            website = [string]$definition.bundle.website
        }
        components = $manifestComponents
    }
    $manifestText = ($manifest | ConvertTo-Json -Depth 10) + [Environment]::NewLine
    $embeddedManifest = Join-Path $stagingFull 'AbsoluteSuite.manifest.json'
    Write-Utf8NoBom -Path $embeddedManifest -Text $manifestText
    $externalManifest = Join-Path $releaseBuild 'AbsoluteSuite.manifest.json'
    Write-Utf8NoBom -Path $externalManifest -Text $manifestText
    $manifestHash = Get-Sha256 -Path $externalManifest

    $archiveName = "Absolute-Suite-v$BundleVersion-Release.zip"
    $archivePath = Join-Path $releaseBuild $archiveName
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archiveStream = [IO.File]::Open($archivePath, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $archive = [IO.Compression.ZipArchive]::new(
            $archiveStream, [IO.Compression.ZipArchiveMode]::Create, $false)
        try {
            $rootPrefix = $stagingFull.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
            $files = @(Get-ChildItem -LiteralPath $stagingFull -File -Recurse | Sort-Object FullName)
            foreach ($file in $files) {
                $relative = $file.FullName.Substring($rootPrefix.Length).Replace('\', '/')
                $entry = $archive.CreateEntry($relative, [IO.Compression.CompressionLevel]::Optimal)
                $entry.LastWriteTime = [DateTimeOffset]::new(2000, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
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
    $archiveHash = Get-Sha256 -Path $archivePath

    $documentTokens = @{
        BUNDLE_VERSION = $BundleVersion
        ARCHIVE_NAME = $archiveName
        ARCHIVE_SHA256 = $archiveHash
        MANIFEST_SHA256 = $manifestHash
        COMPONENT_LIST = ($componentLines -join [Environment]::NewLine)
    }
    foreach ($document in @('README.txt', 'CHANGELOG.txt', 'nexus_description.txt')) {
        Expand-Template `
            -TemplatePath (Join-Path $repositoryRoot "packaging\suite\docs\$document.in") `
            -Destination (Join-Path $releaseBuild $document) `
            -Tokens $documentTokens
    }

    $outputParent = Split-Path -Parent $outputPath
    if (-not (Test-Path -LiteralPath $outputParent)) {
        New-Item -ItemType Directory -Path $outputParent | Out-Null
    }
    Move-Item -LiteralPath $releaseBuild -Destination $outputPath
    $finalArchivePath = Join-Path $outputPath $archiveName
    Write-Host "Created Absolute Suite release candidate: $finalArchivePath"
    Write-Host "Archive SHA-256: $archiveHash"
    Write-Host "Manifest SHA-256: $manifestHash"
} finally {
    if (Test-Path -LiteralPath $workFull) {
        $resolvedWork = [IO.Path]::GetFullPath($workFull)
        if (-not $resolvedWork.StartsWith($temporaryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove staging outside the system temporary directory: $resolvedWork"
        }
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}
