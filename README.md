# ZeroTrace

<img src="assets/avatar.png" width="120" alt="ZeroTrace">

ZeroTrace is a single-file Windows cleanup tool for anti-cheat leftovers, game
launcher traces, temporary files and privacy data. The current release is a
native C++ app; the original Python version is kept as a fallback.

* **Download:** [`dist/ZeroTrace.exe`](dist/ZeroTrace.exe) - one self-contained file
* **Platform:** Windows 10 / 11 (x64, asks for administrator on launch)
* **Log:** `Desktop\ZeroTrace_Cleanup_Log.txt`

## Run it

1. Download `ZeroTrace.exe` (or build it yourself, see below).
2. Double-click it and accept the UAC prompt.
3. Tick the cleanup options you want, then click **Start cleanup**.
4. Close Steam, Epic, browsers and games when the reminder appears.
5. Watch the console at the bottom; the log is written to your Desktop.

Nothing restarts automatically.

## Cleanup options

| Option | What it does |
| --- | --- |
| Stop anti-cheat services | Stops EasyAntiCheat, BattlEye, Vanguard and similar services |
| Kill launcher processes | Ends Steam, Epic, Battle.net and Fortnite processes |
| Delete anti-cheat folders | Removes leftover anti-cheat directories |
| Clear Temp folders | Empties the Windows temp folders |
| Clean registry keys | Removes tracking registry entries |
| Purge browser traces | Deletes Epic / Fortnite / Unreal data from Chrome, Edge and Firefox |
| Randomize MachineGuid | Gives the PC a fresh MachineGuid |
| Clear Jump Lists / Recent | Clears recent-file and jump-list history |
| Flush DNS + netsh reset | Resets the network stack and DNS cache |
| Epic / Fortnite EAC removal | Runs the full Epic / Fortnite / EasyAntiCheat removal pass |

## Build

### C++ (current)

```
build_cpp.bat
```

Uses an installed Visual Studio C++ toolchain when available, or MinGW-w64 as a
fallback, and writes `dist\ZeroTrace.exe`. The avatar, the window icon and the
EAC removal script are compiled into the executable as resources, so the EXE is
a single file. See [`CPP_BUILD_README.txt`](CPP_BUILD_README.txt) for details.

### Python fallback

```
build_exe.bat
```

Builds the original Tkinter version from `src\DeepCleaner_app_v2.py` with
PyInstaller. See [`BUILD_EXE_README.txt`](BUILD_EXE_README.txt).

## Project layout

```
assets/     avatar used in the window and as the app icon
cpp/        C++ sources, icon, resource script and manifest
dist/       built ZeroTrace.exe
scripts/    Epic / Fortnite / EAC removal script (embedded in the EXE)
src/        Python fallback app
build_cpp.bat, build_exe.bat, *_README.txt
```

## Notes

* The EXE is unsigned, so SmartScreen and some antivirus products may warn
  about it. That is expected for a self-built tool - allow it or add an
  exception if you trust the build.
* Administrator rights are required for services, the registry and protected
  folders.
* Close game launchers and games before running a cleanup. A reboot afterwards
  is recommended.
