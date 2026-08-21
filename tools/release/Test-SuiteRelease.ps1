[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ReleaseDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$releaseRoot = (Resolve-Path -LiteralPath $ReleaseDirectory).Path
$archives = @(Get-ChildItem -LiteralPath $releaseRoot -File -Filter 'Absolute-Suite-v*-Release.zip')
if ($archives.Count -ne 1) {
    throw "Expected exactly one Absolute Suite release archive; found $($archives.Count)."
}
$archivePath = $archives[0].FullName
$externalManifestPath = Join-Path $releaseRoot 'AbsoluteSuite.manifest.json'
foreach ($document in @('AbsoluteSuite.manifest.json', 'README.txt', 'CHANGELOG.txt', 'nexus_description.txt')) {
    $path = Join-Path $releaseRoot $document
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -le 0) {
        throw "Required release document is missing or empty: $path"
    }
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

function Get-FileSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    $stream = [IO.File]::OpenRead($Path)
    try {
        return Get-StreamSha256 -Stream $stream
    } finally {
        $stream.Dispose()
    }
}

$externalManifestHash = Get-FileSha256 -Path $externalManifestPath
$manifest = Get-Content -Raw -LiteralPath $externalManifestPath | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1 -or $manifest.bundle.format -ne 'fomod-v5') {
    throw 'Unsupported suite manifest schema or archive format.'
}
$required = @($manifest.components | Where-Object { $_.required })
if ($required.Count -ne 1 -or $required[0].id -ne 'control') {
    throw "Absolute Control must be the suite archive's sole required component."
}

$expected = @('AbsoluteSuite.manifest.json', 'fomod/info.xml', 'fomod/ModuleConfig.xml')
foreach ($component in @($manifest.components)) {
    foreach ($file in @($component.files)) {
        $expected += [string]$file.path
    }
}
$expected = @($expected | Sort-Object -Unique)

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($archivePath)
try {
    $entries = @{}
    foreach ($entry in $archive.Entries) {
        if ([string]::IsNullOrEmpty($entry.Name)) { continue }
        $name = $entry.FullName.Replace('\', '/').TrimStart('/')
        if ($entries.ContainsKey($name)) {
            throw "Duplicate archive entry: $name"
        }
        $entries[$name] = $entry
    }
    $observed = @($entries.Keys | Sort-Object)
    if (($expected -join "`n") -cne ($observed -join "`n")) {
        throw "Suite archive contents differ from the manifest. Expected [$($expected -join ', ')]; observed [$($observed -join ', ')]."
    }
    foreach ($name in $observed) {
        if ($name -match '(?i)(_Custom\.ini$|\.pdb$|research|workbench|\.log$|(^|/)Profiles/)') {
            throw "Forbidden user, debug, research, or retired artifact in suite archive: $name"
        }
    }

    $embeddedManifestStream = $entries['AbsoluteSuite.manifest.json'].Open()
    try {
        $embeddedHash = Get-StreamSha256 -Stream $embeddedManifestStream
    } finally {
        $embeddedManifestStream.Dispose()
    }
    if ($embeddedHash -ine $externalManifestHash) {
        throw 'Embedded and external suite manifests differ.'
    }

    foreach ($component in @($manifest.components)) {
        foreach ($file in @($component.files)) {
            $stream = $entries[[string]$file.path].Open()
            try {
                $hash = Get-StreamSha256 -Stream $stream
            } finally {
                $stream.Dispose()
            }
            if ($hash -ine [string]$file.sha256) {
                throw "Archive file hash differs from suite manifest: $($file.path)"
            }
            if ($entries[[string]$file.path].Length -ne [long]$file.size) {
                throw "Archive file size differs from suite manifest: $($file.path)"
            }
        }
    }

    $moduleConfigReader = [IO.StreamReader]::new($entries['fomod/ModuleConfig.xml'].Open())
    try {
        $moduleConfig = $moduleConfigReader.ReadToEnd()
    } finally {
        $moduleConfigReader.Dispose()
    }
    [void][xml]$moduleConfig
    $infoReader = [IO.StreamReader]::new($entries['fomod/info.xml'].Open())
    try {
        [void][xml]$infoReader.ReadToEnd()
    } finally {
        $infoReader.Dispose()
    }
    foreach ($component in @($manifest.components)) {
        if ($moduleConfig -notmatch [Regex]::Escape([string]$component.installGroup)) {
            throw "FOMOD does not reference component install group: $($component.installGroup)"
        }
    }
} finally {
    $archive.Dispose()
}

$archiveHash = Get-FileSha256 -Path $archivePath
$readme = Get-Content -Raw -LiteralPath (Join-Path $releaseRoot 'README.txt')
if ($readme -notmatch [Regex]::Escape($archiveHash) -or
    $readme -notmatch [Regex]::Escape($externalManifestHash)) {
    throw 'README release hashes do not match the archive and external manifest.'
}

Write-Host "Validated Absolute Suite release: $archivePath"
Write-Host "Validated $($expected.Count) exact archive entries."
