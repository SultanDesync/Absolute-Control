[CmdletBinding()]
param(
    [string]$RunDirectory,
    [ValidateRange(1, 20)]
    [int]$CycleCount = 3,
    [ValidateRange(250, 10000)]
    [int]$SettleMilliseconds = 1000,
    [switch]$SkipWatchdogWait,
    [switch]$CaptureOnly,
    [ValidateRange(0, 8192)]
    [int]$CaptureWidth = 0
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class AbsoluteControlPanelPauseCapture
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left; public int Top; public int Right; public int Bottom; }

    [DllImport("user32.dll")]
    private static extern bool GetWindowRect(IntPtr window, out Rect rect);

    [DllImport("user32.dll")]
    private static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);

    public static void Save(IntPtr window, string path)
    {
        Save(window, path, 0);
    }

    public static void Save(IntPtr window, string path, int maximumWidth)
    {
        Rect bounds;
        if (!GetWindowRect(window, out bounds))
            throw new InvalidOperationException("Could not read the Starfield window bounds.");
        var width = bounds.Right - bounds.Left;
        var height = bounds.Bottom - bounds.Top;
        if (width <= 0 || height <= 0)
            throw new InvalidOperationException("Starfield has invalid window bounds.");

        using (var bitmap = new Bitmap(width, height))
        using (var graphics = Graphics.FromImage(bitmap))
        {
            var deviceContext = graphics.GetHdc();
            bool captured;
            try { captured = PrintWindow(window, deviceContext, 2); }
            finally { graphics.ReleaseHdc(deviceContext); }
            if (!captured)
                graphics.CopyFromScreen(bounds.Left, bounds.Top, 0, 0, bitmap.Size);
            if (maximumWidth > 0 && bitmap.Width > maximumWidth)
            {
                var scaledHeight = Math.Max(1,
                    (int)Math.Round(bitmap.Height * (maximumWidth / (double)bitmap.Width)));
                using (var scaled = new Bitmap(maximumWidth, scaledHeight))
                using (var scaledGraphics = Graphics.FromImage(scaled))
                {
                    scaledGraphics.InterpolationMode =
                        System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
                    scaledGraphics.DrawImage(bitmap, 0, 0, maximumWidth, scaledHeight);
                    scaled.Save(path, ImageFormat.Png);
                }
            }
            else
            {
                bitmap.Save(path, ImageFormat.Png);
            }
        }
    }
}
'@

function Wait-ForEvidence {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Event,
        [string]$DetailContains = '',
        [int]$TimeoutSeconds = 15
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $script:Starfield.Refresh()
        if ($script:Starfield.HasExited) {
            throw 'Starfield exited during the pause-cycle observation.'
        }
        if (Test-Path -LiteralPath $script:PluginEvidencePath) {
            $record = Get-Content -LiteralPath $script:PluginEvidencePath |
                ForEach-Object {
                    try { $_ | ConvertFrom-Json } catch { $null }
                } | Where-Object {
                    $null -ne $_ -and $_.run_id -eq $script:RunId -and
                    $_.event -eq $Event -and
                    ([string]::IsNullOrEmpty($DetailContains) -or
                        ([string]$_.detail).Contains($DetailContains))
                } | Select-Object -Last 1
            if ($null -ne $record) { return $record }
        }
        Start-Sleep -Milliseconds 100
    }
    throw (
        "Timed out waiting for plugin evidence: event=$Event " +
        "detail_contains=$DetailContains")
}

function Invoke-PauseToggle {
    $commandId = $script:NextCommandId
    $script:NextCommandId++
    $temporaryPath = "$script:InputPath.tmp"
    [System.IO.File]::WriteAllLines(
        $temporaryPath,
        [string[]]@("id=$commandId", 'command=pause'),
        [System.Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporaryPath -Destination $script:InputPath -Force

    $keyRecord = Wait-ForEvidence -Event 'research_input_key_up' `
        -DetailContains "id=$commandId command=pause"
    if (-not ([string]$keyRecord.detail).Contains('sent=1 error=0')) {
        throw "Game-thread Escape pulse failed: $($keyRecord.detail)"
    }

    $stateRecord = Wait-ForEvidence -Event 'research_pause_state' `
        -DetailContains "id=$commandId open="
    $stateDetail = [string]$stateRecord.detail
    if ($stateDetail.Contains("id=$commandId open=true")) { return $true }
    if ($stateDetail.Contains("id=$commandId open=false")) { return $false }
    throw "PauseMenu state was malformed: $stateDetail"
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($RunDirectory)) {
    $RunDirectory = Get-ChildItem (Join-Path $repositoryRoot 'artifacts\research-runs') `
        -Directory | Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
$resolvedRunDirectory = (Resolve-Path -LiteralPath $RunDirectory).Path
$script:RunId = Split-Path -Leaf $resolvedRunDirectory
$manifest = Get-Content -Raw -LiteralPath (
    Join-Path $resolvedRunDirectory 'manifest.json') | ConvertFrom-Json
$script:Starfield = Get-Process -Name $manifest.processName -ErrorAction Stop |
    Select-Object -First 1
$script:Starfield.Refresh()
if ($script:Starfield.MainWindowHandle -eq [IntPtr]::Zero) {
    throw 'Starfield has no capturable main window.'
}

$cycleDirectory = Join-Path $resolvedRunDirectory 'pause-cycles'
New-Item -ItemType Directory -Force -Path $cycleDirectory | Out-Null

if ($CaptureOnly) {
    # This branch intentionally returns before resolving or writing the input
    # mailbox. Capture uses the existing window handle without focus changes.
    $capturePath = Join-Path $cycleDirectory (
        'capture-only-{0}.png' -f
        [DateTimeOffset]::UtcNow.ToString('yyyyMMdd-HHmmss-fffffff'))
    [AbsoluteControlPanelPauseCapture]::Save(
        $script:Starfield.MainWindowHandle, $capturePath, $CaptureWidth)
    [pscustomobject]@{
        mode = 'capture_only'
        run_id = $script:RunId
        screenshot = $capturePath
    }
    return
}

$documents = [Environment]::GetFolderPath('MyDocuments')
$script:PluginEvidencePath = Join-Path $documents (
    'My Games\Starfield\SFSE\Logs\AbsoluteControlPanel.evidence.jsonl')
$script:InputPath = Join-Path (Split-Path -Parent $script:PluginEvidencePath) (
    "AbsoluteControlPanelResearch.$($script:RunId).input")

$maximumCommandId = 0
if (Test-Path -LiteralPath $script:PluginEvidencePath) {
    foreach ($line in Get-Content -LiteralPath $script:PluginEvidencePath) {
        try { $record = $line | ConvertFrom-Json } catch { $record = $null }
        if ($null -ne $record -and $record.run_id -eq $script:RunId -and
            ([string]$record.detail) -match '^id=(\d+)') {
            $maximumCommandId = [Math]::Max(
                $maximumCommandId, [int]$Matches[1])
        }
    }
}
$script:NextCommandId = $maximumCommandId + 1

# The visual probe may still be on top when run-probe returns.  Its fixed watchdog
# tears it down before the PauseMenu cycle begins.
if (-not $SkipWatchdogWait) {
    [void](Wait-ForEvidence -Event 'watchdog_fired' -TimeoutSeconds 30)
}

$isOpen = Invoke-PauseToggle
if ($isOpen) {
    $isOpen = Invoke-PauseToggle
}
if ($isOpen) {
    throw 'Could not normalize PauseMenu to the closed state.'
}

$records = @()
for ($cycle = 1; $cycle -le $CycleCount; $cycle++) {
    if (-not (Invoke-PauseToggle)) {
        throw "PauseMenu did not open during cycle $cycle."
    }
    Start-Sleep -Milliseconds $SettleMilliseconds
    $openPath = Join-Path $cycleDirectory ('cycle-{0:D2}-open.png' -f $cycle)
    [AbsoluteControlPanelPauseCapture]::Save(
        $script:Starfield.MainWindowHandle, $openPath)
    try { [Console]::Beep(880, 100) } catch {}

    if (Invoke-PauseToggle) {
        throw "PauseMenu did not close during cycle $cycle."
    }
    Start-Sleep -Milliseconds $SettleMilliseconds
    $closedPath = Join-Path $cycleDirectory ('cycle-{0:D2}-closed.png' -f $cycle)
    [AbsoluteControlPanelPauseCapture]::Save(
        $script:Starfield.MainWindowHandle, $closedPath)
    try { [Console]::Beep(660, 100) } catch {}

    $records += [ordered]@{
        cycle = $cycle
        opened = $true
        closed = $true
        open_screenshot = $openPath
        closed_screenshot = $closedPath
    }
}

[System.IO.File]::WriteAllText(
    (Join-Path $cycleDirectory 'pause-cycles.json'),
    (($records | ConvertTo-Json -Depth 4) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false))

Write-Host "Completed $CycleCount PauseMenu build/teardown cycles in the retained Starfield session."
Write-Host "Evidence: $cycleDirectory"
