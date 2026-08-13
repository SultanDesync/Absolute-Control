[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Repository
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolved = (Resolve-Path -LiteralPath $Repository).Path
if (-not (Test-Path -LiteralPath (Join-Path $resolved 'xmake.lua') -PathType Leaf)) {
    throw "SLOP xmake.lua is missing from $resolved"
}

$buildDirectory = Join-Path $resolved 'build'
Push-Location $resolved
try {
    & xmake f -o $buildDirectory -m releasedbg
    if ($LASTEXITCODE -ne 0) {
        throw "SLOP configuration failed with exit code $LASTEXITCODE"
    }
    & xmake
    if ($LASTEXITCODE -ne 0) {
        throw "SLOP build failed with exit code $LASTEXITCODE"
    }
    & xmake test
    if ($LASTEXITCODE -ne 0) {
        throw "SLOP tests failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}
