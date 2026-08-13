[CmdletBinding()]
param(
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$dll = Join-Path $repoRoot "build\windows\x64\release\AbsoluteControlPanelResearch.dll"
$swf = Join-Path $repoRoot "interface\dist\AbsoluteControlPanelMenu.swf"
$swfMetadata = Join-Path $repoRoot "interface\dist\AbsoluteControlPanelMenu.build.json"
$swfSource = Join-Path $repoRoot "interface\src\AbsoluteControlPanelMenu.as"
$config = Join-Path $repoRoot "packaging\compatibility-test\AbsoluteControlPanelResearch.ini"
$readme = Join-Path $repoRoot "packaging\compatibility-test\README.txt"

foreach ($requiredFile in @($dll, $swf, $swfMetadata, $swfSource, $config, $readme)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required package input is missing: $requiredFile"
    }
}

$interfaceBuild = Get-Content -LiteralPath $swfMetadata -Raw | ConvertFrom-Json
$currentSourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $swfSource).Hash
$currentMovieHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $swf).Hash
if ($interfaceBuild.sourceSha256 -ne $currentSourceHash -or
    $interfaceBuild.outputSha256 -ne $currentMovieHash) {
    throw 'Refusing to package a stale or unrecorded Scaleform movie. Rebuild the interface first.'
}

$packageRoot = Join-Path $repoRoot "artifacts\packages"
$stageRoot = Join-Path $packageRoot "stage-$Version"
$archive = Join-Path $packageRoot "Starfield-Local-Options-Panel-PauseMenu-Compatibility-Test-$Version.zip"

if (Test-Path -LiteralPath $stageRoot) {
    $resolvedStage = (Resolve-Path -LiteralPath $stageRoot).Path
    $resolvedPackages = (Resolve-Path -LiteralPath $packageRoot).Path
    if (-not $resolvedStage.StartsWith($resolvedPackages + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean staging path outside the package directory: $resolvedStage"
    }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}

New-Item -ItemType Directory -Path (Join-Path $stageRoot "SFSE\Plugins") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stageRoot "Interface") -Force | Out-Null
Copy-Item -LiteralPath $dll -Destination (Join-Path $stageRoot "SFSE\Plugins\AbsoluteControlPanelResearch.dll")
Copy-Item -LiteralPath $config -Destination (Join-Path $stageRoot "SFSE\Plugins\AbsoluteControlPanelResearch.ini")
Copy-Item -LiteralPath $swf -Destination (Join-Path $stageRoot "Interface\AbsoluteControlPanelMenu.swf")
Copy-Item -LiteralPath $readme -Destination (Join-Path $stageRoot "README.txt")

if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}

Compress-Archive -Path (Join-Path $stageRoot "*") -DestinationPath $archive -CompressionLevel Optimal
Get-FileHash -LiteralPath $archive -Algorithm SHA256
