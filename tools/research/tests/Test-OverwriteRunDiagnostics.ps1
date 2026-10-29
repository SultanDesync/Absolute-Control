[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$scriptPath = (Resolve-Path (Join-Path $PSScriptRoot '..\measure-overwrite-run.ps1')).Path
$fixture = Join-Path ([IO.Path]::GetTempPath()) (
    'absolute-control-overwrite-test-' + [Guid]::NewGuid().ToString('N'))
$overwrite = Join-Path $fixture 'overwrite'
$artifacts = Join-Path $fixture 'artifacts'

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw "Assertion failed: $Message" }
}

function Read-Json {
    param([string]$Path)
    return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

try {
    [IO.Directory]::CreateDirectory((Join-Path $overwrite 'Data\cache')) | Out-Null
    [IO.Directory]::CreateDirectory($artifacts) | Out-Null
    [IO.File]::WriteAllText((Join-Path $overwrite 'Data\base.bin'), '1234')
    [IO.File]::WriteAllText((Join-Path $overwrite 'Data\cache\ui.osf.cache'), '12')

    $beforePath = Join-Path $artifacts 'before.json'
    & $scriptPath -Mode Snapshot -OverwritePath $overwrite -OutputPath $beforePath |
        Out-Null
    $before = Read-Json $beforePath
    Assert-True ($before.kind -eq 'overwrite-run-snapshot') 'snapshot kind'
    Assert-True ($before.currentSnapshot.status -eq 'complete') 'complete snapshot'
    Assert-True ($before.currentSnapshot.totals.fileCount -eq 2) 'snapshot file count'
    Assert-True ($before.currentSnapshot.totals.totalBytes -eq 6) 'snapshot byte count'
    Assert-True (@($before.currentSnapshot.files | Where-Object {
        $_.relativePath -eq 'Data\base.bin' -and $_.lengthBytes -eq 4 -and
            -not [string]::IsNullOrWhiteSpace($_.lastWriteTimeUtc)
    }).Count -eq 1) 'relative path, length, and timestamp metadata'
    Assert-True (@($before.currentSnapshot.directoryRollups.records | Where-Object {
        $_.relativePath -eq 'Data' -and $_.observedBytes -eq 6
    }).Count -eq 1) 'directory rollup'

    Start-Sleep -Milliseconds 20
    [IO.File]::WriteAllText((Join-Path $overwrite 'Data\base.bin'), '123456789')
    $gpuCache = Join-Path $overwrite 'Data\OSFUI\WebView2\GPUCache'
    $codeCache = Join-Path $overwrite 'Data\OSFUI\WebView2\Code Cache'
    [IO.Directory]::CreateDirectory($gpuCache) | Out-Null
    [IO.Directory]::CreateDirectory($codeCache) | Out-Null
    [IO.File]::WriteAllText((Join-Path $gpuCache 'gpu.bin'), '12345')
    [IO.File]::WriteAllText((Join-Path $codeCache 'code.bin'), '123')
    $afterPath = Join-Path $artifacts 'after.json'
    & $scriptPath -Mode Compare -OverwritePath $overwrite -OutputPath $afterPath `
        -BaselinePath $beforePath | Out-Null
    $after = Read-Json $afterPath
    Assert-True ($after.comparison.comparable -eq $true) 'comparison is comparable'
    Assert-True ($after.comparison.totalDelta.fileCount -eq 2) 'file count delta'
    Assert-True ($after.comparison.totalDelta.bytes -eq 13) 'byte delta'
    Assert-True (@($after.comparison.directoryChanges.records | Where-Object {
        $_.relativePath -eq 'Data' -and $_.deltaBytes -eq 13 -and
            $_.changedFileCount -eq 3
    }).Count -eq 1) ("comparison directory rollup: " +
        ($after.comparison.directoryChanges.records | ConvertTo-Json -Compress))
    Assert-True ($after.comparison.largestGrowthFiles.records[0].relativePath -eq
        'Data\base.bin') 'largest growth file ordering'
    Assert-True ($after.comparison.filenameSuspicion.totalCount -ge 1) `
        'filename-only cache suspicion'
    $suspicionLabels = @($after.comparison.filenameSuspicion.records |
        ForEach-Object { @($_.filenameSuspicion) })
    Assert-True ($suspicionLabels -contains 'osfui_name_token') 'OSFUI filename suspicion'
    Assert-True ($suspicionLabels -contains 'webview2_name_token') `
        'WebView2 filename suspicion'
    Assert-True ($suspicionLabels -contains 'gpu_cache_name_token') `
        'GPUCache filename suspicion'
    Assert-True ($suspicionLabels -contains 'code_cache_name_token') `
        'Code Cache filename suspicion'
    Assert-True ($after.comparison.filenameSuspicion.disclaimer -match 'do not.*cause') `
        'suspicion disclaimer'

    $missingPath = Join-Path $artifacts 'missing.json'
    & $scriptPath -Mode Snapshot -OverwritePath (Join-Path $fixture 'missing') `
        -OutputPath $missingPath | Out-Null
    $missing = Read-Json $missingPath
    Assert-True ($missing.currentSnapshot.status -eq 'missing') 'missing root status'
    Assert-True ($null -eq $missing.currentSnapshot.totals.totalBytes) `
        'missing root total is null rather than zero'
    Assert-True ($missing.currentSnapshot.errors.totalCount -eq 1) 'missing root error'

    $boundedPath = Join-Path $artifacts 'bounded.json'
    & $scriptPath -Mode Snapshot -OverwritePath $overwrite -OutputPath $boundedPath `
        -MaxFiles 1 | Out-Null
    $bounded = Read-Json $boundedPath
    Assert-True ($bounded.currentSnapshot.status -eq 'incomplete') 'bounded scan status'
    Assert-True ($null -eq $bounded.currentSnapshot.totals.fileCount) `
        'bounded total is null rather than partial count'
    Assert-True ($bounded.currentSnapshot.totals.observedFileCount -eq 1) `
        'bounded scan reports explicit observed count'

    $outside = Join-Path $fixture 'outside'
    [IO.Directory]::CreateDirectory($outside) | Out-Null
    [IO.File]::WriteAllText((Join-Path $outside 'must-not-appear.bin'), 'secret')
    $junction = Join-Path $overwrite 'Data\linked-outside'
    try {
        New-Item -ItemType Junction -Path $junction -Target $outside -ErrorAction Stop |
            Out-Null
        $reparsePath = Join-Path $artifacts 'reparse.json'
        & $scriptPath -Mode Snapshot -OverwritePath $overwrite -OutputPath $reparsePath |
            Out-Null
        $reparse = Read-Json $reparsePath
        Assert-True ($reparse.currentSnapshot.reparseBoundaries.totalCount -eq 1) `
            'reparse boundary recorded'
        Assert-True (@($reparse.currentSnapshot.files | Where-Object {
            $_.relativePath -match 'must-not-appear'
        }).Count -eq 0) 'reparse target was not traversed'
    } finally {
        if ([IO.Directory]::Exists($junction)) {
            # Windows PowerShell 5.1 has a Remove-Item junction bug. The .NET
            # non-recursive delete removes the junction itself, never its target.
            [IO.Directory]::Delete($junction)
        }
    }

    Write-Host 'Overwrite run diagnostic tests passed.'
} finally {
    if ([IO.Directory]::Exists($fixture)) {
        Remove-Item -LiteralPath $fixture -Recurse -Force
    }
}
