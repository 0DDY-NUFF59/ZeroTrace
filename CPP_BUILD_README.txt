ZeroTrace C++ build
===================

Run build_cpp.bat on Windows. It uses an installed Visual Studio C++ compiler
when available, or MinGW-w64 as a fallback.

The result is dist\ZeroTrace.exe. The program requests Administrator access,
shows ZeroTrace at the top, and performs the same cleanup categories as the
original Python app.

SINGLE FILE
-----------
dist\ZeroTrace.exe needs no extra files. Two payloads are compiled into the
executable as resources (see cpp\ZeroTrace.rc):

  201 RCDATA  assets\avatar.png
  202 RCDATA  scripts\Remove-Epic-Fortnite-EAC.bat

At runtime the avatar is decoded straight from memory (GDI+ reads the resource
bytes through an in-memory stream), and the EAC batch script is unpacked to
%TEMP%\ZeroTrace\ only when it has to be run, then deleted again.

The project copies of those two files are build inputs only - they must exist
when you run build_cpp.bat, but they do not have to be shipped with the EXE.

Disk copies still win if present. Dropping an assets\avatar.png or a
scripts\Remove-Epic-Fortnite-EAC.bat next to the EXE overrides the embedded
copy, which is handy for swapping the artwork or editing the script without a
rebuild.

The window/taskbar icon is also embedded (resource 101, built from the same
avatar), so Explorer shows the avatar even with no assets folder around.

NOTES
-----
The EAC script asks for confirmation (choice / pause), so ZeroTrace runs it in
its own console window instead of hidden. Answer the prompts there; the log
reports whether the script completed or was closed early.

The cleanup log is saved as Desktop\ZeroTrace_Cleanup_Log.txt.

The EXE is statically linked and imports only Windows system DLLs. It uses the
Universal C Runtime, so it targets Windows 10 and 11.
