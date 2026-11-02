Deep System Cleaner — Python GUI → EXE
======================================

Files:
  src\DeepCleaner_app_v2.py   ← main GUI app (run with Python, or build to EXE)
  scripts\Remove-Epic-Fortnite-EAC.bat  ← Epic/Fortnite/EAC removal batch script
  build_exe.bat               ← one-click build script (recommended)
  BUILD_EXE_README.txt        ← this file


REQUIREMENTS
------------
- Windows
- Python 3.10+ (https://www.python.org/)
  During install: check "Add Python to PATH"
- Run terminal as normal user for install; run the cleaner as Admin


QUICK BUILD (recommended)
-------------------------
Double-click  build_exe.bat
  (or run it from a Command Prompt in the project folder)

What it does:
  • Installs PyInstaller if missing
  • Builds a single EXE with no console window
  • Requests admin (UAC) automatically on launch
  • Bundles scripts\Remove-Epic-Fortnite-EAC.bat INSIDE the exe

Result:
  dist\DeepCleaner.exe   (self-contained — no extra files needed)


MANUAL BUILD (PyInstaller command line)
---------------------------------------
In the project root folder, run:

  pyinstaller --onefile --noconsole --uac-admin ^
      --name "DeepCleaner" ^
      --add-data "scripts\Remove-Epic-Fortnite-EAC.bat;scripts" ^
      "src\DeepCleaner_app_v2.py"

Result:
  dist\DeepCleaner.exe

--uac-admin   → Windows asks for Administrator when you launch the EXE
--noconsole   → no background console window (GUI only)
--add-data    → bundles the EAC removal bat inside the EXE


HOW TO USE THE EXE
------------------
1. Double-click DeepCleaner.exe (UAC prompt appears because of --uac-admin)
2. Check the options you want
   - "Run Epic/Fortnite EAC removal script" runs the bundled batch script
     silently inside the app's console — no second window opens
3. Click START CLEANUP
4. Log is written to Desktop\Deep_Cleanup_Log.txt


NOTES
-----
• The EAC removal bat is bundled into the EXE. You do NOT need to copy
  any extra files next to the EXE — the app finds it automatically.
• Same cleanup features as the batch version, plus the EAC removal script
• No automatic restart
• MachineGuid + System UUID shown in the left panel
• Needs Admin for registry / service / protected folders
• Antivirus may flag new unsigned EXEs — that's normal for homemade tools
