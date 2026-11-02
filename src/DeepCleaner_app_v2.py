"""
Deep System Cleaner - Python GUI (Vercel-inspired)
Run as Administrator.
Package: pyinstaller --onefile --noconsole --uac-admin DeepCleaner.py
"""

import os
import sys
import ctypes
import subprocess
import threading
import uuid
import shutil
import winreg
from pathlib import Path
from datetime import datetime

try:
    import tkinter as tk
    from tkinter import ttk, messagebox, scrolledtext
except ImportError:
    print("tkinter is required.")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Theme (Vercel-inspired)
# ---------------------------------------------------------------------------
C = {
    "bg": "#000000",
    "bg_elevated": "#0a0a0a",
    "card": "#111111",
    "border": "#1e1e1e",
    "border_hover": "#333333",
    "text": "#ededed",
    "text_secondary": "#888888",
    "text_muted": "#666666",
    "accent": "#ffffff",
    "accent_dim": "#a1a1a1",
    "success": "#00c853",
    "danger": "#ff5555",
    "btn_primary_bg": "#ffffff",
    "btn_primary_fg": "#000000",
    "btn_secondary_bg": "#111111",
    "btn_secondary_fg": "#ededed",
    "console_bg": "#0a0a0a",
    "console_fg": "#a1a1a1",
    "console_border": "#1e1e1e",
}


def is_admin() -> bool:
    try:
        return ctypes.windll.shell32.IsUserAnAdmin() != 0
    except Exception:
        return False


def get_desktop_log() -> Path:
    return Path(os.path.expanduser("~")) / "Desktop" / "Deep_Cleanup_Log.txt"


def safe_remove_path(path: str) -> tuple:
    p = Path(path)
    if not p.exists():
        return False, "not found"
    try:
        if p.is_file():
            p.unlink(missing_ok=True)
        else:
            shutil.rmtree(p, ignore_errors=False)
        if p.exists():
            return False, "still exists"
        return True, "deleted"
    except Exception as e:
        return False, str(e)


def safe_delete_reg_key(root, path: str) -> tuple:
    try:
        winreg.DeleteKeyEx(root, path, 0, 0)
        return True, "deleted"
    except FileNotFoundError:
        return False, "not found"
    except OSError:
        try:
            hive = "HKLM" if root == winreg.HKEY_LOCAL_MACHINE else "HKCU"
            subprocess.run(
                ["reg", "delete", f"{hive}\\{path}", "/f"],
                capture_output=True,
                text=True,
                creationflags=subprocess.CREATE_NO_WINDOW,
            )
            return True, "deleted"
        except Exception as e:
            return False, str(e)
    except Exception as e:
        return False, str(e)


def set_machine_guid(new_guid: str) -> tuple:
    try:
        key = winreg.OpenKey(
            winreg.HKEY_LOCAL_MACHINE,
            r"SOFTWARE\Microsoft\Cryptography",
            0,
            winreg.KEY_SET_VALUE,
        )
        winreg.SetValueEx(key, "MachineGuid", 0, winreg.REG_SZ, new_guid)
        winreg.CloseKey(key)
        return True, new_guid
    except Exception as e:
        return False, str(e)


def get_machine_guid() -> str:
    try:
        key = winreg.OpenKey(
            winreg.HKEY_LOCAL_MACHINE,
            r"SOFTWARE\Microsoft\Cryptography",
            0,
            winreg.KEY_READ,
        )
        val, _ = winreg.QueryValueEx(key, "MachineGuid")
        winreg.CloseKey(key)
        return str(val)
    except Exception:
        return "(unavailable)"


def get_system_uuid() -> str:
    try:
        r = subprocess.run(
            ["wmic", "csproduct", "get", "uuid"],
            capture_output=True,
            text=True,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        lines = [
            ln.strip()
            for ln in r.stdout.splitlines()
            if ln.strip() and ln.strip().upper() != "UUID"
        ]
        return lines[0] if lines else "(unavailable)"
    except Exception:
        return "(unavailable)"


def find_existing_file(candidates: list):
    for c in candidates:
        if c and Path(c).is_file():
            return c
    return None


def find_eac_script() -> str | None:
    base = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    candidates = [
        base / "scripts" / "Remove-Epic-Fortnite-EAC.bat",
        base / "Remove-Epic-Fortnite-EAC.bat",
        Path(__file__).resolve().parent / "scripts" / "Remove-Epic-Fortnite-EAC.bat",
        Path(__file__).resolve().parent / "Remove-Epic-Fortnite-EAC.bat",
        Path(sys.executable).resolve().parent / "scripts" / "Remove-Epic-Fortnite-EAC.bat",
        Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "DeepCleaner" / "scripts" / "Remove-Epic-Fortnite-EAC.bat",
    ]
    return find_existing_file([str(c) for c in candidates])


def launcher_paths() -> dict:
    local = os.environ.get("LOCALAPPDATA", "")
    pf = os.environ.get("ProgramFiles", r"C:\Program Files")
    pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    return {
        "Steam": [
            rf"{pf86}\Steam\steam.exe",
            rf"{pf}\Steam\steam.exe",
            r"C:\Program Files (x86)\Steam\steam.exe",
        ],
        "Epic Games Launcher": [
            rf"{pf86}\Epic Games\Launcher\Portal\Binaries\Win32\EpicGamesLauncher.exe",
            rf"{pf}\Epic Games\Launcher\Portal\Binaries\Win32\EpicGamesLauncher.exe",
            rf"{pf86}\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe",
        ],
        "Battle.net": [
            rf"{pf86}\Battle.net\Battle.net Launcher.exe",
            rf"{pf}\Battle.net\Battle.net Launcher.exe",
            rf"{local}\Battle.net\Battle.net.exe",
        ],
    }


def browser_paths() -> dict:
    local = os.environ.get("LOCALAPPDATA", "")
    pf = os.environ.get("ProgramFiles", r"C:\Program Files")
    pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    return {
        "Google Chrome": [
            rf"{pf}\Google\Chrome\Application\chrome.exe",
            rf"{pf86}\Google\Chrome\Application\chrome.exe",
            rf"{local}\Google\Chrome\Application\chrome.exe",
        ],
        "Microsoft Edge": [
            rf"{pf86}\Microsoft\Edge\Application\msedge.exe",
            rf"{pf}\Microsoft\Edge\Application\msedge.exe",
        ],
        "Mozilla Firefox": [
            rf"{pf}\Mozilla Firefox\firefox.exe",
            rf"{pf86}\Mozilla Firefox\firefox.exe",
        ],
    }


def make_btn(parent, text, command, primary=False, padx=14, pady=7):
    if primary:
        bg, fg, abg = C["btn_primary_bg"], C["btn_primary_fg"], "#e0e0e0"
    else:
        bg, fg, abg = C["btn_secondary_bg"], C["btn_secondary_fg"], "#1a1a1a"
    btn = tk.Button(
        parent,
        text=text,
        font=("Segoe UI", 9, "bold" if primary else "normal"),
        bg=bg,
        fg=fg,
        activebackground=abg,
        activeforeground=fg,
        relief="flat",
        bd=0,
        highlightthickness=1,
        highlightbackground=C["border"],
        highlightcolor=C["border_hover"],
        padx=padx,
        pady=pady,
        cursor="hand2",
        command=command,
    )
    return btn


# ---------------------------------------------------------------------------
# Warning dialog
# ---------------------------------------------------------------------------
class CloseWindowsDialog(tk.Toplevel):
    def __init__(self, parent):
        super().__init__(parent)
        self.result = False
        self.title("Before You Start")
        self.configure(bg=C["bg"])
        self.resizable(False, False)
        self.transient(parent)
        self.grab_set()

        w, h = 460, 300
        self.geometry(f"{w}x{h}")
        self.update_idletasks()
        x = parent.winfo_rootx() + (parent.winfo_width() - w) // 2
        y = parent.winfo_rooty() + (parent.winfo_height() - h) // 2
        self.geometry(f"+{x}+{y}")

        card = tk.Frame(self, bg=C["card"], highlightthickness=1, highlightbackground=C["border"])
        card.pack(fill="both", expand=True, padx=16, pady=16)

        tk.Label(
            card, text="Close other windows", font=("Segoe UI", 13, "bold"),
            fg=C["text"], bg=C["card"],
        ).pack(anchor="w", padx=20, pady=(18, 6))

        msg = (
            "This cleaner works best when other programs are closed.\n\n"
            "Please close:\n"
            "  ·  Game launchers (Steam, Epic, Battle.net)\n"
            "  ·  Games and anti-cheat clients\n"
            "  ·  Web browsers (Chrome, Edge, Firefox)\n\n"
            "Open apps can lock files and cause failed deletes."
        )
        tk.Label(
            card, text=msg, font=("Segoe UI", 9), fg=C["text_secondary"],
            bg=C["card"], justify="left",
        ).pack(anchor="w", padx=20, pady=4)

        btns = tk.Frame(card, bg=C["card"])
        btns.pack(anchor="e", padx=20, pady=(16, 18))
        make_btn(btns, "Cancel", self._cancel, primary=False).pack(side="left", padx=(0, 8))
        make_btn(btns, "I've closed all windows", self._confirm, primary=True).pack(side="left")

        self.protocol("WM_DELETE_WINDOW", self._cancel)
        self.bind("<Escape>", lambda e: self._cancel())
        self.focus_set()

    def _confirm(self):
        self.result = True
        self.destroy()

    def _cancel(self):
        self.result = False
        self.destroy()


# ---------------------------------------------------------------------------
# Verify / Restore dialog
# ---------------------------------------------------------------------------
class VerifyRestoreDialog(tk.Toplevel):
    def __init__(self, parent, log_callback=None):
        super().__init__(parent)
        self.log_callback = log_callback or (lambda m: None)
        self.title("Verify & Restore")
        self.configure(bg=C["bg"])
        self.resizable(False, False)
        self.transient(parent)
        self.grab_set()

        w, h = 560, 440
        self.geometry(f"{w}x{h}")
        self.update_idletasks()
        x = parent.winfo_rootx() + (parent.winfo_width() - w) // 2
        y = parent.winfo_rooty() + (parent.winfo_height() - h) // 2
        self.geometry(f"+{x}+{y}")

        card = tk.Frame(self, bg=C["card"], highlightthickness=1, highlightbackground=C["border"])
        card.pack(fill="both", expand=True, padx=16, pady=16)

        tk.Label(
            card, text="Verify & Restore", font=("Segoe UI", 13, "bold"),
            fg=C["text"], bg=C["card"],
        ).pack(anchor="w", padx=16, pady=(14, 2))
        tk.Label(
            card, text="Scan for launchers and browsers, then relaunch if found.",
            font=("Segoe UI", 9), fg=C["text_secondary"], bg=C["card"],
        ).pack(anchor="w", padx=16, pady=(0, 10))

        self.text = scrolledtext.ScrolledText(
            card, height=16, bg=C["console_bg"], fg=C["console_fg"],
            font=("Consolas", 9), relief="flat", bd=0,
            insertbackground=C["text"],
            highlightthickness=1, highlightbackground=C["border"],
        )
        self.text.pack(fill="both", expand=True, padx=16, pady=4)

        btns = tk.Frame(card, bg=C["card"])
        btns.pack(fill="x", padx=16, pady=14)
        make_btn(btns, "Scan again", self.scan).pack(side="left", padx=(0, 8))
        make_btn(btns, "Relaunch found apps", self.relaunch, primary=True).pack(side="left", padx=(0, 8))
        make_btn(btns, "Close", self.destroy).pack(side="right")

        self.found = {}
        self.scan()

    def _write(self, msg: str):
        self.text.insert("end", msg + "\n")
        self.text.see("end")
        self.log_callback(msg)

    def scan(self):
        self.text.delete("1.0", "end")
        self.found = {}
        self._write("Launchers")
        self._write("─────────")
        for name, candidates in launcher_paths().items():
            path = find_existing_file(candidates)
            if path:
                self.found[name] = path
                self._write(f"  ✓  {name}")
                self._write(f"     {path}")
            else:
                self._write(f"  ✗  {name}  — missing (reinstall may be needed)")
        self._write("")
        self._write("Browsers")
        self._write("────────")
        for name, candidates in browser_paths().items():
            path = find_existing_file(candidates)
            if path:
                self.found[name] = path
                self._write(f"  ✓  {name}")
                self._write(f"     {path}")
            else:
                self._write(f"  ✗  {name}")
        self._write("")
        if self.found:
            self._write(f"{len(self.found)} app(s) found. Use Relaunch to start them.")
        else:
            self._write("Nothing found at default paths.")

    def relaunch(self):
        if not self.found:
            messagebox.showinfo("Nothing to launch", "No installed apps were found.", parent=self)
            return
        launched = []
        for name, path in self.found.items():
            try:
                subprocess.Popen(
                    [path],
                    cwd=str(Path(path).parent),
                    creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP,
                )
                self._write(f"  → Started {name}")
                launched.append(name)
            except Exception as e:
                self._write(f"  → Failed {name}: {e}")
        if launched:
            messagebox.showinfo(
                "Relaunched",
                "Started:\n  " + "\n  ".join(launched) + "\n\n"
                "If a launcher was heavily cleaned, use Repair\nor reinstall from the official site.",
                parent=self,
            )


# ---------------------------------------------------------------------------
# Main app
# ---------------------------------------------------------------------------
class DeepCleanerApp:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Deep Cleaner")
        self.root.geometry("920x680")
        self.root.minsize(820, 600)
        self.root.configure(bg=C["bg"])

        self.vars = {}
        self.running = False
        self._build_ui()
        self._load_ids()

    def _build_ui(self):
        # ---- Header ----
        header = tk.Frame(self.root, bg=C["bg"], height=56)
        header.pack(fill="x", padx=24, pady=(20, 0))
        header.pack_propagate(False)

        tk.Label(
            header, text="Deep Cleaner", font=("Segoe UI", 18, "bold"),
            fg=C["text"], bg=C["bg"],
        ).pack(side="left")

        tk.Label(
            header, text="Anti-cheat · Launchers · Privacy",
            font=("Segoe UI", 9), fg=C["text_muted"], bg=C["bg"],
        ).pack(side="left", padx=(12, 0), pady=(6, 0))

        # ---- Main content (scrollable-ish fixed area) ----
        main = tk.Frame(self.root, bg=C["bg"])
        main.pack(fill="both", expand=True, padx=24, pady=(16, 8))

        # Options card
        opt_card = tk.Frame(main, bg=C["card"], highlightthickness=1, highlightbackground=C["border"])
        opt_card.pack(fill="x", pady=(0, 12))

        tk.Label(
            opt_card, text="Cleanup options", font=("Segoe UI", 11, "bold"),
            fg=C["text"], bg=C["card"],
        ).pack(anchor="w", padx=16, pady=(14, 8))

        grid = tk.Frame(opt_card, bg=C["card"])
        grid.pack(fill="x", padx=16, pady=(0, 8))

        options = [
            ("services", "Stop anti-cheat services"),
            ("processes", "Kill launcher processes"),
            ("folders", "Delete anti-cheat folders"),
            ("temp", "Clear Temp folders"),
            ("registry", "Clean registry keys"),
            ("browser", "Purge browser traces"),
            ("machineguid", "Randomize MachineGuid"),
            ("jumplists", "Clear Jump Lists / Recent"),
            ("network", "Flush DNS + netsh reset"),
            ("eac", "Run Epic/Fortnite EAC removal script"),
        ]

        for i, (key, label) in enumerate(options):
            var = tk.BooleanVar(value=True)
            self.vars[key] = var
            row, col = divmod(i, 3)
            cell = tk.Frame(grid, bg=C["card"])
            cell.grid(row=row, column=col, sticky="w", padx=(0, 24), pady=3)
            cb = tk.Checkbutton(
                cell,
                text=label,
                variable=var,
                font=("Segoe UI", 9),
                fg=C["text_secondary"],
                bg=C["card"],
                activebackground=C["card"],
                activeforeground=C["text"],
                selectcolor=C["bg"],
                highlightthickness=0,
                bd=0,
                cursor="hand2",
            )
            cb.pack(anchor="w")

        # Select all / none row
        sel_row = tk.Frame(opt_card, bg=C["card"])
        sel_row.pack(fill="x", padx=16, pady=(4, 14))
        make_btn(sel_row, "Select all", self.select_all, padx=10, pady=4).pack(side="left", padx=(0, 6))
        make_btn(sel_row, "Select none", self.select_none, padx=10, pady=4).pack(side="left")

        # IDs + actions row
        mid = tk.Frame(main, bg=C["bg"])
        mid.pack(fill="x", pady=(0, 12))

        id_card = tk.Frame(mid, bg=C["card"], highlightthickness=1, highlightbackground=C["border"])
        id_card.pack(side="left", fill="both", expand=True, padx=(0, 8))

        tk.Label(
            id_card, text="System IDs", font=("Segoe UI", 11, "bold"),
            fg=C["text"], bg=C["card"],
        ).pack(anchor="w", padx=14, pady=(12, 6))

        self.lbl_mg = tk.Label(
            id_card, text="MachineGuid  ·  loading...",
            font=("Consolas", 8), fg=C["text_muted"], bg=C["card"],
            justify="left", anchor="w", wraplength=400,
        )
        self.lbl_mg.pack(fill="x", padx=14, pady=2)

        self.lbl_uuid = tk.Label(
            id_card, text="System UUID  ·  loading...",
            font=("Consolas", 8), fg=C["text_muted"], bg=C["card"],
            justify="left", anchor="w", wraplength=400,
        )
        self.lbl_uuid.pack(fill="x", padx=14, pady=(2, 12))

        action_card = tk.Frame(mid, bg=C["card"], highlightthickness=1, highlightbackground=C["border"])
        action_card.pack(side="right", fill="y")

        tk.Label(
            action_card, text="Actions", font=("Segoe UI", 11, "bold"),
            fg=C["text"], bg=C["card"],
        ).pack(anchor="w", padx=14, pady=(12, 10))

        self.btn_start = make_btn(action_card, "Start cleanup", self.start_cleanup, primary=True, padx=20, pady=9)
        self.btn_start.pack(padx=14, pady=(0, 8), fill="x")

        make_btn(action_card, "Verify / Restore", self.open_verify, padx=16, pady=6).pack(padx=14, pady=(0, 6), fill="x")
        make_btn(action_card, "Open log folder", self.open_log_folder, padx=16, pady=6).pack(padx=14, pady=(0, 14), fill="x")

        # Progress
        prog_wrap = tk.Frame(main, bg=C["bg"])
        prog_wrap.pack(fill="x", pady=(0, 8))

        self.lbl_status = tk.Label(
            prog_wrap, text="Ready", font=("Segoe UI", 9),
            fg=C["text_secondary"], bg=C["bg"], anchor="w",
        )
        self.lbl_status.pack(fill="x")

        prog_row = tk.Frame(prog_wrap, bg=C["bg"])
        prog_row.pack(fill="x", pady=(6, 0))

        style = ttk.Style()
        try:
            style.theme_use("clam")
        except Exception:
            pass
        style.configure(
            "Vercel.Horizontal.TProgressbar",
            troughcolor=C["card"],
            background=C["accent"],
            bordercolor=C["border"],
            lightcolor=C["accent"],
            darkcolor=C["accent"],
            thickness=4,
        )
        self.progress = ttk.Progressbar(
            prog_row, orient="horizontal", mode="determinate",
            maximum=100, style="Vercel.Horizontal.TProgressbar",
        )
        self.progress.pack(side="left", fill="x", expand=True, padx=(0, 10))
        self.lbl_pct = tk.Label(
            prog_row, text="0%", font=("Segoe UI", 9),
            fg=C["text_muted"], bg=C["bg"], width=4,
        )
        self.lbl_pct.pack(side="right")

        # ---- Console at bottom ----
        console_frame = tk.Frame(self.root, bg=C["bg"])
        console_frame.pack(fill="both", expand=False, padx=24, pady=(0, 16))

        console_header = tk.Frame(console_frame, bg=C["bg"])
        console_header.pack(fill="x", pady=(0, 6))
        tk.Label(
            console_header, text="Console", font=("Segoe UI", 9, "bold"),
            fg=C["text_muted"], bg=C["bg"],
        ).pack(side="left")
        tk.Label(
            console_header, text="Live output · log also saved to Desktop",
            font=("Segoe UI", 8), fg=C["text_muted"], bg=C["bg"],
        ).pack(side="left", padx=(10, 0))

        self.log = scrolledtext.ScrolledText(
            console_frame,
            height=12,
            bg=C["console_bg"],
            fg=C["console_fg"],
            insertbackground=C["text"],
            font=("Consolas", 9),
            relief="flat",
            bd=0,
            highlightthickness=1,
            highlightbackground=C["console_border"],
            highlightcolor=C["border_hover"],
        )
        self.log.pack(fill="both", expand=True)
        self.log.configure(state="disabled")

    def _load_ids(self):
        mg = get_machine_guid()
        uid = get_system_uuid()
        self.lbl_mg.config(text=f"MachineGuid  ·  {mg}")
        self.lbl_uuid.config(text=f"System UUID  ·  {uid}")

    def select_all(self):
        for v in self.vars.values():
            v.set(True)

    def select_none(self):
        for v in self.vars.values():
            v.set(False)

    def open_log_folder(self):
        desktop = Path(os.path.expanduser("~")) / "Desktop"
        os.startfile(str(desktop))

    def open_verify(self):
        VerifyRestoreDialog(self.root)

    def log_write(self, msg: str):
        ts = datetime.now().strftime("%H:%M:%S")
        line = f"[{ts}]  {msg}\n"
        self.log.configure(state="normal")
        self.log.insert("end", line)
        self.log.see("end")
        self.log.configure(state="disabled")
        self.root.update_idletasks()

    def set_status(self, text: str, pct=None):
        self.lbl_status.config(text=text)
        if pct is not None:
            pct = max(0, min(100, int(pct)))
            self.progress["value"] = pct
            self.lbl_pct.config(text=f"{pct}%")
        self.root.update_idletasks()

    def start_cleanup(self):
        if self.running:
            return
        selected = [k for k, v in self.vars.items() if v.get()]
        if not selected:
            messagebox.showinfo("Nothing selected", "Please select at least one option.")
            return

        dlg = CloseWindowsDialog(self.root)
        self.root.wait_window(dlg)
        if not dlg.result:
            self.set_status("Cancelled")
            return

        self.running = True
        self.btn_start.config(state="disabled")
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")
        self.progress["value"] = 0
        self.lbl_pct.config(text="0%")
        threading.Thread(target=self._run_cleanup, args=(selected,), daemon=True).start()

    def _run_cleanup(self, selected: list):
        log_path = get_desktop_log()
        file_lines = [f"Cleanup Started: {datetime.now()}", f"Selected: {', '.join(selected)}", ""]

        def log(msg: str):
            self.root.after(0, lambda m=msg: self.log_write(m))
            file_lines.append(msg)

        def status(text: str, pct: int):
            self.root.after(0, lambda t=text, p=pct: self.set_status(t, p))

        total = max(1, len(selected))
        step = 0

        try:
            if "services" in selected:
                step += 1
                status("Stopping services…", int(step / total * 90))
                log("Stopping game services and tracking telemetry…")
                services = [
                    "EasyAntiCheat", "EasyAntiCheatSys", "BEService",
                    "vgk", "vgc", "EpicOnlineServices",
                ]
                for svc in services:
                    try:
                        r = subprocess.run(
                            ["sc", "query", svc], capture_output=True, text=True,
                            creationflags=subprocess.CREATE_NO_WINDOW,
                        )
                        if r.returncode == 0:
                            subprocess.run(
                                ["sc", "stop", svc], capture_output=True,
                                creationflags=subprocess.CREATE_NO_WINDOW,
                            )
                            log(f"  stopped  {svc}")
                        else:
                            log(f"  skip     {svc} (not found)")
                    except Exception as e:
                        log(f"  error    {svc}: {e}")

            if "processes" in selected:
                step += 1
                status("Terminating processes…", int(step / total * 90))
                log("Terminating tracking processes…")
                processes = [
                    "EpicGamesLauncher", "Steam", "Battle.net",
                    "FortniteClient-Win64-Shipping", "vgtray",
                ]
                for proc in processes:
                    try:
                        r = subprocess.run(
                            ["taskkill", "/F", "/IM", f"{proc}.exe"],
                            capture_output=True, text=True,
                            creationflags=subprocess.CREATE_NO_WINDOW,
                        )
                        if r.returncode == 0:
                            log(f"  killed   {proc}")
                        else:
                            log(f"  skip     {proc} (not running)")
                    except Exception as e:
                        log(f"  error    {proc}: {e}")

            if "folders" in selected:
                step += 1
                status("Removing folders…", int(step / total * 90))
                log("Removing anti-cheat and launcher remnants…")
                local = os.environ.get("LOCALAPPDATA", "")
                appdata = os.environ.get("APPDATA", "")
                pf = os.environ.get("ProgramFiles", r"C:\Program Files")
                pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
                paths = [
                    rf"{pf}\EasyAntiCheat",
                    rf"{pf86}\EasyAntiCheat",
                    rf"{pf}\Common Files\EasyAntiCheat",
                    rf"{pf86}\Common Files\EasyAntiCheat",
                    rf"{local}\EasyAntiCheat",
                    rf"{appdata}\EasyAntiCheat",
                    rf"{pf}\BattlEye",
                    rf"{pf86}\Common Files\BattlEye",
                    rf"{pf}\Riot Vanguard",
                    rf"{local}\Riot Games",
                    rf"{local}\EpicGamesLauncher",
                    rf"{appdata}\EpicGamesLauncher",
                    rf"{local}\FortniteGame",
                    rf"{local}\Battle.net",
                    rf"{appdata}\Battle.net",
                    rf"{local}\Steam",
                    rf"{pf86}\Steam\config",
                ]
                for path in paths:
                    ok, detail = safe_remove_path(path)
                    if detail == "not found":
                        log(f"  skip     {path}")
                    elif ok:
                        log(f"  deleted  {path}")
                    else:
                        log(f"  fail     {path} — {detail}")

            if "temp" in selected:
                step += 1
                status("Clearing temp…", int(step / total * 90))
                log("Purging temporary folders…")
                for td in [os.environ.get("TEMP", ""), r"C:\Windows\Temp"]:
                    if not td or not os.path.isdir(td):
                        continue
                    for root, dirs, files in os.walk(td, topdown=False):
                        for name in files:
                            try:
                                os.remove(os.path.join(root, name))
                            except Exception:
                                pass
                        for name in dirs:
                            try:
                                os.rmdir(os.path.join(root, name))
                            except Exception:
                                pass
                log("  temp cleared")

            if "registry" in selected:
                step += 1
                status("Cleaning registry…", int(step / total * 90))
                log("Cleaning tracking registry entries…")
                reg_targets = [
                    (winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Services\EasyAntiCheat"),
                    (winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Services\BEService"),
                    (winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Services\vgk"),
                    (winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Services\vgc"),
                    (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\EasyAntiCheat"),
                    (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve"),
                    (winreg.HKEY_CURRENT_USER, r"Software\Valve"),
                    (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Epic Games"),
                    (winreg.HKEY_CURRENT_USER, r"Software\Epic Games"),
                    (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Blizzard Entertainment"),
                    (winreg.HKEY_CURRENT_USER, r"Software\Blizzard Entertainment"),
                ]
                for root, path in reg_targets:
                    ok, detail = safe_delete_reg_key(root, path)
                    hive = "HKLM" if root == winreg.HKEY_LOCAL_MACHINE else "HKCU"
                    full = f"{hive}\\{path}"
                    if detail == "not found":
                        log(f"  skip     {full}")
                    elif ok:
                        log(f"  deleted  {full}")
                    else:
                        log(f"  fail     {full} — {detail}")

            if "browser" in selected:
                step += 1
                status("Purging browser traces…", int(step / total * 90))
                log("Purging browser traces (Epic / Fortnite / Unreal)…")
                local = os.environ.get("LOCALAPPDATA", "")
                appdata = os.environ.get("APPDATA", "")
                browser_roots = [
                    Path(local) / "Google" / "Chrome" / "User Data",
                    Path(local) / "Microsoft" / "Edge" / "User Data",
                    Path(appdata) / "Mozilla" / "Firefox" / "Profiles",
                ]
                patterns = ("epicgames", "fortnite", "unrealengine")
                for broot in browser_roots:
                    if not broot.exists():
                        continue
                    for p in broot.rglob("*"):
                        try:
                            if any(x in str(p).lower() for x in patterns):
                                if p.is_file():
                                    p.unlink(missing_ok=True)
                                    log(f"  wiped    {p}")
                                elif p.is_dir():
                                    shutil.rmtree(p, ignore_errors=True)
                                    log(f"  wiped    {p}")
                        except Exception:
                            pass
                log("  browser purge complete")

            if "machineguid" in selected:
                step += 1
                status("Randomizing MachineGuid…", int(step / total * 90))
                log("Randomizing MachineGuid…")
                new_guid = str(uuid.uuid4())
                ok, detail = set_machine_guid(new_guid)
                if ok:
                    log(f"  new guid {new_guid}")
                    self.root.after(0, lambda: self.lbl_mg.config(text=f"MachineGuid  ·  {new_guid}"))
                else:
                    log(f"  fail     {detail}")

            if "jumplists" in selected:
                step += 1
                status("Clearing Jump Lists…", int(step / total * 90))
                log("Clearing Jump Lists and Recent files…")
                appdata = os.environ.get("APPDATA", "")
                shell_paths = [
                    Path(appdata) / "Microsoft" / "Windows" / "Recent" / "AutomaticDestinations",
                    Path(appdata) / "Microsoft" / "Windows" / "Recent" / "CustomDestinations",
                    Path(appdata) / "Microsoft" / "Windows" / "Recent",
                ]
                for sp in shell_paths:
                    if sp.exists():
                        for item in sp.iterdir():
                            try:
                                if item.is_file():
                                    item.unlink(missing_ok=True)
                                elif item.is_dir():
                                    shutil.rmtree(item, ignore_errors=True)
                            except Exception:
                                pass
                        log(f"  cleared  {sp}")
                log("  jump lists cleared")

            if "network" in selected:
                step += 1
                status("Resetting network…", int(step / total * 95))
                log("Flushing DNS and resetting network stack…")
                subprocess.run(
                    ["ipconfig", "/flushdns"], capture_output=True,
                    creationflags=subprocess.CREATE_NO_WINDOW,
                )
                subprocess.run(
                    ["netsh", "int", "ip", "reset"], capture_output=True,
                    creationflags=subprocess.CREATE_NO_WINDOW,
                )
                log("  network reset complete")

            if "eac" in selected:
                step += 1
                status("Running EAC removal script…", int(step / total * 95))
                script = find_eac_script()
                if not script:
                    log("  EAC script not found — skipping")
                else:
                    log(f"Running EAC removal script: {script}")
                    try:
                        proc = subprocess.Popen(
                            ["cmd", "/c", script],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            stdin=subprocess.PIPE,
                            text=True,
                            bufsize=1,
                            creationflags=subprocess.CREATE_NO_WINDOW,
                        )
                        proc.stdin.write("Y\r\nY\r\n\r\n")
                        proc.stdin.close()
                        for line in iter(proc.stdout.readline, ""):
                            if line.strip():
                                self.root.after(0, lambda m=line.rstrip(): self.log_write(m))
                                file_lines.append(line.rstrip())
                        proc.wait()
                        rc = proc.returncode
                        if rc == 0:
                            log("  EAC removal script finished")
                        else:
                            log(f"  EAC removal script exited with code {rc}")
                    except Exception as e:
                        log(f"  EAC script failed: {e}")

            status("Finished", 100)
            log("")
            log("────────────────────────────────")
            log("Cleanup finished")
            log(f"Log → {log_path}")
            log("────────────────────────────────")

            try:
                log_path.write_text("\n".join(file_lines), encoding="utf-8")
            except Exception:
                pass

            def after_done():
                messagebox.showinfo(
                    "Done",
                    "Cleanup finished.\n\n"
                    "Log saved to Desktop\\Deep_Cleanup_Log.txt\n\n"
                    "No restart was performed.",
                )
                if messagebox.askyesno(
                    "Verify & Restore?",
                    "Scan for browsers and launchers now?",
                ):
                    VerifyRestoreDialog(self.root)

            self.root.after(0, after_done)

        except Exception as e:
            log(f"CRITICAL  {e}")
            status("Error — check console", 100)
            self.root.after(0, lambda: messagebox.showerror("Error", str(e)))
        finally:
            try:
                log_path.write_text("\n".join(file_lines), encoding="utf-8")
            except Exception:
                pass
            self.running = False
            self.root.after(0, lambda: self.btn_start.config(state="normal"))

    def run(self):
        self.root.mainloop()


if __name__ == "__main__":
    if not is_admin():
        if sys.platform == "win32":
            try:
                ctypes.windll.shell32.ShellExecuteW(
                    None, "runas", sys.executable,
                    f'"{os.path.abspath(sys.argv[0])}"', None, 1,
                )
                sys.exit(0)
            except Exception:
                pass
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror(
            "Admin Required",
            "This tool requires Administrator privileges.\n\n"
            "Right-click → Run as administrator,\n"
            "or rebuild the exe with --uac-admin.",
        )
        sys.exit(1)

    app = DeepCleanerApp()
    app.run()
