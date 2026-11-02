ZeroTrace
=========

The current Windows version is built with C++ and can be built with build_cpp.bat.
See CPP_BUILD_README.txt for the C++ build and avatar instructions.

The original Python version is retained as a fallback in src\DeepCleaner_app_v2.py.

ZeroTrace Deep Cleaner
============

WHAT THIS IS
------------
ZeroTrace is a Windows tool that deep cleans up anti-cheat leftovers,
game launcher traces, temporary files, and privacy data.


HOW TO USE IT
-------------
1. Save ZeroTrace.exe to your computer (Desktop is fine).
   That single file is everything - the avatar and the EAC removal script are
   built into it.
2. Double-click ZeroTrace.exe.
   - Windows will ask for permission (Admin required). Click Yes.
3. A dark window opens with a list of cleanup options.
4. Tick the boxes for what you want to clean.
5. Click "Start cleanup".
6. A small window reminds you to close other programs first.
   Close Steam, Epic, browsers, and games, then click "I've closed all windows".
7. Watch the console at the bottom for live progress.
8. When it finishes, a message tells you it's done.
   The log is saved to your Desktop as ZeroTrace_Cleanup_Log.txt.


THE CLEANUP OPTIONS
-------------------
- Stop anti-cheat services      Stops EasyAntiCheat, BattlEye, Vanguard, etc.
- Kill launcher processes       Ends Steam, Epic, Battle.net, Fortnite.
- Delete anti-cheat folders     Removes leftover folders from anti-cheat tools.
- Clear Temp folders            Empties Windows temp folders.
- Clean registry keys          Removes tracking registry entries.
- Purge browser traces          Deletes Epic/Fortnite/Unreal data from Chrome, Edge, Firefox.
- Randomize MachineGuid        Gives your PC a fresh MachineGuid.
- Clear Jump Lists / Recent     Clears recent-files and jump-list history.
- Flush DNS + netsh reset       Resets the network stack and DNS cache.
- Run Epic/Fortnite EAC removal Runs a full Epic/Fortnite/EasyAntiCheat removal pass.


IMPORTANT NOTES
---------------
- Run as Administrator (the EXE asks for this automatically).
- Close game launchers, games, and browsers before starting.
- Antivirus may flag the EXE because it's homemade and unsigned.
  This is normal. Add an exception or allow it to run.
- A reboot is recommended after cleaning (some services remove on restart).
- This tool does NOT restart your computer automatically.


QUESTIONS?
----------
The full build and packaging notes are in BUILD_EXE_README.txt.
