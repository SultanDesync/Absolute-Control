[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ReleaseDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$releaseRoot = (Resolve-Path -LiteralPath $ReleaseDirectory).Path
$archives = @(Get-ChildItem -LiteralPath $releaseRoot -File `
    -Filter 'Absolute-Control-SDK-v*-Private-Beta.zip')
if ($archives.Count -ne 1) {
    throw "Expected exactly one SDK beta archive; found $($archives.Count)."
}
$archivePath = $archives[0].FullName
$externalManifestPath = Join-Path $releaseRoot 'AbsoluteControlSDK.manifest.json'
$externalReadmePath = Join-Path $releaseRoot 'README.txt'
foreach ($path in @($externalManifestPath, $externalReadmePath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-Item -LiteralPath $path).Length -le 0) {
        throw "Required SDK release file is missing or empty: $path"
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

$manifest = Get-Content -Raw -LiteralPath $externalManifestPath | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1 -or $manifest.sdk.channel -ne 'private-beta' -or
    $manifest.sdk.dirty) {
    throw 'SDK manifest is unsupported, not a private beta, or was built from tracked changes.'
}
$expected = @('AbsoluteControlSDK.manifest.json', 'README.txt')
$expected += @($manifest.files | ForEach-Object { [string]$_.path })
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
            throw "Duplicate SDK archive entry: $name"
        }
        $entries[$name] = $entry
    }
    $observed = @($entries.Keys | Sort-Object)
    if (($expected -join "`n") -cne ($observed -join "`n")) {
        throw "SDK archive contents differ from the manifest. Expected [$($expected -join ', ')]; observed [$($observed -join ', ')]."
    }
    foreach ($name in $observed) {
        if ($name -match '(?i)(\.dll$|\.pdb$|\.lib$|research|SlopAPI|_Custom\.ini$|\.log$|(^|/)Profiles/)') {
            throw "Forbidden runtime, legacy, user, debug, or research artifact in SDK: $name"
        }
    }

    $externalManifestHash = Get-FileSha256 -Path $externalManifestPath
    $embeddedManifestStream = $entries['AbsoluteControlSDK.manifest.json'].Open()
    try {
        $embeddedManifestHash = Get-StreamSha256 -Stream $embeddedManifestStream
    } finally {
        $embeddedManifestStream.Dispose()
    }
    if ($externalManifestHash -ine $embeddedManifestHash) {
        throw 'Embedded and external SDK manifests differ.'
    }

    foreach ($file in @($manifest.files)) {
        $entry = $entries[[string]$file.path]
        if ($entry.Length -ne [long]$file.size) {
            throw "SDK archive file size differs from manifest: $($file.path)"
        }
        $stream = $entry.Open()
        try {
            $hash = Get-StreamSha256 -Stream $stream
        } finally {
            $stream.Dispose()
        }
        if ($hash -ine [string]$file.sha256) {
            throw "SDK archive file hash differs from manifest: $($file.path)"
        }
    }

    foreach ($header in @('configuration', 'liveComponents', 'semanticComposition')) {
        $baseline = $manifest.apiBaseline.$header
        $entry = $entries[[string]$baseline.header]
        if (-not $entry) {
            throw "SDK baseline header is absent: $($baseline.header)"
        }
        $stream = $entry.Open()
        try {
            $hash = Get-StreamSha256 -Stream $stream
        } finally {
            $stream.Dispose()
        }
        if ($hash -ine [string]$baseline.sha256) {
            throw "SDK baseline header identity differs: $($baseline.header)"
        }
    }

    foreach ($name in $observed) {
        if ($name -notmatch '(?i)\.(md|txt|json|h|cpp|py|lua)$') { continue }
        $reader = [IO.StreamReader]::new($entries[$name].Open())
        try {
            $text = $reader.ReadToEnd()
        } finally {
            $reader.Dispose()
        }
        if ($text -match '(?i)[A-Z]:\\Users\\|D:\\Modlists\\|OneDrive\\Documents\\Projects') {
            throw "Local workspace or mod-list path leaked into SDK archive: $name"
        }
    }
} finally {
    $archive.Dispose()
}

$archiveHash = Get-FileSha256 -Path $archivePath
Write-Host "Validated Absolute Control SDK beta: $archivePath"
Write-Host "Validated $($expected.Count) exact archive entries."
Write-Host "Archive SHA-256: $archiveHash"
