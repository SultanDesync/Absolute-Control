[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RunDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateSet('01', '02', '03')]
    [string]$Phase
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Import-VisualStudioEnvironment {
    $programFilesX86 = [Environment]::GetFolderPath('ProgramFilesX86')
    $vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
    $installation = (& $vswhere -latest -products '*' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath).Trim()
    if ([string]::IsNullOrWhiteSpace($installation)) {
        throw 'No Visual Studio x64 C++ toolchain was found.'
    }
    $vsDevCmd = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
    $command = 'call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $vsDevCmd
    foreach ($line in (& $env:ComSpec /d /s /c $command)) {
        $separator = $line.IndexOf('=')
        if ($separator -le 0) { continue }
        [Environment]::SetEnvironmentVariable(
            $line.Substring(0, $separator), $line.Substring($separator + 1), 'Process')
    }
    if ($LASTEXITCODE -ne 0) {
        throw 'Visual Studio developer environment initialization failed.'
    }
}

$resolvedRun = (Resolve-Path -LiteralPath $RunDirectory).Path
$run = Get-Content -Raw -LiteralPath (Join-Path $resolvedRun 'run.json') | ConvertFrom-Json
$resultPath = Join-Path $resolvedRun "phase-$Phase-result.json"
if (-not (Test-Path -LiteralPath $resultPath -PathType Leaf)) {
    throw "Phase result is missing: $resultPath"
}
$reported = Get-Content -Raw -LiteralPath $resultPath | ConvertFrom-Json
$errors = [System.Collections.Generic.List[string]]::new()
if ($reported.status -ne 'passed') {
    $errors.Add('Builder did not report the phase passed.')
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputDirectory = Join-Path $resolvedRun "acceptance-$Phase"
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
Import-VisualStudioEnvironment

try {
    if ($Phase -eq '01') {
        & (Join-Path $PSScriptRoot 'build-subscriber.ps1') -Repository $run.subscriber.worktree
        $fixture = Join-Path $PSScriptRoot 'fixtures\SubscriberPhaseAcceptance.cpp'
        $executable = Join-Path $outputDirectory 'subscriber-acceptance.exe'
        Push-Location $outputDirectory
        try {
            & cl.exe /nologo /std:c++latest /EHsc /DSLOP_SUBSCRIBER_TEST `
                "/I$($run.subscriber.worktree)\src" `
                "/I$($run.subscriber.worktree)\include" `
                "/I$($run.subscriber.worktree)\shared\include" `
                $fixture `
                (Join-Path $run.subscriber.worktree 'src\SlopSubscriber.cpp') `
                "/Fe:$executable"
            if ($LASTEXITCODE -ne 0) { throw 'Subscriber acceptance fixture did not compile.' }
            & $executable
            if ($LASTEXITCODE -ne 0) { throw 'Subscriber acceptance fixture failed.' }
        } finally {
            Pop-Location
        }
    } elseif ($Phase -eq '02') {
        & (Join-Path $PSScriptRoot 'build-host.ps1') -Repository $run.host.worktree
        $fixture = Join-Path $PSScriptRoot 'fixtures\HostPhaseAcceptance.cpp'
        $executable = Join-Path $outputDirectory 'host-acceptance.exe'
        Push-Location $outputDirectory
        try {
            & cl.exe /nologo /std:c++latest /EHsc /DSLOP_EXPORTS `
                "/I$($run.host.worktree)\include" `
                $fixture `
                (Join-Path $run.host.worktree 'src\MenuApiHost.cpp') `
                (Join-Path $run.host.worktree 'src\MenuSession.cpp') `
                "/Fe:$executable"
            if ($LASTEXITCODE -ne 0) { throw 'Host acceptance fixture did not compile.' }
            & $executable
            if ($LASTEXITCODE -ne 0) { throw 'Host acceptance fixture failed.' }
        } finally {
            Pop-Location
        }
    } else {
        & (Join-Path $PSScriptRoot 'build-interface.ps1') `
            -Repository $run.host.worktree `
            -ToolRoot (Join-Path $run.host.source '.tools')
        & (Join-Path $PSScriptRoot 'build-host.ps1') -Repository $run.host.worktree

        $nativePath = Join-Path $run.host.worktree 'src\NativeMenuProbe.cpp'
        $actionScriptPath = Join-Path $run.host.worktree `
            'interface\src\AbsoluteControlPanelMenu.as'
        $runnerPath = Join-Path $run.host.worktree 'tools\research\run-probe.ps1'
        $moviePath = Join-Path $run.host.worktree `
            'interface\dist\AbsoluteControlPanelMenu.swf'
        $metadataPath = Join-Path $run.host.worktree `
            'interface\dist\AbsoluteControlPanelMenu.build.json'
        foreach ($path in @($nativePath, $actionScriptPath, $runnerPath, $moviePath, $metadataPath)) {
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
                throw "Phase 03 output is missing: $path"
            }
        }

        $native = Get-Content -Raw -LiteralPath $nativePath
        $actionScript = Get-Content -Raw -LiteralPath $actionScriptPath
        $sessionSource = Get-Content -Raw -LiteralPath `
            (Join-Path $run.host.worktree 'src\MenuSession.cpp')
        $runner = Get-Content -Raw -LiteralPath $runnerPath
        $runtimeSources = $native + "`n" + $actionScript
        $forbidden = `
            'toggleFeature|incrementLevel|decrementLevel|beginBindingCapture|' +
            'responseLevel|applySnapshot|dispatchCommand'
        if ($runtimeSources -match $forbidden) {
            throw 'Fixed synthetic bridge commands remain in the runtime renderer.'
        }
        if ($runner -match 'toggleFeature|incrementLevel|beginBindingCapture|applySnapshot') {
            throw 'Research runner still expects the retired synthetic bridge protocol.'
        }
        if ($native -notmatch 'MenuSession::Session' -or
            $native -notmatch 'applyModel' -or $native -notmatch '"dispatch"') {
            throw 'Native bridge does not expose MenuSession through applyModel/dispatch.'
        }
        if ($actionScript -notmatch 'function\s+applyModel\s*\(' -or
            $actionScript -notmatch 'BGSCodeObj\.dispatch' -or
            $actionScript -notmatch '["'']selectPage["'']' -or
            $actionScript -notmatch '\.controls' -or
            $actionScript -notmatch 'for\s*\(') {
            throw 'ActionScript does not contain a descriptor-driven applyModel/dispatch list.'
        }
        if ($actionScript -notmatch '["'']selectControl["'']' -and
            $sessionSource -notmatch 'selectedControlId_\s*=\s*control\.controlId') {
            throw 'Fresh models do not preserve the row selected by an edit or invocation.'
        }
        if ($actionScript -notmatch '0xFF00FF') {
            throw 'The magenta framebuffer sentinel was removed.'
        }
        $metadata = Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json
        $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $actionScriptPath).Hash
        if ($metadata.sourceSha256 -ne $sourceHash -or
            (Get-Item -LiteralPath $moviePath).Length -le 0) {
            throw 'Built SWF metadata does not match the current ActionScript source.'
        }
    }
} catch {
    $errors.Add($_.Exception.Message)
}

$evaluation = [ordered]@{
    schemaVersion = 1
    runId = $run.runId
    phase = $Phase
    valid = $errors.Count -eq 0
    disposition = if ($errors.Count -eq 0) { 'advance' } else { 'discard' }
    errors = @($errors)
}
[System.IO.File]::WriteAllText(
    (Join-Path $resolvedRun "phase-$Phase-evaluation.json"),
    ($evaluation | ConvertTo-Json -Depth 5),
    [System.Text.UTF8Encoding]::new($false))
$evaluation | ConvertTo-Json -Depth 5
if ($errors.Count -ne 0) { exit 1 }
