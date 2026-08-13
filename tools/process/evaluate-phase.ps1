[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RunDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateSet('01', '02')]
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
    } else {
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
