[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Snapshot', 'Compare')]
    [string]$Mode,

    [Parameter(Mandatory = $true)]
    [string]$OverwritePath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [string]$BaselinePath,

    [ValidateRange(1, 100000)]
    [int]$MaxFiles = 20000,

    [ValidateRange(1, 5000)]
    [int]$MaxErrors = 200,

    [ValidateRange(1, 5000)]
    [int]$MaxDirectoryRollups = 200,

    [ValidateRange(1, 1000)]
    [int]$MaxChangedFiles = 200,

    [ValidateRange(1, 1000)]
    [int]$MaxLargestGrowthFiles = 50
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-FullPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return [IO.Path]::GetFullPath($Path)
}

function Test-PathInsideRoot {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root
    )

    $separator = [IO.Path]::DirectorySeparatorChar
    $rootWithSeparator = $Root.TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar) + $separator
    return $Path.Equals($Root, [StringComparison]::OrdinalIgnoreCase) -or
        $Path.StartsWith($rootWithSeparator, [StringComparison]::OrdinalIgnoreCase)
}

function Add-ScanError {
    param(
        [System.Collections.Generic.List[object]]$Records,
        [ref]$TotalCount,
        [string]$Code,
        [string]$RelativePath,
        [string]$Message
    )

    $TotalCount.Value++
    if ($Records.Count -lt $MaxErrors) {
        $Records.Add([ordered]@{
            code = $Code
            relativePath = $RelativePath
            message = $Message
        })
    }
}

function Add-ReparseBoundary {
    param(
        [System.Collections.Generic.List[object]]$Records,
        [ref]$TotalCount,
        [string]$RelativePath,
        [bool]$IsDirectory
    )

    $TotalCount.Value++
    if ($Records.Count -lt $MaxErrors) {
        $Records.Add([ordered]@{
            relativePath = $RelativePath
            kind = if ($IsDirectory) { 'directory' } else { 'file' }
            reason = 'reparse_point_not_traversed_or_measured'
        })
    }
}

function Get-RelativeChildPath {
    param(
        [Parameter(Mandatory = $true)][string]$FullPath,
        [Parameter(Mandatory = $true)][string]$RootWithSeparator
    )
    return $FullPath.Substring($RootWithSeparator.Length).Replace(
        [IO.Path]::AltDirectorySeparatorChar,
        [IO.Path]::DirectorySeparatorChar)
}

function Get-DirectoryRollups {
    param([object[]]$Files)

    $rollups = @{}
    foreach ($file in $Files) {
        $directory = [IO.Path]::GetDirectoryName([string]$file.relativePath)
        while ($true) {
            $key = if ([string]::IsNullOrEmpty($directory)) { '.' } else { $directory }
            if (-not $rollups.ContainsKey($key)) {
                $rollups[$key] = [ordered]@{
                    relativePath = $key
                    observedFileCount = [long]0
                    observedBytes = [long]0
                }
            }
            $rollups[$key].observedFileCount++
            $rollups[$key].observedBytes += [long]$file.lengthBytes
            if ($key -eq '.') { break }
            $directory = [IO.Path]::GetDirectoryName($directory)
        }
    }

    $all = @($rollups.Values | Sort-Object `
        @{ Expression = { [long]$_.observedBytes }; Descending = $true }, `
        @{ Expression = { [string]$_.relativePath }; Descending = $false })
    return [ordered]@{
        totalCount = $all.Count
        records = @($all | Select-Object -First $MaxDirectoryRollups)
        truncated = $all.Count -gt $MaxDirectoryRollups
    }
}

function Get-OverwriteSnapshot {
    param([Parameter(Mandatory = $true)][string]$RootPath)

    $errors = [System.Collections.Generic.List[object]]::new()
    $errorCount = 0
    $boundaries = [System.Collections.Generic.List[object]]::new()
    $boundaryCount = 0
    $files = [System.Collections.Generic.List[object]]::new()
    $root = Get-FullPath -Path $RootPath

    if (-not [IO.Directory]::Exists($root)) {
        Add-ScanError -Records $errors -TotalCount ([ref]$errorCount) `
            -Code 'root_missing_or_not_directory' -RelativePath '.' `
            -Message 'The overwrite path was missing or was not a directory at scan time.'
        return [ordered]@{
            capturedAtUtc = [DateTimeOffset]::UtcNow.ToString('o')
            rootPath = $root
            measurementScope = 'regular non-reparse files beneath root'
            status = 'missing'
            totals = [ordered]@{
                complete = $false
                fileCount = $null
                totalBytes = $null
                observedFileCount = [long]0
                observedBytes = [long]0
            }
            files = @()
            directoryRollups = [ordered]@{ totalCount = 0; records = @(); truncated = $false }
            errors = [ordered]@{ totalCount = $errorCount; records = @($errors); truncated = $false }
            reparseBoundaries = [ordered]@{ totalCount = 0; records = @(); truncated = $false }
        }
    }

    try {
        $rootItem = Get-Item -Force -LiteralPath $root -ErrorAction Stop
        if (($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            Add-ScanError -Records $errors -TotalCount ([ref]$errorCount) `
                -Code 'root_is_reparse_point' -RelativePath '.' `
                -Message 'The overwrite root is a reparse point; the scan did not follow it.'
        }
    } catch {
        Add-ScanError -Records $errors -TotalCount ([ref]$errorCount) `
            -Code 'root_metadata_failed' -RelativePath '.' -Message $_.Exception.Message
    }

    if ($errorCount -gt 0) {
        return [ordered]@{
            capturedAtUtc = [DateTimeOffset]::UtcNow.ToString('o')
            rootPath = $root
            measurementScope = 'regular non-reparse files beneath root'
            status = 'incomplete'
            totals = [ordered]@{
                complete = $false
                fileCount = $null
                totalBytes = $null
                observedFileCount = [long]0
                observedBytes = [long]0
            }
            files = @()
            directoryRollups = [ordered]@{ totalCount = 0; records = @(); truncated = $false }
            errors = [ordered]@{
                totalCount = $errorCount
                records = @($errors)
                truncated = $errorCount -gt $errors.Count
            }
            reparseBoundaries = [ordered]@{ totalCount = 0; records = @(); truncated = $false }
        }
    }

    $rootWithSeparator = $root.TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $pending = [System.Collections.Generic.Queue[string]]::new()
    $pending.Enqueue($root)
    $limitReached = $false

    while ($pending.Count -gt 0 -and -not $limitReached) {
        $directory = $pending.Dequeue()
        try {
            $entries = @([IO.Directory]::EnumerateFileSystemEntries($directory))
        } catch {
            $relativeDirectory = if ($directory -eq $root) {
                '.'
            } else {
                Get-RelativeChildPath -FullPath $directory -RootWithSeparator $rootWithSeparator
            }
            Add-ScanError -Records $errors -TotalCount ([ref]$errorCount) `
                -Code 'directory_enumeration_failed' -RelativePath $relativeDirectory `
                -Message $_.Exception.Message
            continue
        }

        foreach ($entry in $entries) {
            $relativePath = Get-RelativeChildPath -FullPath $entry `
                -RootWithSeparator $rootWithSeparator
            try {
                $item = Get-Item -Force -LiteralPath $entry -ErrorAction Stop
                $isDirectory = $item -is [IO.DirectoryInfo]
                $isReparse = ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0
                if ($isReparse) {
                    Add-ReparseBoundary -Records $boundaries `
                        -TotalCount ([ref]$boundaryCount) -RelativePath $relativePath `
                        -IsDirectory $isDirectory
                    continue
                }
                if ($isDirectory) {
                    $pending.Enqueue($item.FullName)
                    continue
                }
                if (-not ($item -is [IO.FileInfo])) { continue }
                if ($files.Count -ge $MaxFiles) {
                    $limitReached = $true
                    Add-ScanError -Records $errors -TotalCount ([ref]$errorCount) `
                        -Code 'file_limit_reached' -RelativePath $relativePath `
                        -Message "The scan stopped at the configured MaxFiles limit of $MaxFiles."
                    break
                }
                $files.Add([ordered]@{
                    relativePath = $relativePath
                    lengthBytes = [long]$item.Length
                    lastWriteTimeUtc = ([DateTimeOffset]$item.LastWriteTimeUtc).ToString('o')
                })
            } catch {
                Add-ScanError -Records $errors -TotalCount ([ref]$errorCount) `
                    -Code 'entry_metadata_failed' -RelativePath $relativePath `
                    -Message $_.Exception.Message
            }
        }
    }

    $sortedFiles = @($files | Sort-Object { [string]$_.relativePath })
    $observedBytes = [long]0
    foreach ($file in $sortedFiles) { $observedBytes += [long]$file.lengthBytes }
    $complete = $errorCount -eq 0
    return [ordered]@{
        capturedAtUtc = [DateTimeOffset]::UtcNow.ToString('o')
        rootPath = $root
        measurementScope = 'regular non-reparse files beneath root'
        status = if ($complete) { 'complete' } else { 'incomplete' }
        totals = [ordered]@{
            complete = $complete
            fileCount = if ($complete) { [long]$sortedFiles.Count } else { $null }
            totalBytes = if ($complete) { $observedBytes } else { $null }
            observedFileCount = [long]$sortedFiles.Count
            observedBytes = $observedBytes
        }
        files = $sortedFiles
        directoryRollups = Get-DirectoryRollups -Files $sortedFiles
        errors = [ordered]@{
            totalCount = $errorCount
            records = @($errors)
            truncated = $errorCount -gt $errors.Count
        }
        reparseBoundaries = [ordered]@{
            totalCount = $boundaryCount
            records = @($boundaries)
            truncated = $boundaryCount -gt $boundaries.Count
        }
    }
}

function Get-FilenameSuspicion {
    param([string]$RelativePath)

    $labels = [System.Collections.Generic.List[string]]::new()
    $segments = @($RelativePath -split '[\\/]')
    foreach ($segment in $segments) {
        if ($segment -match '(?i)(^|[._-])osf([._-]|$)') { $labels.Add('osf_name_token') }
        if ($segment -match '(?i)(^|[._-])osfui([._-]|$)') {
            $labels.Add('osfui_name_token')
        }
        if ($segment -match '(?i)(^|[._-])webview2([._-]|$)') {
            $labels.Add('webview2_name_token')
        }
        if ($segment -match '(?i)(^|[._-])gpucache([._-]|$)') {
            $labels.Add('gpu_cache_name_token')
        }
        if ($segment -match '(?i)(^|[._-])code[ ._-]?cache([._-]|$)') {
            $labels.Add('code_cache_name_token')
        }
        if ($segment -match '(?i)(^|[._-])cache(d|s)?([._-]|$)') {
            $labels.Add('cache_name_token')
        }
    }
    return @($labels | Select-Object -Unique)
}

function ConvertTo-UtcTimestampText {
    param($Value)

    if ($null -eq $Value) { return $null }
    if ($Value -is [DateTimeOffset]) {
        return ([DateTimeOffset]$Value).ToUniversalTime().ToString('o')
    }
    if ($Value -is [DateTime]) {
        return ([DateTimeOffset]([DateTime]$Value)).ToUniversalTime().ToString('o')
    }
    return [DateTimeOffset]::Parse(
        [string]$Value,
        [Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime().ToString('o')
}

function Get-Comparison {
    param(
        [Parameter(Mandatory = $true)]$Before,
        [Parameter(Mandatory = $true)]$After
    )

    $reasons = [System.Collections.Generic.List[string]]::new()
    if (-not [bool]$Before.totals.complete) { $reasons.Add('baseline_scan_incomplete') }
    if (-not [bool]$After.totals.complete) { $reasons.Add('current_scan_incomplete') }
    if ([string]$Before.rootPath -ne [string]$After.rootPath) {
        $reasons.Add('overwrite_root_mismatch')
    }
    $comparable = $reasons.Count -eq 0

    $beforeByPath = @{}
    foreach ($file in @($Before.files)) { $beforeByPath[[string]$file.relativePath] = $file }
    $afterByPath = @{}
    foreach ($file in @($After.files)) { $afterByPath[[string]$file.relativePath] = $file }
    $changes = [System.Collections.Generic.List[object]]::new()

    foreach ($path in @($beforeByPath.Keys + $afterByPath.Keys | Sort-Object -Unique)) {
        $old = $beforeByPath[$path]
        $new = $afterByPath[$path]
        $kind = $null
        $delta = $null
        if ($null -eq $old) {
            $kind = 'added'
            $delta = [long]$new.lengthBytes
        } elseif ($null -eq $new) {
            $kind = 'removed'
            $delta = -[long]$old.lengthBytes
        } elseif ([long]$old.lengthBytes -ne [long]$new.lengthBytes -or
            (ConvertTo-UtcTimestampText $old.lastWriteTimeUtc) -ne
                (ConvertTo-UtcTimestampText $new.lastWriteTimeUtc)) {
            $kind = 'modified'
            $delta = [long]$new.lengthBytes - [long]$old.lengthBytes
        }
        if ($null -ne $kind) {
            $changes.Add([ordered]@{
                relativePath = [string]$path
                kind = $kind
                beforeLengthBytes = if ($null -eq $old) { $null } else { [long]$old.lengthBytes }
                afterLengthBytes = if ($null -eq $new) { $null } else { [long]$new.lengthBytes }
                deltaBytes = $delta
                beforeLastWriteTimeUtc = if ($null -eq $old) {
                    $null
                } else {
                    ConvertTo-UtcTimestampText $old.lastWriteTimeUtc
                }
                afterLastWriteTimeUtc = if ($null -eq $new) {
                    $null
                } else {
                    ConvertTo-UtcTimestampText $new.lastWriteTimeUtc
                }
                filenameSuspicion = @(Get-FilenameSuspicion -RelativePath ([string]$path))
            })
        }
    }

    $orderedChanges = @($changes | Sort-Object `
        @{ Expression = { [Math]::Abs([long]$_.deltaBytes) }; Descending = $true }, `
        @{ Expression = { [string]$_.relativePath }; Descending = $false })
    $growth = @($changes | Where-Object { [long]$_.deltaBytes -gt 0 } | Sort-Object `
        @{ Expression = { [long]$_.deltaBytes }; Descending = $true }, `
        @{ Expression = { [string]$_.relativePath }; Descending = $false } |
        Select-Object -First $MaxLargestGrowthFiles)
    $suspicious = @($orderedChanges | Where-Object { @($_.filenameSuspicion).Count -gt 0 } |
        Select-Object -First $MaxChangedFiles)
    $directoryChanges = @{}
    foreach ($change in $changes) {
        $directory = [IO.Path]::GetDirectoryName([string]$change.relativePath)
        while ($true) {
            $key = if ([string]::IsNullOrEmpty($directory)) { '.' } else { $directory }
            if (-not $directoryChanges.ContainsKey($key)) {
                $directoryChanges[$key] = [ordered]@{
                    relativePath = $key
                    changedFileCount = [long]0
                    addedFileCount = [long]0
                    modifiedFileCount = [long]0
                    removedFileCount = [long]0
                    deltaBytes = [long]0
                }
            }
            $rollup = $directoryChanges[$key]
            $rollup.changedFileCount++
            $rollup.deltaBytes += [long]$change.deltaBytes
            switch ([string]$change.kind) {
                'added' { $rollup.addedFileCount++ }
                'modified' { $rollup.modifiedFileCount++ }
                'removed' { $rollup.removedFileCount++ }
            }
            if ($key -eq '.') { break }
            $directory = [IO.Path]::GetDirectoryName($directory)
        }
    }
    $orderedDirectoryChanges = @($directoryChanges.Values | Sort-Object `
        @{ Expression = { [Math]::Abs([long]$_.deltaBytes) }; Descending = $true }, `
        @{ Expression = { [string]$_.relativePath }; Descending = $false })

    return [ordered]@{
        comparable = $comparable
        notComparableReasons = @($reasons)
        observedChangeCaveat = if ($comparable) {
            $null
        } else {
            'Changed-file records compare observed metadata only and are not authoritative totals.'
        }
        totalDelta = [ordered]@{
            fileCount = if ($comparable) {
                [long]$After.totals.fileCount - [long]$Before.totals.fileCount
            } else { $null }
            bytes = if ($comparable) {
                [long]$After.totals.totalBytes - [long]$Before.totals.totalBytes
            } else { $null }
        }
        changedFiles = [ordered]@{
            totalCount = $orderedChanges.Count
            records = @($orderedChanges | Select-Object -First $MaxChangedFiles)
            truncated = $orderedChanges.Count -gt $MaxChangedFiles
        }
        directoryChanges = [ordered]@{
            totalCount = $orderedDirectoryChanges.Count
            records = @($orderedDirectoryChanges | Select-Object -First $MaxDirectoryRollups)
            truncated = $orderedDirectoryChanges.Count -gt $MaxDirectoryRollups
        }
        largestGrowthFiles = [ordered]@{
            totalCount = @($changes | Where-Object { [long]$_.deltaBytes -gt 0 }).Count
            records = $growth
            truncated = @($changes | Where-Object { [long]$_.deltaBytes -gt 0 }).Count -gt
                $MaxLargestGrowthFiles
        }
        filenameSuspicion = [ordered]@{
            disclaimer = 'Filename/path tokens only; these matches do not identify a writer or establish cause.'
            totalCount = @($orderedChanges | Where-Object {
                @($_.filenameSuspicion).Count -gt 0
            }).Count
            records = $suspicious
            truncated = @($orderedChanges | Where-Object {
                @($_.filenameSuspicion).Count -gt 0
            }).Count -gt $MaxChangedFiles
        }
    }
}

$resolvedOverwrite = Get-FullPath -Path $OverwritePath
$resolvedOutput = Get-FullPath -Path $OutputPath
if (Test-PathInsideRoot -Path $resolvedOutput -Root $resolvedOverwrite) {
    throw 'OutputPath must be outside OverwritePath so the diagnostic does not measure itself.'
}
if ($Mode -eq 'Compare' -and [string]::IsNullOrWhiteSpace($BaselinePath)) {
    throw 'BaselinePath is required in Compare mode.'
}
if ($Mode -eq 'Compare') {
    $candidateBaseline = Get-FullPath -Path $BaselinePath
    if (Test-PathInsideRoot -Path $candidateBaseline -Root $resolvedOverwrite) {
        throw 'BaselinePath must be outside OverwritePath so the diagnostic does not measure itself.'
    }
}

$current = Get-OverwriteSnapshot -RootPath $resolvedOverwrite
$artifact = [ordered]@{
    schemaVersion = 1
    kind = if ($Mode -eq 'Snapshot') {
        'overwrite-run-snapshot'
    } else {
        'overwrite-run-comparison'
    }
    generatedAtUtc = [DateTimeOffset]::UtcNow.ToString('o')
    limits = [ordered]@{
        maxFiles = $MaxFiles
        maxErrors = $MaxErrors
        maxDirectoryRollups = $MaxDirectoryRollups
        maxChangedFiles = $MaxChangedFiles
        maxLargestGrowthFiles = $MaxLargestGrowthFiles
    }
    currentSnapshot = $current
}

if ($Mode -eq 'Compare') {
    $resolvedBaseline = Get-FullPath -Path $BaselinePath
    if (-not [IO.File]::Exists($resolvedBaseline)) {
        throw "BaselinePath does not exist: $resolvedBaseline"
    }
    $baselineArtifact = Get-Content -Raw -LiteralPath $resolvedBaseline | ConvertFrom-Json
    if ([string]$baselineArtifact.kind -ne 'overwrite-run-snapshot') {
        throw 'BaselinePath must contain an overwrite-run-snapshot artifact.'
    }
    $artifact.baselinePath = $resolvedBaseline
    $artifact.baselineCapturedAtUtc = [string]$baselineArtifact.currentSnapshot.capturedAtUtc
    $artifact.comparison = Get-Comparison `
        -Before $baselineArtifact.currentSnapshot -After $current
}

$outputDirectory = Split-Path -Parent $resolvedOutput
if (-not [string]::IsNullOrEmpty($outputDirectory)) {
    [IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
}
$temporaryOutput = "$resolvedOutput.tmp"
[IO.File]::WriteAllText(
    $temporaryOutput,
    (($artifact | ConvertTo-Json -Depth 12) + [Environment]::NewLine),
    [Text.UTF8Encoding]::new($false))
Move-Item -LiteralPath $temporaryOutput -Destination $resolvedOutput -Force

$artifact
