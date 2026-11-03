// Window enumeration, PrintWindow capture and pixel heuristics for the Starfield test runner.
// Copied from Absolute-Control 03162e7 tools/research/run-probe.ps1 (lines 28-256),
// where it drove 14 cold starts on 2026-09-19. Only the class name changed.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public static class AbsoluteTestWindow
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
