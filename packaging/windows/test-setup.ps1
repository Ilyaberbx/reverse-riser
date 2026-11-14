# Runs the Windows installer through the scenarios users hit and checks the result. Needs admin;
# removes any Glacier Riser already installed, so run it on CI or a test machine.
#   pwsh packaging/windows/test-setup.ps1 dist/GlacierRiser-Windows-Setup.exe
param([Parameter(Mandatory)][string]$Setup)
$ErrorActionPreference = "Stop"

$Vst3 = "$env:CommonProgramFiles\VST3\Glacier Riser.vst3"
$Vst3Dll = "$Vst3\Contents\x86_64-win\Glacier Riser.vst3"
$AppDir = "$env:ProgramFiles\Ice Cold Sound\Glacier Riser"
$Exe = "$AppDir\Glacier Riser.exe"
$Desktop = "$env:PUBLIC\Desktop\Glacier Riser.lnk"
$StartMenu = "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\Glacier Riser.lnk"
$Uninstall = "$AppDir\unins000.exe"

function Fail($msg) { Write-Host "FAIL: $msg"; exit 1 }
function Pass($msg) { Write-Host "ok - $msg" }
function Run($file, [string[]]$extra) {
  $p = Start-Process $file -ArgumentList (@("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART") + $extra) -Wait -PassThru
  if ($p.ExitCode -ne 0) { Fail "$file exited $($p.ExitCode)" }
}
function Expect([string[]]$present, [string[]]$absent) {
  foreach ($p in $present) { if (-not (Test-Path $p)) { Fail "missing $p" } }
  foreach ($p in $absent) { if (Test-Path $p) { Fail "should not exist: $p" } }
}
function Uninstall { if (Test-Path $Uninstall) { Run $Uninstall @() }; Start-Sleep 2 }

# 1. Fresh install, default components.
Uninstall
Run $Setup @()
Expect @($Vst3Dll, $Exe, $StartMenu, $Uninstall) @($Desktop)
$entry = Get-ChildItem HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall, HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall |
  Get-ItemProperty | Where-Object DisplayName -eq "Glacier Riser"
if (-not $entry) { Fail "no Apps & features entry" }
Pass "fresh install: VST3, Standalone, Start Menu, uninstall entry $($entry.DisplayVersion)"

# 2. Reinstall over the top (an update).
Run $Setup @()
Expect @($Vst3Dll, $Exe) @()
Pass "reinstall over an existing install"

# 3. Uninstall removes everything.
Uninstall
Expect @() @($Vst3, $Exe, $StartMenu)
Pass "uninstall removes VST3, app and shortcuts"

# 4. VST3 only.
Run $Setup @("/COMPONENTS=vst3")
Expect @($Vst3Dll) @($Exe, $StartMenu)
Uninstall
Expect @() @($Vst3)
Pass "VST3-only install and uninstall"

# 5. Standalone only, with a desktop shortcut.
Run $Setup @("/COMPONENTS=standalone", "/TASKS=desktopicon")
Expect @($Exe, $Desktop) @($Vst3)
Uninstall
Expect @() @($Exe, $Desktop)
Pass "Standalone-only install with desktop shortcut, then uninstall"

Write-Host "All installer scenarios passed."
