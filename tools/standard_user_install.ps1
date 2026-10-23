# standard_user_install.ps1 - ME-157 (ME-45): the Windows installer, run silently by a freshly created local account
# that is NOT in Administrators, installs per-user. Run on a throwaway GitHub-hosted runner (ci.yml windows-msvc),
# never on a developer PC: it creates a local user. Prints the four records the W4 verdict asks for and exits 1 on any
# failure:
#   1. whoami /groups of that account without S-1-5-32-544 (Administrators);
#   2. lj4evr.exe under that account's %LOCALAPPDATA%\Programs\LJ4EVR;
#   3. the HKCU uninstall key in that account's hive;
#   4. the installed version read back (DisplayVersion) equal to VERSION.
# usage: pwsh -File tools/standard_user_install.ps1 -Installer <setup.exe> -Version <x.y.z>
param([Parameter(Mandatory = $true)][string]$Installer, [Parameter(Mandatory = $true)][string]$Version)
$ErrorActionPreference = 'Stop'
$user = 'ljstd' + (Get-Random -Maximum 9999)
$pw = 'Lj4!' + [guid]::NewGuid().ToString('N').Substring(0, 16)
$sec = ConvertTo-SecureString $pw -AsPlainText -Force
New-LocalUser -Name $user -Password $sec -AccountNeverExpires -PasswordNeverExpires | Out-Null
Add-LocalGroupMember -SID 'S-1-5-32-545' -Member $user                         # Users, and nothing else
$cred = New-Object System.Management.Automation.PSCredential("$env:COMPUTERNAME\$user", $sec)
$work = Join-Path $env:SystemDrive 'lj4evr-me157'
New-Item -ItemType Directory -Force -Path $work | Out-Null
& icacls $work /grant "${user}:(OI)(CI)F" | Out-Null
Copy-Item $Installer (Join-Path $work 'setup.exe')
$fail = 0

function As-User([string]$exe, [string]$argline) {
    $p = Start-Process -FilePath $exe -ArgumentList $argline -Credential $cred -LoadUserProfile -WorkingDirectory $work -Wait -PassThru -WindowStyle Hidden
    return $p.ExitCode
}

As-User 'cmd.exe' "/c whoami /groups > `"$work\groups.txt`" 2>&1 & whoami /user /fo csv /nh > `"$work\user.txt`" 2>&1 & echo %LOCALAPPDATA%> `"$work\local.txt`"" | Out-Null
$groups = Get-Content "$work\groups.txt" -Raw
$sid = ((Get-Content "$work\user.txt" -Raw).Trim() -split ',')[1].Trim('"')
Write-Output "ME-157 account: $env:COMPUTERNAME\$user ($sid)"
Write-Output '--- 1. whoami /groups (as that account)'
Write-Output $groups
if ($groups -match 'S-1-5-32-544') { Write-Output 'FAIL: the account is in Administrators'; $fail = 1 } else { Write-Output 'OK: no S-1-5-32-544 (Administrators) in the token' }

$rc = As-User (Join-Path $work 'setup.exe') '/S'
Write-Output "--- installer /S as that account: exit code $rc"
if ($rc -ne 0) { $fail = 1 }

$prof = (Get-CimInstance Win32_UserProfile | Where-Object { $_.SID -eq $sid }).LocalPath
$local = (Get-Content "$work\local.txt" -Raw).Trim()
Write-Output "profile $prof, the account's %LOCALAPPDATA% = $local"

Write-Output '--- 3. HKCU uninstall key in that account''s hive'
$hive = "Registry::HKEY_USERS\$sid"
$loaded = $false
if (-not (Test-Path $hive)) { & reg.exe load "HKU\$sid" (Join-Path $prof 'NTUSER.DAT') | Out-Null; $loaded = $true }
$keys = Get-ChildItem "$hive\Software\Microsoft\Windows\CurrentVersion\Uninstall" -ErrorAction SilentlyContinue |
    Where-Object { (Get-ItemProperty $_.PSPath).DisplayName -like 'LJ4EVR*' }
$dv = $null
$instdir = $null
foreach ($k in $keys) {
    $p = Get-ItemProperty $k.PSPath
    Write-Output ("OK: HKU\$sid\" + ($k.Name -replace '^HKEY_USERS\\[^\\]+\\', ''))
    Write-Output "    DisplayName=$($p.DisplayName)"
    Write-Output "    UninstallString=$($p.UninstallString)"
    $dv = $p.DisplayVersion
    $instdir = Split-Path ($p.UninstallString.Trim('"'))
}
if (-not $keys) { Write-Output 'FAIL: no LJ4EVR uninstall key under the account''s HKCU'; $fail = 1 }
$machine = Get-ChildItem 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall' | Where-Object { (Get-ItemProperty $_.PSPath).DisplayName -like 'LJ4EVR*' }
if ($machine) { Write-Output 'FAIL: an LJ4EVR key under HKLM (not per-user)'; $fail = 1 }

Write-Output '--- 2. install location (from the uninstall key)'
$want = Join-Path $local 'Programs\LJ4EVR'
if ($instdir -and ($instdir.TrimEnd('\') -ieq $want.TrimEnd('\')) -and (Test-Path (Join-Path $instdir 'lj4evr.exe'))) {
    Write-Output "OK: $instdir = the account's %LOCALAPPDATA%\Programs\LJ4EVR"
    Get-ChildItem $instdir | ForEach-Object { Write-Output ("    " + $_.Name) }
} else { Write-Output "FAIL: install dir '$instdir', expected '$want' with lj4evr.exe"; $fail = 1 }

Write-Output '--- 4. installed version read back'
Write-Output "DisplayVersion=$dv, VERSION=$Version"
if ($dv -ne $Version) { Write-Output 'FAIL: version differs'; $fail = 1 } else { Write-Output 'OK' }
if ($loaded) { [gc]::Collect(); & reg.exe unload "HKU\$sid" 2>$null | Out-Null }   # may stay loaded until the runner exits: harmless
exit $fail
