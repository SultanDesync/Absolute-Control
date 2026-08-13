[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ManifestPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public static class AbsoluteControlPanelWindow
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left; public int Top; public int Right; public int Bottom; }

    public sealed class WindowInfo
    {
        public IntPtr Handle { get; set; }
        public string Title { get; set; }
        public string ClassName { get; set; }
        public Rect Bounds { get; set; }
    }

    public sealed class PixelSignal
    {
        public long PixelCount { get; set; }
        public int MinimumX { get; set; }
        public int MinimumY { get; set; }
        public int MaximumX { get; set; }
        public int MaximumY { get; set; }
        public int ImageWidth { get; set; }
        public int ImageHeight { get; set; }

        public PixelSignal()
        {
            MinimumX = -1;
            MinimumY = -1;
            MaximumX = -1;
            MaximumY = -1;
        }
    }

    private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out Rect rect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr window);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowTextLength(IntPtr window);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr window, StringBuilder text, int maximumCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr window, StringBuilder className, int maximumCount);

    public static WindowInfo[] GetProcessWindows(int expectedProcessId)
    {
        var windows = new List<WindowInfo>();
        EnumWindows(delegate(IntPtr window, IntPtr parameter)
        {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId != expectedProcessId || !IsWindowVisible(window))
                return true;

            var title = new StringBuilder(GetWindowTextLength(window) + 1);
            GetWindowText(window, title, title.Capacity);
            var className = new StringBuilder(256);
            GetClassName(window, className, className.Capacity);
            Rect bounds;
            GetWindowRect(window, out bounds);
            windows.Add(new WindowInfo {
                Handle = window,
                Title = title.ToString(),
                ClassName = className.ToString(),
                Bounds = bounds
            });
            return true;
        }, IntPtr.Zero);
        return windows.ToArray();
    }

    public static PixelSignal AnalyzeMagenta(
        string imagePath, byte minimumRed, byte maximumGreen, byte minimumBlue)
    {
        using (var source = new Bitmap(imagePath))
        using (var bitmap = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb))
        using (var graphics = Graphics.FromImage(bitmap))
        {
            graphics.DrawImageUnscaled(source, 0, 0);
            var result = new PixelSignal {
                ImageWidth = bitmap.Width,
                ImageHeight = bitmap.Height
            };
            var bounds = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
            var data = bitmap.LockBits(bounds, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            try
            {
                var bytes = new byte[Math.Abs(data.Stride) * data.Height];
                Marshal.Copy(data.Scan0, bytes, 0, bytes.Length);
                for (var y = 0; y < data.Height; y++)
                {
                    var row = data.Stride >= 0 ? y * data.Stride :
                        (data.Height - 1 - y) * -data.Stride;
                    for (var x = 0; x < data.Width; x++)
                    {
                        var pixel = row + x * 4;
                        var blue = bytes[pixel];
                        var green = bytes[pixel + 1];
                        var red = bytes[pixel + 2];
                        if (red < minimumRed || green > maximumGreen || blue < minimumBlue)
                            continue;

                        result.PixelCount++;
                        if (result.MinimumX < 0 || x < result.MinimumX) result.MinimumX = x;
                        if (result.MinimumY < 0 || y < result.MinimumY) result.MinimumY = y;
                        if (x > result.MaximumX) result.MaximumX = x;
                        if (y > result.MaximumY) result.MaximumY = y;
                    }
                }
            }
            finally
            {
                bitmap.UnlockBits(data);
            }
            return result;
        }
    }

    public static PixelSignal AnalyzeBrightFooter(string imagePath)
    {
        using (var source = new Bitmap(imagePath))
        using (var bitmap = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb))
        using (var graphics = Graphics.FromImage(bitmap))
        {
            graphics.DrawImageUnscaled(source, 0, 0);
            var result = new PixelSignal {
                ImageWidth = bitmap.Width,
                ImageHeight = bitmap.Height
            };
            var minimumX = (int)(bitmap.Width * 0.35);
            var maximumX = (int)(bitmap.Width * 0.65);
            var minimumY = (int)(bitmap.Height * 0.88);
            var maximumY = (int)(bitmap.Height * 0.97);
            var bounds = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
            var data = bitmap.LockBits(bounds, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            try
            {
                var bytes = new byte[Math.Abs(data.Stride) * data.Height];
                Marshal.Copy(data.Scan0, bytes, 0, bytes.Length);
                for (var y = minimumY; y < maximumY; y++)
                {
                    var row = data.Stride >= 0 ? y * data.Stride :
                        (data.Height - 1 - y) * -data.Stride;
                    for (var x = minimumX; x < maximumX; x++)
                    {
                        var pixel = row + x * 4;
                        var blue = bytes[pixel];
                        var green = bytes[pixel + 1];
                        var red = bytes[pixel + 2];
                        if (red < 220 || green < 220 || blue < 220)
                            continue;

                        result.PixelCount++;
                        if (result.MinimumX < 0 || x < result.MinimumX) result.MinimumX = x;
                        if (result.MinimumY < 0 || y < result.MinimumY) result.MinimumY = y;
                        if (x > result.MaximumX) result.MaximumX = x;
                        if (y > result.MaximumY) result.MaximumY = y;
                    }
                }
            }
            finally
            {
                bitmap.UnlockBits(data);
            }
            return result;
        }
    }

    public static bool IsTitlePromptSignal(PixelSignal signal)
    {
        if (signal == null || signal.PixelCount < 1500 || signal.MinimumX < 0)
            return false;
        var width = signal.MaximumX - signal.MinimumX + 1;
        var height = signal.MaximumY - signal.MinimumY + 1;
        return width >= (int)(signal.ImageWidth * 0.10) &&
            height >= 12 && height <= (int)(signal.ImageHeight * 0.06);
    }

    public static double AnalyzeMeanLuminance(
        string imagePath, double centerXRatio, double centerYRatio,
        double halfWidthRatio, double halfHeightRatio)
    {
        using (var bitmap = new Bitmap(imagePath))
        {
            var centerX = (int)(bitmap.Width * centerXRatio);
            var centerY = (int)(bitmap.Height * centerYRatio);
            var halfWidth = Math.Max(1, (int)(bitmap.Width * halfWidthRatio));
            var halfHeight = Math.Max(1, (int)(bitmap.Height * halfHeightRatio));
            var minimumX = Math.Max(0, centerX - halfWidth);
            var maximumX = Math.Min(bitmap.Width - 1, centerX + halfWidth);
            var minimumY = Math.Max(0, centerY - halfHeight);
            var maximumY = Math.Min(bitmap.Height - 1, centerY + halfHeight);
            double total = 0;
            long count = 0;
            for (var y = minimumY; y <= maximumY; y++)
            {
                for (var x = minimumX; x <= maximumX; x++)
                {
                    var color = bitmap.GetPixel(x, y);
                    total += 0.2126 * color.R + 0.7152 * color.G + 0.0722 * color.B;
                    count++;
                }
            }
            return count == 0 ? 0 : total / count;
        }
    }

}
'@

function Write-RunnerEvent {
    param([string]$Event, [string]$Detail = '')
    $record = [ordered]@{
        timestamp = [DateTimeOffset]::UtcNow.ToString('o')
        event = $Event
        detail = $Detail
    }
    Add-Content -LiteralPath $script:RunnerEventsPath -Encoding UTF8 -Value (
        $record | ConvertTo-Json -Compress)
}

function Save-WindowHandleScreenshot {
    param(
        [AbsoluteControlPanelWindow+WindowInfo]$Window,
        [string]$Path
    )

    $width = $Window.Bounds.Right - $Window.Bounds.Left
    $height = $Window.Bounds.Bottom - $Window.Bounds.Top
    if ($width -le 0 -or $height -le 0) {
        return $false
    }

    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $deviceContext = $graphics.GetHdc()
        try {
            $captured = [AbsoluteControlPanelWindow]::PrintWindow(
                $Window.Handle, $deviceContext, 2)
        } finally {
            $graphics.ReleaseHdc($deviceContext)
        }
        if (-not $captured) {
            $graphics.CopyFromScreen(
                $Window.Bounds.Left, $Window.Bounds.Top, 0, 0, $bitmap.Size)
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        return $true
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Save-DiagnosticBundle {
    param([System.Diagnostics.Process]$Process, [string]$Reason)

    if ($script:DiagnosticBundleCaptured -or $null -eq $Process -or $Process.HasExited) {
        return
    }

    $windows = [AbsoluteControlPanelWindow]::GetProcessWindows($Process.Id)
    $metadata = @()
    for ($index = 0; $index -lt $windows.Count; $index++) {
        $window = $windows[$index]
        $metadata += [ordered]@{
            index = $index
            handle = ('0x{0:X}' -f $window.Handle.ToInt64())
            title = $window.Title
            class_name = $window.ClassName
            left = $window.Bounds.Left
            top = $window.Bounds.Top
            right = $window.Bounds.Right
            bottom = $window.Bounds.Bottom
        }
        $imagePath = Join-Path $script:RunDirectory (
            'diagnostic-window-{0:D2}.png' -f $index)
        if (Save-WindowHandleScreenshot -Window $window -Path $imagePath) {
            Write-RunnerEvent 'diagnostic_screenshot_captured' (
                "reason=$Reason index=$index title=$($window.Title) path=$imagePath")
        }
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $script:RunDirectory 'diagnostic-windows.json'),
        ($metadata | ConvertTo-Json -Depth 4),
        [System.Text.UTF8Encoding]::new($false))
    $script:DiagnosticBundleCaptured = $true
}

function Wait-ForCondition {
    param(
        [scriptblock]$Condition,
        [int]$TimeoutSeconds,
        [string]$Description,
        [System.Diagnostics.Process]$Process
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($null -ne $Process) {
            $Process.Refresh()
            if ($Process.HasExited) {
                throw "Starfield exited while waiting for $Description."
            }
            $dialogs = @([AbsoluteControlPanelWindow]::GetProcessWindows($Process.Id) |
                Where-Object { $_.ClassName -eq '#32770' })
            if ($dialogs.Count -gt 0) {
                Save-DiagnosticBundle -Process $Process -Reason 'dialog_detected'
                $titles = ($dialogs | ForEach-Object Title) -join '; '
                throw "Starfield opened a diagnostic dialog while waiting for ${Description}: $titles"
            }
        }
        $value = & $Condition
        if ($null -ne $value -and $value -ne $false) {
            return $value
        }
        Start-Sleep -Milliseconds 500
    }
    throw "Timed out waiting for $Description after $TimeoutSeconds seconds."
}

function Save-WindowScreenshot {
    param([System.Diagnostics.Process]$Process, [string]$Path)
    $Process.Refresh()
    if ($Process.MainWindowHandle -eq [IntPtr]::Zero) {
        throw 'Starfield has no capturable main window yet.'
    }

    $windows = [AbsoluteControlPanelWindow]::GetProcessWindows($Process.Id)
    $window = $windows | Where-Object {
        $_.Handle -eq $Process.MainWindowHandle
    } | Select-Object -First 1
    if ($null -eq $window) {
        $window = $windows | Where-Object { $_.Title -eq 'Starfield' } |
            Select-Object -First 1
    }
    if ($null -eq $window -or -not (Save-WindowHandleScreenshot -Window $window -Path $Path)) {
        throw 'Could not capture the Starfield main window.'
    }
}

function Get-SentinelSignal {
    param([string]$Path)

    return [AbsoluteControlPanelWindow]::AnalyzeMagenta(
        $Path,
        [byte]$script:SentinelRedMinimum,
        [byte]$script:SentinelGreenMaximum,
        [byte]$script:SentinelBlueMinimum)
}

function Get-StarfieldMainWindow {
    param([System.Diagnostics.Process]$Process)

    $Process.Refresh()
    $windows = @([AbsoluteControlPanelWindow]::GetProcessWindows($Process.Id))
    $window = $windows | Where-Object {
        $_.Handle -eq $Process.MainWindowHandle
    } | Select-Object -First 1
    if ($null -eq $window) {
        $window = $windows | Where-Object { $_.Title -eq 'Starfield' } |
            Select-Object -First 1
    }
    return $window
}

function Get-TitlePromptSignal {
    param([string]$Path)
    return [AbsoluteControlPanelWindow]::AnalyzeBrightFooter($Path)
}

function Write-SentinelSignal {
    param(
        [AbsoluteControlPanelWindow+PixelSignal]$Signal,
        [string]$Path,
        [string]$Event
    )

    $present = $Signal.PixelCount -ge $script:SentinelMinimumPixels
    $record = [ordered]@{
        present = $present
        pixel_count = $Signal.PixelCount
        minimum_pixels = $script:SentinelMinimumPixels
        red_minimum = $script:SentinelRedMinimum
        green_maximum = $script:SentinelGreenMaximum
        blue_minimum = $script:SentinelBlueMinimum
        bounds = [ordered]@{
            minimum_x = $Signal.MinimumX
            minimum_y = $Signal.MinimumY
            maximum_x = $Signal.MaximumX
            maximum_y = $Signal.MaximumY
        }
        image_width = $Signal.ImageWidth
        image_height = $Signal.ImageHeight
        screenshot = $Path
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $script:RunDirectory 'sentinel-signal.json'),
        ($record | ConvertTo-Json -Depth 4),
        [System.Text.UTF8Encoding]::new($false))
    Write-RunnerEvent $Event (
        "present=$present pixels=$($Signal.PixelCount) " +
        "bounds=$($Signal.MinimumX),$($Signal.MinimumY)-$($Signal.MaximumX),$($Signal.MaximumY)")
    return $present
}

function Invoke-ResearchInput {
    param(
        [uint32]$CommandId,
        [string]$Command,
        [System.Diagnostics.Process]$Process
    )

    $temporaryPath = "$script:InputPath.tmp"
    [System.IO.File]::WriteAllLines(
        $temporaryPath,
        [string[]]@("id=$CommandId", "command=$Command"),
        [System.Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporaryPath -Destination $script:InputPath -Force
    Write-RunnerEvent 'research_input_requested' "id=$CommandId command=$Command"

    $marker = '"run_id":"{0}","event":"research_input_key_up","detail":"id={1} command={2}' -f `
        $script:RunId, $CommandId, $Command
    $eventLine = Wait-ForCondition -TimeoutSeconds 15 `
        -Description "game-task input acknowledgement $CommandId/$Command" -Process $Process `
        -Condition {
            if (-not (Test-Path -LiteralPath $script:PluginEvidencePath)) {
                return $false
            }
            $line = Get-Content -LiteralPath $script:PluginEvidencePath |
                Where-Object { $_.Contains($marker) } | Select-Object -Last 1
            if ($null -ne $line) { return $line }
            return $false
        }
    if (-not $eventLine.Contains('sent=1 error=0')) {
        throw "Research input failed: $eventLine"
    }
    Write-RunnerEvent 'research_input_acknowledged' "id=$CommandId command=$Command"
}

function Wait-ForPluginEvent {
    param(
        [string]$Event,
        [string]$DetailContains,
        [int]$TimeoutSeconds,
        [System.Diagnostics.Process]$Process
    )

    $eventMarker = '"run_id":"{0}","event":"{1}"' -f $script:RunId, $Event
    return Wait-ForCondition -TimeoutSeconds $TimeoutSeconds `
        -Description "plugin event $Event" -Process $Process -Condition {
            if (-not (Test-Path -LiteralPath $script:PluginEvidencePath)) {
                return $false
            }
            $line = Get-Content -LiteralPath $script:PluginEvidencePath |
                Where-Object {
                    $_.Contains($eventMarker) -and
                    ([string]::IsNullOrEmpty($DetailContains) -or
                        $_.Contains($DetailContains))
                } | Select-Object -Last 1
            if ($null -ne $line) { return $line }
            return $false
        }
}

function Invoke-VJoyButtonPulse {
    param(
        [uint32]$DeviceId = 1,
        [uint32]$Button = 1
    )

    $programFiles = [Environment]::GetFolderPath('ProgramFiles')
    $nativeCandidates = @(
        (Join-Path $programFiles 'vJoy\x64\vJoyInterface.dll'),
        (Join-Path $programFiles 'vJoy\SDK\lib\amd64\vJoyInterface.dll')
    )
    $nativePath = $nativeCandidates |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
    if ($null -eq $nativePath) {
        throw 'The vJoy 64-bit native interface was not found.'
    }
    if ($null -eq ('AbsoluteControlPanelVJoy' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class AbsoluteControlPanelVJoy
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetDllDirectory(string path);

    [DllImport("vJoyInterface.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool vJoyEnabled();

    [DllImport("vJoyInterface.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetVJDStatus(uint deviceId);

    [DllImport("vJoyInterface.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool AcquireVJD(uint deviceId);

    [DllImport("vJoyInterface.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void RelinquishVJD(uint deviceId);

    [DllImport("vJoyInterface.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ResetVJD(uint deviceId);

    [DllImport("vJoyInterface.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetBtn(
        [MarshalAs(UnmanagedType.Bool)] bool pressed, uint deviceId, byte button);
}
'@
    }
    if (-not [AbsoluteControlPanelVJoy]::SetDllDirectory(
            (Split-Path -Parent $nativePath))) {
        throw 'Could not configure the vJoy native-library search path.'
    }
    if (-not [AbsoluteControlPanelVJoy]::vJoyEnabled()) {
        throw 'The vJoy driver is not enabled.'
    }
    $status = [AbsoluteControlPanelVJoy]::GetVJDStatus($DeviceId)
    if ($status -eq 2 -or $status -eq 3 -or $status -eq 4) {
        throw "vJoy device $DeviceId cannot be driven: $status"
    }

    $acquired = $status -eq 0
    $acquiredHere = $false
    if (-not $acquired) {
        $acquired = [AbsoluteControlPanelVJoy]::AcquireVJD($DeviceId)
        $acquiredHere = $acquired
    }
    if (-not $acquired) {
        throw "Could not acquire vJoy device $DeviceId."
    }

    try {
        if (-not [AbsoluteControlPanelVJoy]::ResetVJD($DeviceId)) {
            throw "Could not reset vJoy device $DeviceId."
        }
        [void][AbsoluteControlPanelVJoy]::SetBtn($false, $DeviceId, [byte]$Button)
        Start-Sleep -Milliseconds 300
        if (-not [AbsoluteControlPanelVJoy]::SetBtn(
                $true, $DeviceId, [byte]$Button)) {
            throw "Could not press vJoy device $DeviceId button $Button."
        }
        Write-RunnerEvent 'vjoy_button_pressed' (
            "device=$DeviceId button=$Button")
        Start-Sleep -Milliseconds 500
        if (-not [AbsoluteControlPanelVJoy]::SetBtn(
                $false, $DeviceId, [byte]$Button)) {
            throw "Could not release vJoy device $DeviceId button $Button."
        }
        Write-RunnerEvent 'vjoy_button_released' (
            "device=$DeviceId button=$Button")
    } finally {
        [void][AbsoluteControlPanelVJoy]::SetBtn($false, $DeviceId, [byte]$Button)
        if ($acquiredHere) {
            [AbsoluteControlPanelVJoy]::RelinquishVJD($DeviceId)
        }
    }
}

function Invoke-StepTone {
    param([int]$Frequency)
    try {
        [Console]::Beep($Frequency, 120)
    } catch {
        Write-RunnerEvent 'step_tone_unavailable' $_.Exception.Message
    }
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$resolvedManifest = (Resolve-Path -LiteralPath $ManifestPath).Path
$manifest = Get-Content -Raw -LiteralPath $resolvedManifest | ConvertFrom-Json
$runId = '{0}-{1}' -f $manifest.name, [DateTimeOffset]::UtcNow.ToString('yyyyMMdd-HHmmss')
$runDirectory = Join-Path $repositoryRoot (Join-Path 'artifacts\research-runs' $runId)
New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null
$script:RunDirectory = $runDirectory
$script:RunId = $runId
$script:RunnerEventsPath = Join-Path $runDirectory 'runner-events.jsonl'
$script:DiagnosticBundleCaptured = $false
$script:SentinelRedMinimum = if ($null -ne $manifest.sentinelRedMinimum) {
    [int]$manifest.sentinelRedMinimum
} else { 240 }
$script:SentinelGreenMaximum = if ($null -ne $manifest.sentinelGreenMaximum) {
    [int]$manifest.sentinelGreenMaximum
} else { 20 }
$script:SentinelBlueMinimum = if ($null -ne $manifest.sentinelBlueMinimum) {
    [int]$manifest.sentinelBlueMinimum
} else { 240 }
$script:SentinelMinimumPixels = if ($null -ne $manifest.sentinelMinimumPixels) {
    [int]$manifest.sentinelMinimumPixels
} else { 512 }
$titleScreenTimeoutSeconds = if ($null -ne $manifest.titleScreenTimeoutSeconds) {
    [int]$manifest.titleScreenTimeoutSeconds
} else { 120 }
$titleTransitionTimeoutSeconds = if ($null -ne $manifest.titleTransitionTimeoutSeconds) {
    [int]$manifest.titleTransitionTimeoutSeconds
} else { 45 }
$script:SentinelPollMilliseconds = if ($null -ne $manifest.sentinelPollMilliseconds) {
    [int]$manifest.sentinelPollMilliseconds
} else { 1000 }
$keepGameRunning = $null -ne $manifest.keepGameRunning -and
    [bool]$manifest.keepGameRunning

Copy-Item -LiteralPath $resolvedManifest -Destination (Join-Path $runDirectory 'manifest.json')
Write-RunnerEvent 'run_created' "run_id=$runId"

if ($manifest.addressLibraryFile) {
    if (-not (Test-Path -LiteralPath $manifest.addressLibraryFile -PathType Leaf)) {
        throw "Address Library preflight failed: missing $($manifest.addressLibraryFile)"
    }
    $addressLibraryHash =
        (Get-FileHash -Algorithm SHA256 -LiteralPath $manifest.addressLibraryFile).Hash
    Write-RunnerEvent 'address_library_present' (
        "path=$($manifest.addressLibraryFile) sha256=$addressLibraryHash")
}

if ($manifest.profileModListPath) {
    $profile = & (Join-Path $PSScriptRoot 'prepare-test-profile.ps1') `
        -ManifestPath $resolvedManifest
    if ($LASTEXITCODE -ne 0 -or -not $profile.ready) {
        throw 'MO2 profile preparation failed.'
    }
    Write-RunnerEvent 'mo2_profile_ready' (
        "modlist=$($profile.profileModListPath) required=$($profile.requiredMods -join ',')")
}

$starfield = $null
$pluginEvidencePath = $null
$armPath = $null
$advancePath = $null
$inputPath = $null
try {
    & (Join-Path $PSScriptRoot 'deploy-probe.ps1') `
        -ModPath $manifest.modPath `
        -RunId $runId `
        -RequireArm `
        -AdvanceTitleWithSendInput `
        -ArmTimeoutMilliseconds (($titleScreenTimeoutSeconds +
            $titleTransitionTimeoutSeconds + 60) * 1000) `
        -OpenDelayMilliseconds $manifest.openDelayMilliseconds `
        -VisibleMilliseconds $manifest.visibleMilliseconds `
        -MenuFlags $manifest.menuFlags
    Write-RunnerEvent 'deploy_complete' "mod_path=$($manifest.modPath)"

    $documents = [Environment]::GetFolderPath('MyDocuments')
    $pluginEvidencePath = Join-Path $documents (
        'My Games\Starfield\SFSE\Logs\AbsoluteControlPanelResearch.evidence.jsonl')
    $script:PluginEvidencePath = $pluginEvidencePath
    Write-RunnerEvent 'launch_requested' "shortcut=$($manifest.shortcut)"
    Start-Process -FilePath $manifest.shortcut | Out-Null

    $starfield = Wait-ForCondition -TimeoutSeconds $manifest.launchTimeoutSeconds `
        -Description 'Starfield process' -Condition {
            Get-Process -Name $manifest.processName -ErrorAction SilentlyContinue |
                Select-Object -First 1
        }
    Write-RunnerEvent 'game_process_seen' "pid=$($starfield.Id)"

    $mainWindow = Wait-ForCondition -TimeoutSeconds $manifest.launchTimeoutSeconds `
        -Description 'capturable Starfield main window' -Process $starfield -Condition {
            Get-StarfieldMainWindow -Process $starfield
        }
    Write-RunnerEvent 'game_window_seen' (
        "handle=0x{0:X}" -f $mainWindow.Handle.ToInt64())

    $titleProbePath = Join-Path $runDirectory 'title-probe.png'
    $titleSignal = Wait-ForCondition -TimeoutSeconds $titleScreenTimeoutSeconds `
        -Description 'Starfield title prompt pixel signal' -Process $starfield -Condition {
            Save-WindowScreenshot -Process $starfield -Path $titleProbePath
            $signal = Get-TitlePromptSignal -Path $titleProbePath
            if ([AbsoluteControlPanelWindow]::IsTitlePromptSignal($signal)) {
                return $signal
            }
            return $false
        }
    Write-RunnerEvent 'title_prompt_seen' (
        "pixels=$($titleSignal.PixelCount) " +
        "bounds=$($titleSignal.MinimumX),$($titleSignal.MinimumY)-" +
        "$($titleSignal.MaximumX),$($titleSignal.MaximumY)")

    $advancePath = Join-Path (Split-Path -Parent $pluginEvidencePath) (
        "AbsoluteControlPanelResearch.$runId.advance")
    [System.IO.File]::WriteAllText(
        $advancePath,
        [DateTimeOffset]::UtcNow.ToString('o'),
        [System.Text.UTF8Encoding]::new($false))
    Write-RunnerEvent 'title_advance_request_written' $advancePath

    $sendInputMarker = '"run_id":"{0}","event":"title_enter_key_up"' -f $runId
    $sendInputEvent = Wait-ForCondition -TimeoutSeconds 30 `
        -Description 'plugin-side title Enter pulse' -Process $starfield -Condition {
            if (-not (Test-Path -LiteralPath $pluginEvidencePath)) { return $false }
            $eventLine = Get-Content -LiteralPath $pluginEvidencePath |
                Where-Object { $_.Contains($sendInputMarker) } | Select-Object -Last 1
            if ($null -ne $eventLine) { return $eventLine }
            return $false
        }
    if (-not $sendInputEvent.Contains('sent=1 error=0')) {
        throw "Plugin-side Enter pulse failed: $sendInputEvent"
    }
    Write-RunnerEvent 'title_enter_sent' 'method=plugin_game_task_scan_code'

    $script:TitleAbsentSamples = 0
    Wait-ForCondition -TimeoutSeconds $titleTransitionTimeoutSeconds `
        -Description 'Starfield title prompt transition' -Process $starfield -Condition {
            Save-WindowScreenshot -Process $starfield -Path $titleProbePath
            $signal = Get-TitlePromptSignal -Path $titleProbePath
            if ([AbsoluteControlPanelWindow]::IsTitlePromptSignal($signal)) {
                $script:TitleAbsentSamples = 0
                return $false
            }
            $script:TitleAbsentSamples++
            return $script:TitleAbsentSamples -ge 2
        } | Out-Null
    Write-RunnerEvent 'title_prompt_cleared' 'consecutive_absent_samples=2'

    $inputPath = Join-Path (Split-Path -Parent $pluginEvidencePath) (
        "AbsoluteControlPanelResearch.$runId.input")
    $script:InputPath = $inputPath
    Start-Sleep -Milliseconds 1500
    Invoke-ResearchInput -CommandId 1 -Command 'menu_up' -Process $starfield
    Start-Sleep -Milliseconds 500

    $continueSelectionPath = Join-Path $runDirectory 'continue-selection.png'
    Save-WindowScreenshot -Process $starfield -Path $continueSelectionPath
    $continueLuminance = [AbsoluteControlPanelWindow]::AnalyzeMeanLuminance(
        $continueSelectionPath, 0.220, 0.363, 0.008, 0.009)
    $newLuminance = [AbsoluteControlPanelWindow]::AnalyzeMeanLuminance(
        $continueSelectionPath, 0.220, 0.397, 0.008, 0.009)
    $continueSelected = $continueLuminance -ge 180 -and
        ($continueLuminance - $newLuminance) -ge 50
    Write-RunnerEvent 'continue_selection_evaluated' (
        'selected={0} continue_luminance={1:F2} new_luminance={2:F2}' -f
        $continueSelected, $continueLuminance, $newLuminance)
    if (-not $continueSelected) {
        throw 'Continue was not visibly selected after the guarded W input.'
    }
    Invoke-StepTone -Frequency 660

    Invoke-ResearchInput -CommandId 2 -Command 'accept' -Process $starfield
    Start-Sleep -Milliseconds 3000
    Invoke-ResearchInput -CommandId 3 -Command 'accept' -Process $starfield
    Invoke-StepTone -Frequency 740
    $loadWaitMilliseconds = if ($null -ne $manifest.loadWaitMilliseconds) {
        [int]$manifest.loadWaitMilliseconds
    } else { 30000 }
    Write-RunnerEvent 'continue_load_wait_started' "milliseconds=$loadWaitMilliseconds"
    Start-Sleep -Milliseconds $loadWaitMilliseconds
    Invoke-ResearchInput -CommandId 4 -Command 'pause' -Process $starfield

    $pauseMarker = '"run_id":"{0}","event":"research_pause_state","detail":"id=4 open=true"' -f $runId
    Wait-ForCondition -TimeoutSeconds 15 -Description 'PauseMenu open state' `
        -Process $starfield -Condition {
            if (-not (Test-Path -LiteralPath $pluginEvidencePath)) { return $false }
            $content = Get-Content -Raw -LiteralPath $pluginEvidencePath
            if ($content.Contains($pauseMarker)) { return $true }
            return $false
        } | Out-Null
    Write-RunnerEvent 'pause_menu_confirmed'
    Invoke-StepTone -Frequency 880
    $pauseCheckpointPath = Join-Path $runDirectory 'pause-menu.png'
    Save-WindowScreenshot -Process $starfield -Path $pauseCheckpointPath

    Invoke-ResearchInput -CommandId 5 -Command 'pause' -Process $starfield
    $pauseClosedMarker = '"run_id":"{0}","event":"research_pause_state","detail":"id=5 open=false"' -f $runId
    Wait-ForCondition -TimeoutSeconds 15 -Description 'PauseMenu closed state' `
        -Process $starfield -Condition {
            if (-not (Test-Path -LiteralPath $pluginEvidencePath)) { return $false }
            $content = Get-Content -Raw -LiteralPath $pluginEvidencePath
            if ($content.Contains($pauseClosedMarker)) { return $true }
            return $false
        } | Out-Null
    Write-RunnerEvent 'pause_menu_closed_before_custom_menu'

    $armPath = Join-Path (Split-Path -Parent $pluginEvidencePath) (
        "AbsoluteControlPanelResearch.$runId.arm")
    [System.IO.File]::WriteAllText(
        $armPath,
        [DateTimeOffset]::UtcNow.ToString('o'),
        [System.Text.UTF8Encoding]::new($false))
    Write-RunnerEvent 'experiment_arm_written' $armPath

    $movieMarker = '"run_id":"{0}","event":"movie_load_result","detail":"loaded=true"' -f $runId
    $sentinelProbePath = Join-Path $runDirectory 'sentinel-probe.png'
    $script:NextSentinelSample = [DateTime]::UtcNow
    $movieOracle = Wait-ForCondition -TimeoutSeconds $manifest.movieTimeoutSeconds `
        -Description 'successful native menu movie load' -Process $starfield -Condition {
            if (Test-Path -LiteralPath $pluginEvidencePath) {
                $content = Get-Content -Raw -LiteralPath $pluginEvidencePath
                if ($content.Contains($movieMarker)) { return 'plugin_movie_event' }
            }
            if ([DateTime]::UtcNow -ge $script:NextSentinelSample) {
                $script:NextSentinelSample = [DateTime]::UtcNow.AddMilliseconds(
                    $script:SentinelPollMilliseconds)
                try {
                    Save-WindowScreenshot -Process $starfield -Path $sentinelProbePath
                    $probeSignal = Get-SentinelSignal -Path $sentinelProbePath
                    if ($probeSignal.PixelCount -ge $script:SentinelMinimumPixels) {
                        return 'magenta_sentinel'
                    }
                } catch {
                    Write-RunnerEvent 'sentinel_probe_deferred' $_.Exception.Message
                }
            }
            return $false
        }
    Write-RunnerEvent 'movie_oracle_seen' $movieOracle
    Invoke-StepTone -Frequency 1040

    Start-Sleep -Milliseconds $manifest.captureDelayMilliseconds
    $screenshotPath = Join-Path $runDirectory 'native-menu.png'
    Save-WindowScreenshot -Process $starfield -Path $screenshotPath
    Write-RunnerEvent 'screenshot_captured' $screenshotPath
    $sentinelSignal = Get-SentinelSignal -Path $screenshotPath
    if (-not (Write-SentinelSignal -Signal $sentinelSignal -Path $screenshotPath `
            -Event 'sentinel_evaluated')) {
        throw "Native menu movie did not produce the required magenta sentinel."
    }

    Wait-ForPluginEvent -Event 'bridge_snapshot_applied' `
        -DetailContains 'generation=0 expected=0 matches=true' -TimeoutSeconds 10 `
        -Process $starfield | Out-Null
    Write-RunnerEvent 'initial_snapshot_round_trip_confirmed'

    Invoke-ResearchInput -CommandId 6 -Command 'accept' -Process $starfield
    Wait-ForPluginEvent -Event 'bridge_command_accepted' `
        -DetailContains 'command=toggleFeature generation=1 enabled=true level=50' `
        -TimeoutSeconds 10 -Process $starfield | Out-Null
    Wait-ForPluginEvent -Event 'bridge_snapshot_applied' `
        -DetailContains 'generation=1 expected=1 matches=true' -TimeoutSeconds 10 `
        -Process $starfield | Out-Null

    Invoke-ResearchInput -CommandId 7 -Command 'nav_down' -Process $starfield
    Invoke-ResearchInput -CommandId 8 -Command 'nav_right' -Process $starfield
    Wait-ForPluginEvent -Event 'bridge_command_accepted' `
        -DetailContains 'command=incrementLevel generation=2 enabled=true level=55' `
        -TimeoutSeconds 10 -Process $starfield | Out-Null
    Wait-ForPluginEvent -Event 'bridge_snapshot_applied' `
        -DetailContains 'generation=2 expected=2 matches=true' -TimeoutSeconds 10 `
        -Process $starfield | Out-Null
    Write-RunnerEvent 'representative_controls_round_trip_confirmed'

    Invoke-ResearchInput -CommandId 9 -Command 'nav_down' -Process $starfield
    Invoke-ResearchInput -CommandId 10 -Command 'accept' -Process $starfield
    Wait-ForPluginEvent -Event 'binding_capture_started' `
        -DetailContains 'kind=button' -TimeoutSeconds 10 -Process $starfield | Out-Null
    Wait-ForPluginEvent -Event 'bridge_command_accepted' `
        -DetailContains 'command=beginBindingCapture generation=3' `
        -TimeoutSeconds 10 -Process $starfield | Out-Null

    $vJoyDeviceId = if ($null -ne $manifest.vJoyDeviceId) {
        [uint32]$manifest.vJoyDeviceId
    } else { 1 }
    $vJoyButton = if ($null -ne $manifest.vJoyButton) {
        [uint32]$manifest.vJoyButton
    } else { 1 }
    Invoke-VJoyButtonPulse -DeviceId $vJoyDeviceId -Button $vJoyButton
    $bindingEvent = Wait-ForPluginEvent -Event 'binding_capture_completed' `
        -DetailContains ("@{0}" -f $vJoyButton) -TimeoutSeconds 15 -Process $starfield
    Write-RunnerEvent 'enumerated_binding_capture_confirmed' $bindingEvent
    Wait-ForPluginEvent -Event 'bridge_snapshot_applied' `
        -DetailContains 'generation=4 expected=4 matches=true' -TimeoutSeconds 10 `
        -Process $starfield | Out-Null
    Wait-ForPluginEvent -Event 'dummy_config_written' `
        -DetailContains 'provider=absolute-control-panel.research' -TimeoutSeconds 10 `
        -Process $starfield | Out-Null

    $dummyConfigPath = Join-Path (Split-Path -Parent $pluginEvidencePath) `
        'AbsoluteControlPanelResearch.dummy.ini'
    Wait-ForCondition -TimeoutSeconds 10 -Description 'research-only dummy config' `
        -Process $starfield -Condition {
            if (-not (Test-Path -LiteralPath $dummyConfigPath -PathType Leaf)) {
                return $false
            }
            $dummy = Get-Content -Raw -LiteralPath $dummyConfigPath
            return $dummy.Contains('bAxisInvert=true') -and
                $dummy.Contains('fAxisSensitivity=0.55') -and
                $dummy.Contains(("@{0}" -f $vJoyButton))
        } | Out-Null
    Copy-Item -LiteralPath $dummyConfigPath `
        -Destination (Join-Path $runDirectory 'dummy-config.ini')
    Write-RunnerEvent 'dummy_config_validated' (
        "invert=true sensitivity=0.55 button=$vJoyButton")

    $interactiveScreenshotPath = Join-Path $runDirectory 'interactive-state.png'
    Save-WindowScreenshot -Process $starfield -Path $interactiveScreenshotPath
    Write-RunnerEvent 'interactive_screenshot_captured' $interactiveScreenshotPath

    Invoke-ResearchInput -CommandId 11 -Command 'probe_escape' -Process $starfield
    Wait-ForPluginEvent -Event 'bridge_close' -DetailContains '' -TimeoutSeconds 10 `
        -Process $starfield | Out-Null
    Wait-ForPluginEvent -Event 'menu_message_hide' -DetailContains '' -TimeoutSeconds 10 `
        -Process $starfield | Out-Null
    Wait-ForPluginEvent -Event 'research_pause_state_after_probe_close' `
        -DetailContains 'open=false' -TimeoutSeconds 10 -Process $starfield | Out-Null
    Write-RunnerEvent 'explicit_close_confirmed' 'pause_menu_open=false gameplay_resumed=true'

    $watchdogMarker = '"run_id":"{0}","event":"watchdog_fired"' -f $runId
    Wait-ForCondition -TimeoutSeconds $manifest.watchdogTimeoutSeconds `
        -Description 'native menu watchdog close' -Process $starfield -Condition {
            if (Test-Path -LiteralPath $pluginEvidencePath) {
                $content = Get-Content -Raw -LiteralPath $pluginEvidencePath
                if ($content.Contains($watchdogMarker)) { return $true }
            }
            return $false
        } | Out-Null
    Write-RunnerEvent 'watchdog_seen'

    $matchingEvidence = Get-Content -LiteralPath $pluginEvidencePath |
        Where-Object { $_.Contains(('"run_id":"{0}"' -f $runId)) }
    [System.IO.File]::WriteAllLines(
        (Join-Path $runDirectory 'plugin-events.jsonl'),
        [string[]]$matchingEvidence,
        [System.Text.UTF8Encoding]::new($false))
    Copy-Item -LiteralPath (
        Join-Path $repositoryRoot 'interface\dist\AbsoluteControlPanelMenu.build.json') `
        -Destination $runDirectory
    Write-RunnerEvent 'run_complete'
} catch {
    if ($null -ne $starfield -and -not $starfield.HasExited) {
        Save-DiagnosticBundle -Process $starfield -Reason 'run_failed'
    }
    if ($null -ne $pluginEvidencePath -and (Test-Path -LiteralPath $pluginEvidencePath)) {
        $partialEvidence = @(Get-Content -LiteralPath $pluginEvidencePath |
            Where-Object { $_.Contains(('"run_id":"{0}"' -f $runId)) })
        if ($partialEvidence.Count -gt 0) {
            [System.IO.File]::WriteAllLines(
                (Join-Path $runDirectory 'plugin-events.partial.jsonl'),
                [string[]]$partialEvidence,
                [System.Text.UTF8Encoding]::new($false))
        }
    }
    Write-RunnerEvent 'run_failed' $_.Exception.Message
    throw
} finally {
    if ($null -ne $advancePath -and (Test-Path -LiteralPath $advancePath -PathType Leaf)) {
        Remove-Item -LiteralPath $advancePath -Force
        Write-RunnerEvent 'title_advance_request_removed'
    }
    if ($null -ne $armPath -and (Test-Path -LiteralPath $armPath -PathType Leaf)) {
        Remove-Item -LiteralPath $armPath -Force
        Write-RunnerEvent 'experiment_arm_removed'
    }
    if ($null -ne $inputPath -and (Test-Path -LiteralPath $inputPath -PathType Leaf)) {
        Remove-Item -LiteralPath $inputPath -Force
        Write-RunnerEvent 'research_input_mailbox_removed'
    }
    if ($null -ne $starfield) {
        $starfield.Refresh()
        if (-not $starfield.HasExited) {
            if ($keepGameRunning) {
                Write-RunnerEvent 'session_retained' (
                    "pid=$($starfield.Id) run_id=$runId; use cycle-pause.ps1")
            } else {
                Write-RunnerEvent 'shutdown_requested' 'CloseMainWindow'
                [void]$starfield.CloseMainWindow()
                [void]$starfield.WaitForExit($manifest.shutdownTimeoutSeconds * 1000)
                if (-not $starfield.HasExited) {
                    Write-RunnerEvent 'shutdown_incomplete' 'Starfield was left running; no forced termination used.'
                } else {
                    Write-RunnerEvent 'shutdown_complete'
                }
            }
        }
    }
}
