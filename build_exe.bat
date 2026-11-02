@echo off
rem =====================================================================
rem  Deep Cleaner - one-file EXE build script
rem  Bundles the Epic/Fortnite EAC removal bat into the EXE so the
rem  "Run Epic/Fortnite EAC removal script" option always finds it.
rem =====================================================================

setlocal EnableExtensions

python -m PyInstaller --version >nul 2>nul
if errorlevel 1 (
    echo Installing PyInstaller...
    python -m pip install pyinstaller || goto :fail
)

echo Building DeepCleaner.exe ...

python -m PyInstaller --onefile --noconsole --uac-admin ^
    --name "DeepCleaner" ^
    --add-data "scripts\Remove-Epic-Fortnite-EAC.bat;scripts" ^
    "src\DeepCleaner_app_v2.py"

if errorlevel 1 goto :fail

echo.
echo Build complete.
echo  EXE: dist\DeepCleaner.exe
echo  The EAC removal script is bundled inside - no extra files needed.
echo.
pause
exit /b 0

:fail
echo.
echo Build FAILED. See output above.
echo.
pause
exit /b 1
