[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Repository,

    [string]$ConfigurePreset = 'build-release',
    [string]$BuildPreset = 'release',
    [string]$TestDirectory = 'build\release',

    [ValidateSet('Auto', 'Existing', 'SlopRecommended')]
    [string]$EnvironmentMode = 'Auto'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Import-VisualStudioEnvironment {
    $programFilesX86 = [Environment]::GetFolderPath('ProgramFilesX86')
    $vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw "Visual Studio locator is missing: $vswhere"
    }
    $installation = (& $vswhere -latest -products '*' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath).Trim()
    if ([string]::IsNullOrWhiteSpace($installation)) {
        throw 'No Visual Studio installation with the x64 C++ toolchain was found.'
    }

    $vsDevCmd = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
    $ninja = Join-Path $installation `
        'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
    $vcpkgRoot = Join-Path $installation 'VC\vcpkg'
    foreach ($required in @($vsDevCmd, $ninja, (Join-Path $vcpkgRoot 'vcpkg.exe'))) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Required Visual Studio build component is missing: $required"
        }
    }

    $command = 'call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $vsDevCmd
    $environment = & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) {
        throw 'Visual Studio developer environment initialization failed.'
    }
    foreach ($line in $environment) {
        $separator = $line.IndexOf('=')
        if ($separator -le 0) { continue }
        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        [Environment]::SetEnvironmentVariable($name, $value, 'Process')
    }
    $env:VCPKG_ROOT = $vcpkgRoot
    $env:PATH = "$(Split-Path -Parent $ninja);$env:PATH"
}

$resolved = (Resolve-Path -LiteralPath $Repository).Path
if (-not (Test-Path -LiteralPath (Join-Path $resolved 'CMakePresets.json') -PathType Leaf)) {
    throw "Subscriber CMakePresets.json is missing from $resolved"
}

if ($EnvironmentMode -eq 'SlopRecommended' -or
    ($EnvironmentMode -eq 'Auto' -and
     ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT) -or
      $null -eq (Get-Command ninja.exe -ErrorAction SilentlyContinue)))) {
    Import-VisualStudioEnvironment
}
Push-Location $resolved
try {
    & cmake --preset $ConfigurePreset
    if ($LASTEXITCODE -ne 0) {
        throw "Subscriber configure failed with exit code $LASTEXITCODE"
    }
    & cmake --build --preset $BuildPreset
    if ($LASTEXITCODE -ne 0) {
        throw "Subscriber build failed with exit code $LASTEXITCODE"
    }
    & ctest --test-dir $TestDirectory --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "Subscriber tests failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}
