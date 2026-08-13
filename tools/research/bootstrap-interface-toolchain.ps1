[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$toolsRoot = Join-Path $repositoryRoot '.tools'
$flexArchive = Join-Path $toolsRoot 'apache-flex-sdk-4.16.1-bin.zip'
$flexRoot = Join-Path $toolsRoot 'apache-flex-sdk-4.16.1'
$playerGlobalRoot = Join-Path $toolsRoot 'playerglobal-repo'

$flexUrl = 'https://archive.apache.org/dist/flex/4.16.1/binaries/apache-flex-sdk-4.16.1-bin.zip'
$flexMd5 = '8841C64BD5E32F8575EBA86E2574873A'
$flexSha256 = '757AA19299C8A9C8AF0901C1AE35F97FA94B7AF0B0A9ABC2BAB04FE61D756E8B'
$playerGlobalCommit = 'fef560243029214656d83fc673be0267a1ea0816'

New-Item -ItemType Directory -Force -Path $toolsRoot | Out-Null

if (-not (Test-Path -LiteralPath $flexArchive)) {
    Invoke-WebRequest -Uri $flexUrl -OutFile $flexArchive
}

$observedMd5 = (Get-FileHash -Algorithm MD5 -LiteralPath $flexArchive).Hash
$observedSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $flexArchive).Hash
if ($observedMd5 -ne $flexMd5 -or $observedSha256 -ne $flexSha256) {
    throw "Apache Flex archive checksum mismatch. Refusing to extract $flexArchive"
}

if (-not (Test-Path -LiteralPath $flexRoot)) {
    Expand-Archive -LiteralPath $flexArchive -DestinationPath $flexRoot
}

if (-not (Test-Path -LiteralPath $playerGlobalRoot)) {
    git clone --filter=blob:none --sparse https://github.com/nexussays/playerglobal.git $playerGlobalRoot
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not clone the PlayerGlobal definitions.'
    }
    git -C $playerGlobalRoot sparse-checkout set 11.5
    git -C $playerGlobalRoot checkout $playerGlobalCommit
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not check out the pinned PlayerGlobal commit.'
    }
}

$observedCommit = (git -c "safe.directory=$playerGlobalRoot" -C $playerGlobalRoot rev-parse HEAD).Trim()
if ($observedCommit -ne $playerGlobalCommit) {
    throw "PlayerGlobal checkout is $observedCommit; expected $playerGlobalCommit"
}

Write-Host 'Native-menu interface toolchain is ready.'
Write-Host "Apache Flex 4.16.1 SHA-256: $observedSha256"
Write-Host "PlayerGlobal commit: $observedCommit (target 11.5)"
