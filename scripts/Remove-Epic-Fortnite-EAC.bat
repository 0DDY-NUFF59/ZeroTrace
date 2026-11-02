@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Epic Games / Fortnite / Easy Anti-Cheat - Complete Removal

rem ====================================================================
rem  1. ADMIN CHECK (offers to relaunch elevated via UAC)
rem ====================================================================
fltmc >nul 2>&1
if %errorlevel% equ 0 goto :IsAdmin

echo.
echo  This script must be run as Administrator.
echo.
choice /c YN /m "Relaunch with administrator rights now (UAC prompt)"
if errorlevel 2 exit /b 1
powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs" >nul 2>&1
if errorlevel 1 goto :ElevationFailed
exit /b 0

:ElevationFailed
echo.
echo  Could not request elevation. Right-click this file and choose
echo  "Run as administrator".
echo.
pause
exit /b 1

:IsAdmin
cls
echo ======================================================================
echo   EPIC GAMES / FORTNITE / EASY ANTI-CHEAT - COMPLETE REMOVAL
echo ======================================================================
echo.
echo  This will PERMANENTLY remove Epic Games Launcher, Fortnite, Easy
echo  Anti-Cheat, and their services, files, registry keys, scheduled
echo  tasks, firewall rules, shares and temp files.
echo.
echo  Current user profile : %USERPROFILE%
echo  Target user profile  : %TARGET_PROFILE%  (user "%TARGET_USER%")
echo  (Installs outside C: are NOT touched.)
echo.
choice /c YN /m "Continue"
if errorlevel 2 exit /b 0

rem ---- Setup ----------------------------------------------------------
set "LOG=%TEMP%\cleanup_run_log.txt"
set "FWTMP=%TEMP%\fw_removed.tmp"
set "TARGET_USER=Zero"
set "TARGET_PROFILE=C:\Users\Zero"
set /a CNT_PROC=0, CNT_SVC=0, CNT_APP=0, CNT_DIR=0, CNT_FILE=0, CNT_REG=0
set /a CNT_TASK=0, CNT_FW=0, CNT_NET=0, CNT_TEMP=0, CNT_FAIL=0
echo Run started %date% %time% > "%LOG%"

rem ====================================================================
rem  Step 0: Stop running processes
rem ====================================================================
call :Section "Stopping running processes"
for %%P in (EpicGamesLauncher.exe EpicWebHelper.exe EpicOnlineServicesUserHelper.exe ^
 FortniteLauncher.exe FortniteClient-Win64-Shipping.exe FortniteClient-Win64-Shipping_BE.exe ^
 FortniteClient-Win64-Shipping_EAC.exe FortniteClient-Win64-Shipping_EAC_EOS.exe ^
 EasyAntiCheat.exe EasyAntiCheat_EOS.exe EasyAntiCheat_Setup.exe) do call :KillProc "%%P"

rem ====================================================================
rem  Step 1: Services
rem ====================================================================
call :Section "Stopping and removing services"
for %%S in (EasyAntiCheat EasyAntiCheat_EOS EasyAntiCheatSys EpicOnlineServices) do call :DelService "%%S"

rem Discover any other Epic / EAC services by name, display name or binary path
for /f "usebackq delims=" %%L in (`wmic service where "name like '%%EpicOnline%%' or name like '%%EpicGames%%' or name like '%%Epic Games%%' or name like '%%EasyAntiCheat%%' or displayname like '%%Epic Games%%' or displayname like '%%Easy Anti-Cheat%%' or pathname like '%%Epic Games%%' or pathname like '%%EasyAntiCheat%%'" get Name /value 2^>nul`) do for /f "tokens=1,* delims==" %%A in ("%%L") do if /i "%%A"=="Name" call :DelService "%%B"

rem ====================================================================
rem  Step 2: Uninstall Epic Games Launcher / EOS / EAC via MsiExec
rem ====================================================================
call :Section "Uninstalling installed programs (MsiExec)"
call :ScanUninstall "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall" UNINSTALL
call :ScanUninstall "HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall" UNINSTALL
call :ScanUninstall "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall" UNINSTALL

rem ====================================================================
rem  Step 3: Files and folders
rem ====================================================================
call :Section "Deleting files and folders"
call :DelDir "%ProgramFiles%\Epic Games"
call :DelDir "%ProgramFiles(x86)%\Epic Games"
call :DelDir "%ProgramFiles%\EasyAntiCheat"
call :DelDir "%ProgramFiles(x86)%\EasyAntiCheat"
call :DelDir "%ProgramFiles(x86)%\EasyAntiCheat_EOS"
call :DelDir "%CommonProgramFiles%\Epic Games"
call :DelDir "%CommonProgramFiles(x86)%\Epic Games"
call :DelDir "%ProgramData%\Epic"
call :DelDir "%LocalAppData%\EpicGamesLauncher"
call :DelDir "%LocalAppData%\FortniteGame"
call :DelDir "%LocalAppData%\EasyAntiCheat"
call :DelDir "%AppData%\Epic Games"
call :DelDir "%AppData%\FortniteGame"
call :DelDir "%ProgramData%\Microsoft\Windows\Start Menu\Programs\Epic Games"
call :DelDir "%ProgramData%\Microsoft\Windows\Start Menu\Programs\Fortnite"
call :DelDir "%AppData%\Microsoft\Windows\Start Menu\Programs\Epic Games"
call :DelDir "%AppData%\Microsoft\Windows\Start Menu\Programs\Fortnite"
call :DelFile "%PUBLIC%\Desktop\Epic Games Launcher.lnk"
call :DelFile "%USERPROFILE%\Desktop\Epic Games Launcher.lnk"
call :DelFile "%PUBLIC%\Desktop\Fortnite.lnk"
call :DelFile "%USERPROFILE%\Desktop\Fortnite.lnk"
call :DelDir "C:\Program Files\Epic Games\Fortnite"
call :DelDir "C:\Epic Games"
call :CleanProfile

rem ====================================================================
rem  Step 4: Registry
rem ====================================================================
call :Section "Removing registry keys"
call :DelKey "HKLM\SOFTWARE\Epic Games"
call :DelKey "HKLM\SOFTWARE\EpicGames"
call :DelKey "HKLM\SOFTWARE\WOW6432Node\Epic Games"
call :DelKey "HKLM\SOFTWARE\WOW6432Node\EpicGames"
call :DelKey "HKCU\Software\Epic Games"
call :DelKey "HKCU\Software\EpicGames"
call :DelKey "HKLM\SOFTWARE\Classes\com.epicgames.launcher"
call :DelKey "HKCU\Software\Classes\com.epicgames.launcher"
call :DelRegValue "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" "EpicGamesLauncher"
call :ScanUninstall "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall" DELETE
call :ScanUninstall "HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall" DELETE
call :ScanUninstall "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall" DELETE
call :CleanUserHive

rem ====================================================================
rem  Step 5: Scheduled tasks (matches task name OR the program it runs)
rem ====================================================================
call :Section "Removing scheduled tasks"
for /f "usebackq tokens=2 delims=," %%T in (`schtasks /query /fo csv /v /nh 2^>nul ^| findstr /i /c:"Epic Games" /c:"EpicGames" /c:"Fortnite" /c:"EasyAntiCheat" /c:"Easy Anti-Cheat"`) do call :DelTask "%%~T"

rem ====================================================================
rem  Step 6: Firewall rules (inbound + outbound; name OR program path)
rem ====================================================================
call :Section "Removing Windows Firewall rules"
if exist "%FWTMP%" del /f /q "%FWTMP%" >nul 2>&1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='Epic ?Games|Fortnite|Easy ?Anti-?Cheat|EpicWebHelper|EpicOnlineServices'; $r=@(); $r+=Get-NetFirewallRule -ErrorAction SilentlyContinue | Where-Object { $_.DisplayName -match $p }; $r+=Get-NetFirewallApplicationFilter -ErrorAction SilentlyContinue | Where-Object { $_.Program -match $p } | Get-NetFirewallRule -ErrorAction SilentlyContinue; $r | Sort-Object Name -Unique | ForEach-Object { ('{0} [{1}]' -f $_.DisplayName,$_.Direction); $_ | Remove-NetFirewallRule -ErrorAction SilentlyContinue }" > "%FWTMP%" 2>nul
if exist "%FWTMP%" for /f "usebackq delims=" %%L in ("%FWTMP%") do call :LogFW "%%L"
if exist "%FWTMP%" del /f /q "%FWTMP%" >nul 2>&1

rem ====================================================================
rem  Step 7: Network shares and mapped drives
rem ====================================================================
call :Section "Removing network shares and mapped drives"
for /f "usebackq tokens=1-3" %%A in (`net use 2^>nul ^| findstr /i /c:"Epic" /c:"Fortnite"`) do call :DelMap "%%B"
for /f "usebackq tokens=1" %%S in (`net share 2^>nul ^| findstr /i /c:"Epic" /c:"Fortnite"`) do call :DelShare "%%S"

rem ====================================================================
rem  Step 8: Temp files
rem ====================================================================
call :Section "Cleaning temp files"
call :CleanTemp "%TEMP%"
call :CleanTemp "%SystemRoot%\Temp"

rem ====================================================================
rem  Step 9: Summary
rem ====================================================================
echo.
echo ======================================================================
echo   SUMMARY
echo ======================================================================
echo   Processes stopped ............ !CNT_PROC!
echo   Services removed ............. !CNT_SVC!
echo   Programs uninstalled ......... !CNT_APP!
echo   Folders deleted .............. !CNT_DIR!
echo   Files deleted ................ !CNT_FILE!
echo   Registry keys/values removed . !CNT_REG!
echo   Scheduled tasks removed ...... !CNT_TASK!
echo   Firewall rules removed ....... !CNT_FW!
echo   Shares / mapped drives ....... !CNT_NET!
echo   Temp items deleted ........... !CNT_TEMP!
echo   FAILURES ..................... !CNT_FAIL!
echo ----------------------------------------------------------------------
echo   Detailed list of everything that was done:
echo ----------------------------------------------------------------------
if exist "%LOG%" type "%LOG%"
echo ----------------------------------------------------------------------
echo   Log saved to: %LOG%
echo   A reboot is recommended (services in use are removed on restart).
echo ======================================================================
echo.
pause
endlocal
exit /b 0


rem ####################################################################
rem  SUBROUTINES
rem ####################################################################

:Section
echo.
echo ---- %~1 ----
>>"%LOG%" echo.
>>"%LOG%" echo ---- %~1 ----
exit /b 0

:Log
set "MSG=%~1"
echo   !MSG!
>>"%LOG%" echo   !MSG!
exit /b 0

:KillProc
tasklist /fi "imagename eq %~1" 2>nul | find /i "%~1" >nul
if errorlevel 1 exit /b 0
taskkill /f /im "%~1" >nul 2>&1
call :Log "[STOPPED]  Process: %~1"
set /a CNT_PROC+=1
exit /b 0

:DelService
if "%~1"=="" exit /b 0
sc query "%~1" >nul 2>&1
if errorlevel 1 exit /b 0
sc stop "%~1" >nul 2>&1
timeout /t 2 /nobreak >nul
sc delete "%~1" >nul 2>&1
set "RC=!errorlevel!"
if "!RC!"=="1072" exit /b 0
if not "!RC!"=="0" goto :DelService_Fail
call :Log "[REMOVED]  Service: %~1"
set /a CNT_SVC+=1
exit /b 0
:DelService_Fail
call :Log "[FAILED]   Service: %~1 - sc delete error !RC!"
set /a CNT_FAIL+=1
exit /b 0

:DelDir
if not exist "%~1\" exit /b 0
set "CTR=CNT_DIR"
if not "%~2"=="" set "CTR=%~2"
rd /s /q "%~1" >nul 2>&1
if exist "%~1\" takeown /f "%~1" /r /d y >nul 2>&1
if exist "%~1\" icacls "%~1" /grant *S-1-5-32-544:F /t /c /q >nul 2>&1
if exist "%~1\" rd /s /q "%~1" >nul 2>&1
if exist "%~1\" goto :DelDir_Fail
call :Log "[REMOVED]  Folder: %~1"
set /a !CTR!+=1
exit /b 0
:DelDir_Fail
call :Log "[FAILED]   Folder not fully deleted (in use?): %~1"
set /a CNT_FAIL+=1
exit /b 0

:DelFile
if not exist "%~1" exit /b 0
set "CTR=CNT_FILE"
if not "%~2"=="" set "CTR=%~2"
attrib -r -s -h "%~1" >nul 2>&1
del /f /q "%~1" >nul 2>&1
if exist "%~1" goto :DelFile_Fail
call :Log "[REMOVED]  File: %~1"
set /a !CTR!+=1
exit /b 0
:DelFile_Fail
call :Log "[FAILED]   File: %~1"
set /a CNT_FAIL+=1
exit /b 0

:DelKey
reg query "%~1" >nul 2>&1
if errorlevel 1 exit /b 0
reg delete "%~1" /f >nul 2>&1
reg query "%~1" >nul 2>&1
if not errorlevel 1 goto :DelKey_Fail
call :Log "[REMOVED]  Registry key: %~1"
set /a CNT_REG+=1
exit /b 0
:DelKey_Fail
call :Log "[FAILED]   Registry key: %~1"
set /a CNT_FAIL+=1
exit /b 0

:DelRegValue
reg query "%~1" /v "%~2" >nul 2>&1
if errorlevel 1 exit /b 0
reg delete "%~1" /v "%~2" /f >nul 2>&1
if errorlevel 1 goto :DelRegValue_Fail
call :Log "[REMOVED]  Registry value: %~1 [%~2]"
set /a CNT_REG+=1
exit /b 0
:DelRegValue_Fail
call :Log "[FAILED]   Registry value: %~1 [%~2]"
set /a CNT_FAIL+=1
exit /b 0

rem  %1 = Uninstall root key, %2 = UNINSTALL or DELETE
:ScanUninstall
for /f "usebackq delims=" %%K in (`reg query "%~1" 2^>nul`) do call :CheckUninstallKey "%%K" %~2
exit /b 0

:CheckUninstallKey
set "DN="
for /f "usebackq tokens=2,*" %%A in (`reg query "%~1" /v DisplayName 2^>nul ^| findstr /i /c:"DisplayName"`) do set "DN=%%B"
if not defined DN exit /b 0
echo "!DN!"| findstr /i /c:"Epic Games" /c:"Fortnite" /c:"Easy Anti-Cheat" /c:"EasyAntiCheat" /c:"Epic Online Services" >nul
if errorlevel 1 exit /b 0
if /i "%~2"=="DELETE" goto :CUK_Delete
for %%G in ("%~1") do set "KN=%%~nxG"
if not "!KN:~0,1!"=="{" exit /b 0
call :Log "[UNINSTALL] !DN! !KN!"
start "" /wait msiexec /x "!KN!" /qn /norestart
set "RC=!errorlevel!"
if "!RC!"=="0" goto :CUK_Ok
if "!RC!"=="3010" goto :CUK_Ok
if "!RC!"=="1605" exit /b 0
call :Log "[FAILED]   MsiExec uninstall of !DN! returned !RC!"
set /a CNT_FAIL+=1
exit /b 0
:CUK_Ok
call :Log "[REMOVED]  Program: !DN!"
set /a CNT_APP+=1
exit /b 0
:CUK_Delete
call :DelKey "%~1"
exit /b 0

:DelTask
schtasks /query /tn "%~1" >nul 2>&1
if errorlevel 1 exit /b 0
schtasks /delete /tn "%~1" /f >nul 2>&1
if errorlevel 1 goto :DelTask_Fail
call :Log "[REMOVED]  Scheduled task: %~1"
set /a CNT_TASK+=1
exit /b 0
:DelTask_Fail
call :Log "[FAILED]   Scheduled task: %~1"
set /a CNT_FAIL+=1
exit /b 0

:LogFW
call :Log "[REMOVED]  Firewall rule: %~1"
set /a CNT_FW+=1
exit /b 0

:DelMap
set "T=%~1"
if "!T!"=="" exit /b 0
if "!T:~1,1!"==":" goto :DelMap_Go
if "!T:~0,2!"=="\\" goto :DelMap_Go
exit /b 0
:DelMap_Go
net use "!T!" /delete /y >nul 2>&1
if errorlevel 1 goto :DelMap_Fail
call :Log "[REMOVED]  Mapped drive / connection: !T!"
set /a CNT_NET+=1
exit /b 0
:DelMap_Fail
call :Log "[FAILED]   Mapped drive / connection: !T!"
set /a CNT_FAIL+=1
exit /b 0

:DelShare
if "%~1"=="" exit /b 0
net share "%~1" /delete /y >nul 2>&1
if errorlevel 1 goto :DelShare_Fail
call :Log "[REMOVED]  Network share: %~1"
set /a CNT_NET+=1
exit /b 0
:DelShare_Fail
call :Log "[FAILED]   Network share: %~1"
set /a CNT_FAIL+=1
exit /b 0

:CleanProfile
if not exist "%TARGET_PROFILE%\" exit /b 0
call :DelDir "%TARGET_PROFILE%\AppData\Local\EpicGamesLauncher"
call :DelDir "%TARGET_PROFILE%\AppData\Local\FortniteGame"
call :DelDir "%TARGET_PROFILE%\AppData\Local\EasyAntiCheat"
call :DelDir "%TARGET_PROFILE%\AppData\Roaming\Epic Games"
call :DelDir "%TARGET_PROFILE%\AppData\Roaming\FortniteGame"
call :DelDir "%TARGET_PROFILE%\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Epic Games"
call :DelDir "%TARGET_PROFILE%\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Fortnite"
call :DelFile "%TARGET_PROFILE%\Desktop\Epic Games Launcher.lnk"
call :DelFile "%TARGET_PROFILE%\Desktop\Fortnite.lnk"
call :DelFile "%TARGET_PROFILE%\OneDrive\Desktop\Epic Games Launcher.lnk"
call :DelFile "%TARGET_PROFILE%\OneDrive\Desktop\Fortnite.lnk"
call :CleanTemp "%TARGET_PROFILE%\AppData\Local\Temp"
exit /b 0

rem  Cleans the HKCU-equivalent registry keys of TARGET_USER when the script
rem  is running as a different account (uses the live hive or loads NTUSER.DAT)
:CleanUserHive
if /i "%USERNAME%"=="%TARGET_USER%" exit /b 0
if not exist "%TARGET_PROFILE%\NTUSER.DAT" exit /b 0
set "UHIVE="
set "UNLOAD="
set "USID="
for /f "usebackq tokens=2 delims==" %%S in (`wmic useraccount where "name='%TARGET_USER%'" get sid /value 2^>nul`) do for /f "delims=" %%Z in ("%%S") do set "USID=%%Z"
if defined USID reg query "HKU\!USID!" >nul 2>&1 && set "UHIVE=HKU\!USID!"
if defined UHIVE goto :CUH_Work
reg load "HKU\TempUserHive" "%TARGET_PROFILE%\NTUSER.DAT" >nul 2>&1
if errorlevel 1 goto :CUH_LoadFail
set "UHIVE=HKU\TempUserHive"
set "UNLOAD=1"
:CUH_Work
call :Log "[INFO]     Cleaning registry of user %TARGET_USER%"
call :DelKey "!UHIVE!\Software\Epic Games"
call :DelKey "!UHIVE!\Software\EpicGames"
call :DelKey "!UHIVE!\Software\Classes\com.epicgames.launcher"
call :DelRegValue "!UHIVE!\Software\Microsoft\Windows\CurrentVersion\Run" "EpicGamesLauncher"
call :ScanUninstall "!UHIVE!\Software\Microsoft\Windows\CurrentVersion\Uninstall" DELETE
if not defined UNLOAD exit /b 0
timeout /t 1 /nobreak >nul
reg unload "HKU\TempUserHive" >nul 2>&1
if errorlevel 1 call :Log "[WARNING]  Could not unload temp hive HKU\TempUserHive - it will clear on reboot"
exit /b 0
:CUH_LoadFail
call :Log "[FAILED]   Could not load registry hive for %TARGET_USER% (profile in use?)"
set /a CNT_FAIL+=1
exit /b 0

:CleanTemp
if not exist "%~1\" exit /b 0
for /d %%D in ("%~1\*Epic*" "%~1\*Fortnite*" "%~1\*EasyAntiCheat*") do call :DelDir "%%~fD" CNT_TEMP
for %%F in ("%~1\*Epic*" "%~1\*Fortnite*" "%~1\*EasyAntiCheat*") do call :DelFile "%%~fF" CNT_TEMP
exit /b 0
