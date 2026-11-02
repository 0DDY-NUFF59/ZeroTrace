@echo off
setlocal EnableExtensions
cd /d "%~dp0"
if not exist cpp\ZeroTrace.cpp (
  echo C++ source was not found.
  pause
  exit /b 1
)
if not exist assets\avatar.png (
  echo assets\avatar.png is missing - the avatar cannot be embedded.
  pause
  exit /b 1
)
if not exist scripts\Remove-Epic-Fortnite-EAC.bat (
  echo scripts\Remove-Epic-Fortnite-EAC.bat is missing - the EAC script cannot be embedded.
  pause
  exit /b 1
)
if not defined VSINSTALLDIR if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  for /f "usebackq delims=" %%V in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%V\VC\Auxiliary\Build\vcvarsall.bat" x64
)
if exist "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat" (
  call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64
)
where cl >nul 2>nul
if not errorlevel 1 goto msvc
where g++ >nul 2>nul
if not errorlevel 1 goto mingw
echo No C++ compiler was found.
echo Install Visual Studio Build Tools with Desktop C++ tools, or MinGW-w64.
pause
exit /b 1
:msvc
if not exist dist mkdir dist
rc /nologo /I "%~dp0" /fo cpp\ZeroTrace.res cpp\ZeroTrace.rc
if errorlevel 1 goto fail
cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /Fe:dist\ZeroTrace.exe cpp\ZeroTrace.cpp cpp\ZeroTrace.res /link /SUBSYSTEM:WINDOWS comctl32.lib user32.lib gdi32.lib ole32.lib dwmapi.lib
if errorlevel 1 goto fail
goto copy
:mingw
if not exist dist mkdir dist
windres cpp\ZeroTrace.rc -O coff -o cpp\ZeroTrace.res
g++ -std=c++17 -municode -mwindows -O2 -static -static-libgcc -static-libstdc++ -o dist\ZeroTrace.exe cpp\ZeroTrace.cpp cpp\ZeroTrace.res -lgdiplus -lcomctl32 -lshell32 -ladvapi32 -lole32 -luuid -ldwmapi
if errorlevel 1 goto fail
:copy
echo.
echo Build complete: dist\ZeroTrace.exe
echo The avatar and the EAC script are embedded in the EXE - it is a single self-contained file.
echo.
pause
exit /b 0
:fail
echo.
echo Build FAILED. See output above.
pause
exit /b 1
