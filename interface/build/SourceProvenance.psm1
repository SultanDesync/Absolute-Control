Set-StrictMode -Version Latest

function Get-Sha256FileHash {
    param([Parameter(Mandatory = $true)][string]$LiteralPath)
    $stream = [System.IO.File]::OpenRead($LiteralPath)
    $digest = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString(
                $digest.ComputeHash($stream))).Replace('-', '')
    } finally {
        $digest.Dispose()
        $stream.Dispose()
    }
}

function Get-ActionScriptSourceProvenance {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceRoot
    )

    $resolvedRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
    $records = @(
        Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File |
            Where-Object { $_.Extension -in @('.as', '.ttf', '.otf') } |
            ForEach-Object {
                [pscustomobject]@{
                    path = $_.FullName.Substring($resolvedRoot.Length).TrimStart('\').Replace('\', '/')
                    sha256 = Get-Sha256FileHash -LiteralPath $_.FullName
                }
            } |
            Sort-Object -Property path
    )
    if ($records.Count -eq 0) {
        throw "No ActionScript or embedded-font inputs found below $resolvedRoot"
    }

    $treeMaterial = ($records | ForEach-Object {
            "$($_.path)`0$($_.sha256)"
        }) -join "`n"
    $treeBytes = [System.Text.Encoding]::UTF8.GetBytes($treeMaterial)
    $digest = [System.Security.Cryptography.SHA256]::Create()
    try {
        $treeHash = ([System.BitConverter]::ToString(
                $digest.ComputeHash($treeBytes))).Replace('-', '')
    } finally {
        $digest.Dispose()
    }

    return [pscustomobject]@{
        sources = $records
        sourceTreeSha256 = $treeHash
    }
}

Export-ModuleMember -Function Get-ActionScriptSourceProvenance
