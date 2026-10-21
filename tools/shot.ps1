# shot.ps1 - screenshot evidence for the W0 acceptance (design §10 W0 item 3).
#
#   window mode (default): start lj4evr.exe, find the OS window titled -Title in that
#   process, PrintWindow(PW_RENDERFULLCONTENT) it to -Out (PNG). CopyFromScreen at the
#   window rect returns the desktop wallpaper on this four-monitor PC (spikes/c-engineering),
#   PrintWindow reads the window's own surface regardless of z-order or monitor.
#
#   -Taskbar: also grab the primary taskbar (Shell_TrayWnd) to -TaskbarOut so one
#   button for main + Sequence + Cue can be seen; PrintWindow first, screen copy fallback.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\shot.ps1 -Exe build-msvc\Release\lj4evr.exe -Title Sequence -Out docs\screenshots\w0\seqform.png
#   powershell -ExecutionPolicy Bypass -File tools\shot.ps1 -Exe ... -Taskbar -TaskbarOut docs\screenshots\w0\taskbar.png
#   -Extra passes extra switches to lj4evr (e.g. '--native', '--explode'); -Ms is how long it runs.
param(
    [string]$Exe = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build-msvc\Release\lj4evr.exe'),
    [string]$Title = 'Sequence',
    [string]$Out = 'shot.png',
    [switch]$Taskbar,
    [string]$TaskbarOut = 'taskbar.png',
    [string[]]$Extra = @(),
    [int]$Ms = 4000
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class W {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
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

function Grab([IntPtr]$h, [string]$path) {
    $r = New-Object W+RECT; [void][W]::GetWindowRect($h, [ref]$r)
    $w = $r.R - $r.L; $hh = $r.B - $r.T
    $bm = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bm)
    $hdc = $g.GetHdc()
    $ok = [W]::PrintWindow($h, $hdc, 2)      # PW_RENDERFULLCONTENT
    $g.ReleaseHdc($hdc)
    if (-not $ok) { $g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $hh)) }
    $bm.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bm.Dispose()
    "  $path : ${w}x${hh} at $($r.L),$($r.T) PrintWindow=$ok"
}

$p = Start-Process -FilePath $Exe -ArgumentList (@('--ms', "$Ms") + $Extra) -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 100 -and $h -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 50; $h = [W]::FindByPid([uint32]$p.Id, $Title) }
"window '$Title' found after $($i * 50) ms"
if ($h -eq [IntPtr]::Zero) { throw "window '$Title' not found" }
[void][W]::SetForegroundWindow($h)
Start-Sleep -Milliseconds 600
Grab $h $Out
if ($Taskbar) {
    $t = [W]::FindWindow('Shell_TrayWnd', $null)
    if ($t -eq [IntPtr]::Zero) { throw 'Shell_TrayWnd not found' }
    Grab $t $TaskbarOut
}
$p.WaitForExit()
"exit code    : $($p.ExitCode)"
