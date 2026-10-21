# Runs the spike, finds its top-level window by PID, captures it with PrintWindow and saves a PNG.
param([string]$Exe = "$PSScriptRoot\build\Release\spike_sequence.exe", [string]$Out = "$PSScriptRoot\screenshot.png", [string]$ExtraArgs = "")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public struct RECT { public int L, T, R, B; }
public class Cap {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  public static IntPtr Find(uint pid) { IntPtr f = IntPtr.Zero; EnumWindows((h, l) => { uint q; GetWindowThreadProcessId(h, out q); RECT r; GetWindowRect(h, out r); if (q == pid && IsWindowVisible(h) && r.R > r.L) { f = h; return false; } return true; }, IntPtr.Zero); return f; }
}
"@
if ($ExtraArgs -ne "") { $p = Start-Process -FilePath $Exe -ArgumentList $ExtraArgs -PassThru -RedirectStandardOutput "$PSScriptRoot\run.log" }
else { $p = Start-Process -FilePath $Exe -PassThru -RedirectStandardOutput "$PSScriptRoot\run.log" }
$null = $p.Handle
Start-Sleep -Milliseconds 1000
$h = [Cap]::Find([uint32]$p.Id)
$r = New-Object RECT; [Cap]::GetWindowRect($h, [ref]$r) | Out-Null
"hwnd=$h rect=$($r.L),$($r.T),$($r.R),$($r.B) size=$($r.R-$r.L)x$($r.B-$r.T)"
$bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
$ok = [Cap]::PrintWindow($h, $hdc, 2)   # PW_RENDERFULLCONTENT
$g.ReleaseHdc($hdc)
if (-not $ok) { $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size) }
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$p.WaitForExit()
Get-Content "$PSScriptRoot\run.log"
"PrintWindow=$ok exit code: $($p.ExitCode)"
