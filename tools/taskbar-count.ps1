# taskbar-count.ps1 - ME-7 / ME-59 evidence: how many taskbar buttons LJ4EVR owns while
# its windows are open (design 2.8: one button for main + every owned window).
#
#   powershell -ExecutionPolicy Bypass -File tools\taskbar-count.ps1 -Exe build-msvc\Release\lj4evr.exe
#   ... -Extra '--script','tests\ui\scripts\w1_acceptance.lj4s'   (any lj4evr switches)
#   ... -Expect 1                                                 (exit 1 when the count differs)
#
# Starts the exe (--ms unless -Extra carries a script), waits -SettleMs, then walks the
# taskbar's UI Automation tree: every Button whose Name mentions LJ4EVR, or that belongs to
# the lj4evr process, counts. Prints the names and the count; the count is the evidence
# line for docs/screenshots/w1 (W1 acceptance 1). Pair with tools\shot.ps1 -Taskbar for
# the picture. Windows 11: the buttons live under the "Taskbar" pane of Shell_TrayWnd.
param(
    [string]$Exe = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build-msvc\Release\lj4evr.exe'),
    [string[]]$Extra = @(),
    [int]$Ms = 6000,
    [int]$SettleMs = 2500,
    [int]$Expect = -1
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

$args = @()
if (-not ($Extra -contains '--script')) { $args += @('--ms', "$Ms") }
$p = Start-Process -FilePath $Exe -ArgumentList ($args + $Extra) -PassThru
Start-Sleep -Milliseconds $SettleMs

$root = [System.Windows.Automation.AutomationElement]::RootElement
$trayCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ClassNameProperty, 'Shell_TrayWnd')
$tray = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $trayCond)
if ($null -eq $tray) { throw 'Shell_TrayWnd not found' }
$btnCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button)
$buttons = $tray.FindAll([System.Windows.Automation.TreeScope]::Descendants, $btnCond)

$count = 0
foreach ($b in $buttons) {
    $name = $b.Current.Name
    $procId = $b.Current.ProcessId
    if ($name -like '*LJ4EVR*' -or $name -like '*lj4evr*' -or $name -like '*Martin LightJockey*' -or $procId -eq $p.Id) {
        "  taskbar button: '$name' (pid $procId)"
        $count++
    }
}
"LJ4EVR taskbar buttons: $count"
if ($p.HasExited -eq $false) { $p.WaitForExit() }
"exit code    : $($p.ExitCode)"
if ($Expect -ge 0 -and $count -ne $Expect) { "expected $Expect"; exit 1 }
