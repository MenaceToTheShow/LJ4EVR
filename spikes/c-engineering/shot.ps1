param([string]$exe = '')
$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ($exe -eq '') { $exe = Join-Path $dir 'build\Release\seqform.exe' }
# shot.ps1 - start the spike, grab the OS window titled "Sequence" with System.Drawing,
# and also keep the BMP the spike writes of its own back buffer.
# Usage: powershell -ExecutionPolicy Bypass -File shot.ps1
$ErrorActionPreference = 'Stop'
$bmp = Join-Path $dir 'seqform-backbuffer.bmp'
$png = Join-Path $dir 'seqform-screen.png'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class W {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
  public static IntPtr FindByPid(uint pid, string title) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p);
      if (p == pid && IsWindowVisible(h)) { var sb = new System.Text.StringBuilder(256); GetWindowText(h, sb, 256);
        if (sb.ToString() == title) { found = h; return false; } } return true; }, IntPtr.Zero);
    return found; }
}
"@
[void][W]::SetProcessDPIAware()   # SDL3 windows are per-monitor DPI aware; without this GetWindowRect is virtualised
$p = Start-Process -FilePath $exe -ArgumentList @('--shot', $bmp, '--ms', '6000') -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 100 -and $h -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 50; $h = [W]::FindByPid([uint32]$p.Id, 'Sequence') }
"window found after $($i * 50) ms"
if ($h -eq [IntPtr]::Zero) { throw 'window "Sequence" not found' }
[void][W]::SetForegroundWindow($h)
Start-Sleep -Milliseconds 600
$r = New-Object W+RECT; [void][W]::GetWindowRect($h, [ref]$r)
$c = New-Object W+RECT; [void][W]::GetClientRect($h, [ref]$c)
$w = $r.R - $r.L; $hh = $r.B - $r.T
$bm = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bm)
# PrintWindow with PW_RENDERFULLCONTENT (2) reads the window's own surface (works for D3D-presented
# windows on Windows 8.1+), independent of z-order and monitor layout. CopyFromScreen at the
# GetWindowRect coordinates returned the desktop wallpaper on this 4-monitor PC (kept as fallback).
$hdc = $g.GetHdc()
$ok = [W]::PrintWindow($h, $hdc, 2)
$g.ReleaseHdc($hdc)
if (-not $ok) { $g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $hh)) }
"PrintWindow  : $ok"
$bm.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bm.Dispose()
$p.WaitForExit()
"window rect  : $($r.L),$($r.T) $($w)x$($hh)"
"client rect  : $($c.R - $c.L)x$($c.B - $c.T)  (expected 466 x 182 = 163 + 19 px menu bar)"
"exit code    : $($p.ExitCode)"
"screen grab  : $png"
"back buffer  : $bmp"
