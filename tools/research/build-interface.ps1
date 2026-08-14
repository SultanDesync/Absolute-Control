[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$compiler = Join-Path $repositoryRoot '.tools\apache-flex-sdk-4.16.1\bin\mxmlc.bat'
$playerGlobalRoot = Join-Path $repositoryRoot '.tools\playerglobal-repo'
$sourceRoot = Join-Path $repositoryRoot 'interface\src'
$sourceProvenanceModule = Join-Path $repositoryRoot 'interface\build\SourceProvenance.psm1'
$source = Join-Path $repositoryRoot 'interface\src\AbsoluteControlPanelMenu.as'
$outputDirectory = Join-Path $repositoryRoot 'interface\dist'
$output = Join-Path $outputDirectory 'AbsoluteControlPanelMenu.swf'
$buildInfo = Join-Path $outputDirectory 'AbsoluteControlPanelMenu.build.json'
$expectedPlayerGlobalCommit = 'fef560243029214656d83fc673be0267a1ea0816'

if (-not (Test-Path -LiteralPath $compiler) -or
    -not (Test-Path -LiteralPath (Join-Path $playerGlobalRoot '11.5\playerglobal.swc'))) {
    throw 'Interface toolchain is missing. Run tools\research\bootstrap-interface-toolchain.ps1 first.'
}

$observedCommit = (git -c "safe.directory=$playerGlobalRoot" -C $playerGlobalRoot rev-parse HEAD).Trim()
if ($observedCommit -ne $expectedPlayerGlobalCommit) {
    throw "PlayerGlobal checkout is $observedCommit; expected $expectedPlayerGlobalCommit"
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$env:PLAYERGLOBAL_HOME = $playerGlobalRoot
$compilerArguments = @(
    '-target-player=11.5',
    '-swf-version=18',
    '-debug=false',
    '-static-link-runtime-shared-libraries=true',
    "-source-path+=$sourceRoot",
    "-output=$output",
    $source
)
& $compiler @compilerArguments
if ($LASTEXITCODE -ne 0) {
    throw "mxmlc failed with exit code $LASTEXITCODE"
}

$sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash
Import-Module -Name $sourceProvenanceModule -Force
$sourceProvenance = Get-ActionScriptSourceProvenance -SourceRoot $sourceRoot
$outputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $output).Hash
$metadata = [ordered]@{
    compiler = 'Apache Flex mxmlc 4.16.1 build 20171115'
    targetPlayer = '11.5'
    swfVersion = 18
    playerGlobalCommit = $observedCommit
    sourceSha256 = $sourceHash
    sourceTreeSha256 = $sourceProvenance.sourceTreeSha256
    sources = $sourceProvenance.sources
    outputSha256 = $outputHash
}
[System.IO.File]::WriteAllText(
    $buildInfo,
    (($metadata | ConvertTo-Json -Depth 4) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false))

Write-Host "Built $output"
Write-Host "SWF SHA-256: $outputHash"
