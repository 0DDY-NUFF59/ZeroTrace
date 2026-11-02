#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <winreg.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <random>
#include <algorithm>
#include <cwctype>
#include <objbase.h>
#include <windowsx.h>
#include <unordered_map>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;
using namespace Gdiplus;

constexpr int WM_LOG = WM_APP + 1;
constexpr int WM_FINISHED = WM_APP + 2;
constexpr int WM_PROGRESS = WM_APP + 3;
constexpr int WM_STATUS = WM_APP + 4;

enum ControlId {
    ID_START = 100,
    ID_VERIFY,
    ID_OPEN_LOG,
    ID_SELECT_ALL,
    ID_SELECT_NONE,
    ID_OPTIONS = 200,
    ID_LOG = 300,
    ID_PROGRESS,
    ID_STATUS,
    ID_PERCENT,
    ID_MACHINE_GUID,
    ID_SYSTEM_UUID,
    ID_AVATAR,
    ID_MINIMIZE = 400,
    ID_CLOSE
};

struct Option { const wchar_t* key; const wchar_t* label; };
const Option OPTIONS[] = {
    {L"services", L"Stop anti-cheat services"},
    {L"processes", L"Kill launcher processes"},
    {L"folders", L"Delete anti-cheat folders"},
    {L"temp", L"Clear Temp folders"},
    {L"registry", L"Clean registry keys"},
    {L"browser", L"Purge browser traces"},
    {L"machineguid", L"Randomize MachineGuid"},
    {L"jumplists", L"Clear Jump Lists / Recent"},
    {L"network", L"Flush DNS + netsh reset"},
    {L"eac", L"Run Epic/Fortnite EAC removal script"}
};

HINSTANCE g_instance;
HWND g_window, g_log, g_status, g_percent, g_machineGuid, g_systemUuid, g_start;
HWND g_checks[10];
bool g_checked[10] = { true, true, true, true, true, true, true, true, true, true };
std::unordered_map<HWND, COLORREF> g_labelColors;
std::unordered_map<HWND, COLORREF> g_labelBacks;
ULONG_PTR g_gdiplusToken;
std::atomic<bool> g_running = false;
std::mutex g_logMutex;
std::vector<std::wstring> g_fileLog;
int g_scale = 100;
int g_fontDpi = 96;
int g_progressValue = 0;
RECT g_barRect{};

// Layout in 96-DPI units; everything is scaled by g_scale at runtime.
constexpr int WIN_W = 940;
constexpr int WIN_H = 700;
constexpr int TITLEBAR_H = 44;

// Charcoal base, true black frame, bright purple accent.
const COLORREF BG           = RGB(22, 22, 24);
const COLORREF TITLE_BG     = RGB(27, 27, 31);
const COLORREF CARD         = RGB(33, 33, 38);
const COLORREF BORDER       = RGB(0, 0, 0);
const COLORREF EDGE         = RGB(52, 52, 62);
const COLORREF TEXT         = RGB(240, 240, 244);
const COLORREF MUTED        = RGB(146, 146, 158);
const COLORREF CONSOLE      = RGB(12, 12, 14);
const COLORREF CONSOLE_TEXT = RGB(198, 202, 212);
const COLORREF ACCENT       = RGB(168, 85, 247);
const COLORREF ACCENT_DARK  = RGB(126, 44, 200);
const COLORREF SUCCESS      = RGB(34, 197, 94);

int S(int value) { return MulDiv(value, g_scale, 100); }


std::wstring nowTime() {
    SYSTEMTIME t{}; GetLocalTime(&t);
    wchar_t value[32];
    swprintf_s(value, L"[%02d:%02d:%02d]  ", t.wHour, t.wMinute, t.wSecond);
    return value;
}

std::wstring desktopPath() {
    PWSTR path = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &path))) {
        result = path;
        CoTaskMemFree(path);
    }
    return result;
}

std::wstring exeDirectory() {
    wchar_t buffer[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return fs::path(buffer).parent_path().wstring();
}

std::wstring tempDirectory() {
    wchar_t buffer[MAX_PATH]{};
    DWORD length = GetTempPathW(MAX_PATH, buffer);
    if (length == 0 || length >= MAX_PATH) return L".";
    return buffer;
}

// Resource IDs for the payloads baked into the exe, so a lone ZeroTrace.exe
// behaves exactly like the folder version.
constexpr int RES_AVATAR_PNG = 201;
constexpr int RES_EAC_SCRIPT = 202;

bool loadResourceBytes(int id, LPCWSTR type, std::vector<BYTE>& out) {
    HRSRC info = FindResourceW(g_instance, MAKEINTRESOURCEW(id), type);
    if (!info) return false;
    DWORD size = SizeofResource(g_instance, info);
    if (size == 0) return false;
    HGLOBAL loaded = LoadResource(g_instance, info);
    if (!loaded) return false;
    const BYTE* data = static_cast<const BYTE*>(LockResource(loaded));
    if (!data) return false;
    out.assign(data, data + size);
    return true;
}

bool writeBytes(const fs::path& path, const std::vector<BYTE>& bytes) {
    std::ofstream file(path.wstring().c_str(), std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
    std::error_code error;
    return !file.fail() && fs::exists(path, error);
}

void postText(UINT message, const std::wstring& text) {
    auto* value = new std::wstring(text);
    PostMessageW(g_window, message, 0, reinterpret_cast<LPARAM>(value));
}

void logLine(const std::wstring& message) {
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        g_fileLog.push_back(message);
    }
    postText(WM_LOG, nowTime() + message + L"\r\n");
}

void statusLine(const std::wstring& message, int percent) {
    postText(WM_STATUS, message);
    PostMessageW(g_window, WM_PROGRESS, percent, 0);
}

bool runHidden(const std::wstring& command) {
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    std::vector<wchar_t> commandLine(command.begin(), command.end()); commandLine.push_back(L'\0');
    bool started = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    if (!started) return false;
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return code == 0;
}

// Applies to console programs that ask questions (choice/pause). A hidden run
// with redirected stdin would answer nothing, so these get their own window.
bool runVisible(const std::wstring& command) {
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::vector<wchar_t> commandLine(command.begin(), command.end()); commandLine.push_back(L'\0');
    bool started = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &startup, &process);
    if (!started) return false;
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return code == 0;
}

bool removePath(const fs::path& path) {
    std::error_code error;
    if (!fs::exists(path, error)) return false;
    fs::remove_all(path, error);
    return !error && !fs::exists(path, error);
}

void clearDirectory(const fs::path& path) {
    std::error_code error;
    if (!fs::is_directory(path, error)) return;
    for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, error)) {
        std::error_code itemError;
        fs::remove_all(entry.path(), itemError);
    }
}

std::wstring machineGuid() {
    HKEY key{}; wchar_t value[256]{}; DWORD size = sizeof(value), type = 0;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &key) != ERROR_SUCCESS) return L"(unavailable)";
    LONG result = RegQueryValueExW(key, L"MachineGuid", nullptr, &type, reinterpret_cast<BYTE*>(value), &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS ? value : L"(unavailable)";
}

std::wstring systemUuid() {
    wchar_t buffer[512]{};
    FILE* pipe = _wpopen(L"wmic csproduct get uuid 2>nul", L"r");
    if (!pipe) return L"(unavailable)";
    std::wstring output;
    while (fgetws(buffer, 512, pipe)) output += buffer;
    _pclose(pipe);
    std::wistringstream stream(output); std::wstring line;
    while (std::getline(stream, line)) {
        line.erase(std::remove_if(line.begin(), line.end(), iswspace), line.end());
        if (!line.empty() && _wcsicmp(line.c_str(), L"UUID") != 0) return line;
    }
    return L"(unavailable)";
}

std::wstring randomGuid() {
    GUID guid{}; CoCreateGuid(&guid); wchar_t value[64]{};
    StringFromGUID2(guid, value, 64);
    std::wstring result = value;
    if (result.size() > 1) result = result.substr(1, result.size() - 2);
    return result;
}

void deleteRegistry(HKEY root, const wchar_t* path) {
    LONG result = RegDeleteTreeW(root, path);
    std::wstring hive = root == HKEY_LOCAL_MACHINE ? L"HKLM\\" : L"HKCU\\";
    if (result == ERROR_SUCCESS) logLine(L"  deleted  " + hive + path);
    else if (result == ERROR_FILE_NOT_FOUND) logLine(L"  skip     " + hive + path);
    else logLine(L"  fail     " + hive + path);
}

void stopServices() {
    logLine(L"Stopping game services and tracking telemetry...");
    for (const wchar_t* service : {L"EasyAntiCheat", L"EasyAntiCheatSys", L"BEService", L"vgk", L"vgc", L"EpicOnlineServices"}) {
        std::wstring query = L"sc query \"" + std::wstring(service) + L"\"";
        if (runHidden(query)) { runHidden(L"sc stop \"" + std::wstring(service) + L"\""); logLine(L"  stopped  " + std::wstring(service)); }
        else logLine(L"  skip     " + std::wstring(service) + L" (not found)");
    }
}

void killProcesses() {
    logLine(L"Terminating launcher processes...");
    for (const wchar_t* process : {L"EpicGamesLauncher.exe", L"Steam.exe", L"Battle.net.exe", L"FortniteClient-Win64-Shipping.exe", L"vgtray.exe"}) {
        if (runHidden(L"taskkill /F /IM \"" + std::wstring(process) + L"\"")) logLine(L"  killed   " + std::wstring(process));
        else logLine(L"  skip     " + std::wstring(process) + L" (not running)");
    }
}

void deleteFolders() {
    logLine(L"Removing anti-cheat and launcher remnants...");
    const wchar_t* local = _wgetenv(L"LOCALAPPDATA"); const wchar_t* appdata = _wgetenv(L"APPDATA");
    const wchar_t* pf = _wgetenv(L"ProgramFiles"); const wchar_t* pf86 = _wgetenv(L"ProgramFiles(x86)");
    std::vector<fs::path> paths = {
        fs::path(pf ? pf : L"C:\\Program Files") / L"EasyAntiCheat",
        fs::path(pf86 ? pf86 : L"C:\\Program Files (x86)") / L"EasyAntiCheat",
        fs::path(pf ? pf : L"C:\\Program Files") / L"Common Files\\EasyAntiCheat",
        fs::path(pf86 ? pf86 : L"C:\\Program Files (x86)") / L"Common Files\\EasyAntiCheat",
        fs::path(local ? local : L"") / L"EasyAntiCheat", fs::path(appdata ? appdata : L"") / L"EasyAntiCheat",
        fs::path(pf ? pf : L"C:\\Program Files") / L"BattlEye", fs::path(pf86 ? pf86 : L"C:\\Program Files (x86)") / L"Common Files\\BattlEye",
        fs::path(pf ? pf : L"C:\\Program Files") / L"Riot Vanguard", fs::path(local ? local : L"") / L"Riot Games",
        fs::path(local ? local : L"") / L"EpicGamesLauncher", fs::path(appdata ? appdata : L"") / L"EpicGamesLauncher",
        fs::path(local ? local : L"") / L"FortniteGame", fs::path(local ? local : L"") / L"Battle.net", fs::path(appdata ? appdata : L"") / L"Battle.net",
        fs::path(local ? local : L"") / L"Steam", fs::path(pf86 ? pf86 : L"C:\\Program Files (x86)") / L"Steam\\config"
    };
    for (const auto& path : paths) logLine((removePath(path) ? L"  deleted  " : L"  skip     ") + path.wstring());
}

void clearTemp() {
    logLine(L"Purging temporary folders...");
    const wchar_t* temp = _wgetenv(L"TEMP"); if (temp) clearDirectory(temp);
    clearDirectory(L"C:\\Windows\\Temp"); logLine(L"  temp cleared");
}

void cleanRegistry() {
    logLine(L"Cleaning tracking registry entries...");
    const std::pair<HKEY, const wchar_t*> targets[] = {
        {HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EasyAntiCheat"}, {HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\BEService"},
        {HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\vgk"}, {HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\vgc"},
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\EasyAntiCheat"}, {HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve"},
        {HKEY_CURRENT_USER, L"Software\\Valve"}, {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Epic Games"}, {HKEY_CURRENT_USER, L"Software\\Epic Games"},
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Blizzard Entertainment"}, {HKEY_CURRENT_USER, L"Software\\Blizzard Entertainment"}
    };
    for (const auto& target : targets) deleteRegistry(target.first, target.second);
}

void purgeBrowser() {
    logLine(L"Purging browser traces (Epic / Fortnite / Unreal)...");
    const wchar_t* local = _wgetenv(L"LOCALAPPDATA"); const wchar_t* appdata = _wgetenv(L"APPDATA");
    std::vector<fs::path> roots = {fs::path(local ? local : L"") / L"Google\\Chrome\\User Data", fs::path(local ? local : L"") / L"Microsoft\\Edge\\User Data", fs::path(appdata ? appdata : L"") / L"Mozilla\\Firefox\\Profiles"};
    for (const auto& root : roots) {
        std::error_code error; if (!fs::exists(root, error)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, error)) {
            std::wstring path = entry.path().wstring(); std::wstring lower = path; std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
            if (lower.find(L"epicgames") != std::wstring::npos || lower.find(L"fortnite") != std::wstring::npos || lower.find(L"unrealengine") != std::wstring::npos) {
                std::error_code itemError; fs::remove_all(entry.path(), itemError);
            }
        }
    }
    logLine(L"  browser purge complete");
}

void clearJumpLists() {
    logLine(L"Clearing Jump Lists and Recent files...");
    const wchar_t* appdata = _wgetenv(L"APPDATA");
    if (appdata) {
        for (const wchar_t* folder : {L"Microsoft\\Windows\\Recent\\AutomaticDestinations", L"Microsoft\\Windows\\Recent\\CustomDestinations", L"Microsoft\\Windows\\Recent"}) clearDirectory(fs::path(appdata) / folder);
    }
    logLine(L"  jump lists cleared");
}

void resetNetwork() {
    logLine(L"Flushing DNS and resetting network stack...");
    runHidden(L"ipconfig /flushdns"); runHidden(L"netsh int ip reset"); logLine(L"  network reset complete");
}

void runEacScript() {
    fs::path script = fs::path(exeDirectory()) / L"scripts\\Remove-Epic-Fortnite-EAC.bat";
    if (!fs::exists(script)) script = fs::path(exeDirectory()) / L"Remove-Epic-Fortnite-EAC.bat";

    // A copy next to the exe still wins (lets the user tweak the script), but
    // otherwise the version baked into the exe is unpacked and used.
    fs::path unpacked;
    if (!fs::exists(script)) {
        std::vector<BYTE> bytes;
        if (!loadResourceBytes(RES_EAC_SCRIPT, RT_RCDATA, bytes) || bytes.empty()) {
            logLine(L"  EAC script not found - skipping");
            return;
        }
        unpacked = fs::path(tempDirectory()) / L"ZeroTrace";
        std::error_code error;
        fs::create_directories(unpacked, error);
        unpacked /= L"Remove-Epic-Fortnite-EAC.bat";
        if (!writeBytes(unpacked, bytes)) {
            logLine(L"  could not unpack the embedded EAC script - skipping");
            return;
        }
        script = unpacked;
        logLine(L"Unpacked the embedded EAC script to Temp");
        logLine(L"  " + unpacked.wstring());
    }

    // The script asks for confirmation, so it needs a real console window.
    logLine(L"Running EAC removal script in its own window...");
    bool ok = runVisible(L"cmd /c \"\"" + script.wstring() + L"\"\"");
    logLine(ok ? L"  EAC removal script finished" : L"  EAC removal script closed without completing");

    if (!unpacked.empty()) { std::error_code error; fs::remove(unpacked, error); }
}

void cleanupWorker(std::vector<int> selected) {
    { std::lock_guard<std::mutex> lock(g_logMutex); g_fileLog.clear(); }
    std::wstring logPath = desktopPath() + L"\\ZeroTrace_Cleanup_Log.txt";
    logLine(L"Cleanup started");
    int step = 0, total = static_cast<int>(selected.size());
    for (int option : selected) {
        ++step; statusLine(L"Working...", step * 90 / (std::max)(total, 1));
        switch (option) {
            case 0: stopServices(); break; case 1: killProcesses(); break; case 2: deleteFolders(); break; case 3: clearTemp(); break;
            case 4: cleanRegistry(); break; case 5: purgeBrowser(); break;
            case 6: { logLine(L"Randomizing MachineGuid..."); std::wstring guid = randomGuid(); HKEY key{}; if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) { RegSetValueExW(key, L"MachineGuid", 0, REG_SZ, reinterpret_cast<const BYTE*>(guid.c_str()), static_cast<DWORD>((guid.size() + 1) * sizeof(wchar_t))); RegCloseKey(key); logLine(L"  new guid " + guid); SetWindowTextW(g_machineGuid, (L"MachineGuid  ·  " + guid).c_str()); } else logLine(L"  failed to update MachineGuid"); break; }
            case 7: clearJumpLists(); break; case 8: resetNetwork(); break; case 9: runEacScript(); break;
        }
    }
    logLine(L""); logLine(L"Cleanup finished"); logLine(L"Log saved to Desktop\\ZeroTrace_Cleanup_Log.txt");
    std::wofstream file(logPath.c_str()); if (file) { std::lock_guard<std::mutex> lock(g_logMutex); for (const auto& line : g_fileLog) file << line << L"\n"; }
    PostMessageW(g_window, WM_FINISHED, 0, 0);
}

HBRUSH brushFor(COLORREF color) {
    static std::unordered_map<COLORREF, HBRUSH> cache;
    auto found = cache.find(color);
    if (found != cache.end()) return found->second;
    HBRUSH brush = CreateSolidBrush(color);
    cache[color] = brush;
    return brush;
}

HFONT font(int size, bool bold = false) {
    return CreateFontW(-MulDiv(size, g_fontDpi, 72), 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}
void setFont(HWND control, HFONT value) { SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(value), TRUE); }

HWND label(HWND parent, const wchar_t* text, int x, int y, int w, int h, COLORREF color = TEXT, bool bold = false, int size = 0, COLORREF back = BG) {
    HWND control = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, S(x), S(y), S(w), S(h), parent, nullptr, g_instance, nullptr);
    setFont(control, font(size > 0 ? size : (bold ? 11 : 9), bold));
    g_labelColors[control] = color;
    g_labelBacks[control] = back;
    return control;
}

HWND button(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, bool primary = false) {
    HWND control = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        S(x), S(y), S(w), S(h), parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
    setFont(control, font(9, primary));
    return control;
}


void appendLog(const std::wstring& text) { int length = GetWindowTextLengthW(g_log); SendMessageW(g_log, EM_SETSEL, length, length); SendMessageW(g_log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str())); }

void openVerify() {
    MessageBoxW(g_window, L"Use the launcher and browser repair tools built into Windows or reinstall an application if it no longer starts.\n\nZeroTrace can scan for installed launchers in a future update.", L"Verify / Restore", MB_OK | MB_ICONINFORMATION);
}

void openLogFolder() { ShellExecuteW(g_window, L"open", desktopPath().c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

// Decodes the baked-in avatar.png once. The stream is deliberately kept
// reachable: GDI+ reads pixels from it lazily, so it must outlive the Bitmap.
Bitmap* embeddedAvatar() {
    static Bitmap* image = nullptr;
    static bool tried = false;
    if (tried) return image;
    tried = true;

    std::vector<BYTE> bytes;
    if (!loadResourceBytes(RES_AVATAR_PNG, RT_RCDATA, bytes) || bytes.empty()) return nullptr;

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!memory) return nullptr;
    void* target = GlobalLock(memory);
    if (!target) { GlobalFree(memory); return nullptr; }
    memcpy(target, bytes.data(), bytes.size());
    GlobalUnlock(memory);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream)) || !stream) { GlobalFree(memory); return nullptr; }

    auto* candidate = new Bitmap(stream, FALSE);
    if (candidate->GetLastStatus() == Ok) image = candidate;
    else delete candidate;

    // GDI+ holds its own reference, so this only drops ours.
    stream->Release();
    return image;
}

HICON loadAvatarIcon() {
    fs::path path = fs::path(exeDirectory()) / L"assets\\avatar.png";
    if (fs::exists(path)) { Bitmap image(path.c_str()); HICON icon{}; if (image.GetLastStatus() == Ok && image.GetHICON(&icon) == Ok) return icon; }
    // Embedded icon (resource 101) keeps the taskbar/window icon correct even when
    // the exe is copied on its own without the assets folder.
    HICON embedded = static_cast<HICON>(LoadImageW(g_instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    if (embedded) return embedded;
    return LoadIconW(nullptr, IDI_APPLICATION);
}

void drawAvatar(HDC dc, RECT rect) {
    fs::path path = fs::path(exeDirectory()) / L"assets\\avatar.png";
    Rect target(static_cast<INT>(rect.left), static_cast<INT>(rect.top), static_cast<INT>(rect.right - rect.left), static_cast<INT>(rect.bottom - rect.top));
    Graphics graphics(dc);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    Bitmap disk(path.c_str());
    if (fs::exists(path) && disk.GetLastStatus() == Ok) {
        graphics.DrawImage(&disk, target);
        return;
    }
    if (Bitmap* memory = embeddedAvatar()) {
        graphics.DrawImage(memory, target);
        return;
    }
    // Last resort: draw the embedded icon instead of a placeholder circle.
    HICON embedded = static_cast<HICON>(LoadImageW(g_instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
        static_cast<int>(rect.right - rect.left), static_cast<int>(rect.bottom - rect.top), LR_DEFAULTCOLOR));
    if (embedded) {
        DrawIconEx(dc, rect.left, rect.top, embedded, static_cast<int>(rect.right - rect.left),
            static_cast<int>(rect.bottom - rect.top), 0, nullptr, DI_NORMAL);
        DestroyIcon(embedded);
        return;
    }
    SolidBrush brush(Color(255, 229, 155, 0));
    graphics.FillEllipse(&brush, target);
    SetTextColor(dc, RGB(20, 20, 20)); SetBkMode(dc, TRANSPARENT); HFONT old = static_cast<HFONT>(SelectObject(dc, font(16, true))); DrawTextW(dc, L"ZT", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE); SelectObject(dc, old);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CTLCOLORSTATIC) {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        if (control == g_log) { SetTextColor(dc, CONSOLE_TEXT); SetBkColor(dc, CONSOLE); return reinterpret_cast<LRESULT>(brushFor(CONSOLE)); }
        auto color = g_labelColors.find(control);
        auto back = g_labelBacks.find(control);
        COLORREF backColor = back != g_labelBacks.end() ? back->second : BG;
        SetTextColor(dc, color != g_labelColors.end() ? color->second : TEXT);
        SetBkColor(dc, backColor);
        return reinterpret_cast<LRESULT>(brushFor(backColor));
    }
    if (message == WM_CTLCOLOREDIT) {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, CONSOLE_TEXT);
        SetBkColor(dc, CONSOLE);
        return reinterpret_cast<LRESULT>(brushFor(CONSOLE));
    }
    if (message == WM_DRAWITEM) {
        auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        HDC dc = draw->hDC;
        RECT rect = draw->rcItem;
        bool isCheck = draw->CtlID >= ID_OPTIONS && draw->CtlID < ID_OPTIONS + 10;
        bool isCaption = draw->CtlID == ID_MINIMIZE || draw->CtlID == ID_CLOSE;
        bool pressed = (draw->itemState & ODS_SELECTED) != 0;
        bool disabled = (draw->itemState & ODS_DISABLED) != 0;
        bool isPrimary = draw->CtlID == ID_START;

        FillRect(dc, &rect, brushFor(isCheck || isCaption ? BG : TITLE_BG));
        SetBkMode(dc, TRANSPARENT);
        HFONT controlFont = reinterpret_cast<HFONT>(SendMessageW(draw->hwndItem, WM_GETFONT, 0, 0));
        HGDIOBJ oldFont = controlFont ? SelectObject(dc, controlFont) : nullptr;

        if (isCheck) {
            bool checked = g_checked[draw->CtlID - ID_OPTIONS];
            int boxSize = S(16);
            int top = rect.top + (rect.bottom - rect.top - boxSize) / 2;
            RECT box{rect.left + S(10), top, rect.left + S(10) + boxSize, top + boxSize};

            HBRUSH boxFill = CreateSolidBrush(checked ? (disabled ? RGB(70, 62, 82) : (pressed ? ACCENT_DARK : ACCENT)) : (pressed ? RGB(45, 45, 54) : CONSOLE));
            HPEN boxPen = CreatePen(PS_SOLID, 1, checked ? (pressed ? ACCENT_DARK : ACCENT) : EDGE);
            HGDIOBJ oldBrush = SelectObject(dc, boxFill);
            HGDIOBJ oldPen = SelectObject(dc, boxPen);
            RoundRect(dc, box.left, box.top, box.right, box.bottom, S(4), S(4));
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(boxPen);
            DeleteObject(boxFill);

            if (checked) {
                HPEN mark = CreatePen(PS_SOLID, (std::max)(2, S(2)), RGB(255, 255, 255));
                HGDIOBJ markOld = SelectObject(dc, mark);
                POINT points[3] = {
                    {box.left + MulDiv(boxSize, 30, 100), box.top + MulDiv(boxSize, 52, 100)},
                    {box.left + MulDiv(boxSize, 44, 100), box.top + MulDiv(boxSize, 68, 100)},
                    {box.left + MulDiv(boxSize, 72, 100), box.top + MulDiv(boxSize, 32, 100)}
                };
                Polyline(dc, points, 3);
                SelectObject(dc, markOld);
                DeleteObject(mark);
            }

            SetTextColor(dc, disabled ? MUTED : TEXT);
            RECT text{rect.left + S(36), rect.top, rect.right, rect.bottom};
            DrawTextW(dc, OPTIONS[draw->CtlID - ID_OPTIONS].label, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        else if (isCaption) {
            bool close = draw->CtlID == ID_CLOSE;
            if (pressed) FillRect(dc, &rect, brushFor(close ? RGB(160, 40, 60) : RGB(44, 44, 52)));
            int cx = (rect.left + rect.right) / 2;
            int cy = (rect.top + rect.bottom) / 2;
            int arm = S(5);
            HPEN glyph = CreatePen(PS_SOLID, (std::max)(1, S(1)), close ? RGB(240, 240, 244) : RGB(190, 190, 202));
            HGDIOBJ glyphOld = SelectObject(dc, glyph);
            if (close) {
                MoveToEx(dc, cx - arm, cy - arm, nullptr); LineTo(dc, cx + arm + 1, cy + arm + 1);
                MoveToEx(dc, cx + arm, cy - arm, nullptr); LineTo(dc, cx - arm - 1, cy + arm + 1);
            } else {
                MoveToEx(dc, cx - arm, cy, nullptr); LineTo(dc, cx + arm + 1, cy);
            }
            SelectObject(dc, glyphOld);
            DeleteObject(glyph);
        }
        else {
            COLORREF fillColor = isPrimary ? (pressed ? ACCENT_DARK : ACCENT) : (pressed ? RGB(48, 48, 58) : CARD);
            COLORREF borderColor = isPrimary ? ACCENT : (pressed ? ACCENT : EDGE);
            if (disabled) { fillColor = RGB(45, 38, 55); borderColor = RGB(70, 60, 84); }

            HBRUSH fill = CreateSolidBrush(fillColor);
            HPEN border = CreatePen(PS_SOLID, 1, borderColor);
            HGDIOBJ oldBrush = SelectObject(dc, fill);
            HGDIOBJ oldPen = SelectObject(dc, border);
            RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, S(8), S(8));
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(border);
            DeleteObject(fill);

            SetTextColor(dc, disabled ? RGB(150, 142, 162) : (isPrimary ? RGB(255, 255, 255) : TEXT));
            wchar_t caption[128]{};
            GetWindowTextW(draw->hwndItem, caption, 128);
            RECT text = rect;
            DrawTextW(dc, caption, -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        if (oldFont) SelectObject(dc, oldFont);
        return TRUE;
    }
    if (message == WM_NCHITTEST) {
        LRESULT hit = DefWindowProcW(window, message, wParam, lParam);
        if (hit == HTCLIENT) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(window, &point);
            if (point.y >= 0 && point.y < S(TITLEBAR_H)) return HTCAPTION;
        }
        return hit;
    }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint);
        RECT client{}; GetClientRect(window, &client);

        FillRect(dc, &client, brushFor(BG));

        RECT strip{0, 0, client.right, S(TITLEBAR_H)};
        FillRect(dc, &strip, brushFor(TITLE_BG));
        RECT stripLine{0, S(TITLEBAR_H) - 1, client.right, S(TITLEBAR_H)};
        FillRect(dc, &stripLine, brushFor(RGB(40, 40, 48)));

        if (g_barRect.right > g_barRect.left) {
            FillRect(dc, &g_barRect, brushFor(RGB(40, 40, 48)));
            if (g_progressValue > 0) {
                RECT filled = g_barRect;
                filled.right = filled.left + MulDiv(g_barRect.right - g_barRect.left, g_progressValue, 100);
                FillRect(dc, &filled, brushFor(ACCENT));
            }
        }

        drawAvatar(dc, RECT{S(14), S(7), S(44), S(37)});

        HPEN frame = CreatePen(PS_SOLID, 1, BORDER);
        HGDIOBJ frameOldPen = SelectObject(dc, frame);
        HGDIOBJ frameOldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, 0, 0, client.right, client.bottom);
        SelectObject(dc, frameOldBrush);
        SelectObject(dc, frameOldPen);
        DeleteObject(frame);

        EndPaint(window, &paint);
        return 0;
    }

    if (message == WM_LOG) { auto* text = reinterpret_cast<std::wstring*>(lParam); appendLog(*text); delete text; return 0; }
    if (message == WM_STATUS) { auto* text = reinterpret_cast<std::wstring*>(lParam); SetWindowTextW(g_status, text->c_str()); delete text; return 0; }
    if (message == WM_PROGRESS) {
        g_progressValue = static_cast<int>(wParam);
        wchar_t value[16];
        swprintf_s(value, L"%d%%", g_progressValue);
        SetWindowTextW(g_percent, value);
        InvalidateRect(window, &g_barRect, FALSE);
        return 0;
    }
    if (message == WM_FINISHED) { g_running = false; EnableWindow(g_start, TRUE); SetWindowTextW(g_status, L"Finished"); MessageBoxW(window, L"Cleanup finished.\n\nThe log was saved to your Desktop as ZeroTrace_Cleanup_Log.txt.\n\nNo restart was performed.", L"ZeroTrace", MB_OK | MB_ICONINFORMATION); return 0; }
    if (message == WM_COMMAND) {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id >= ID_OPTIONS && id < ID_OPTIONS + 10) {
            if (code == BN_CLICKED || code == BN_DOUBLECLICKED) {
                int index = id - ID_OPTIONS;
                g_checked[index] = !g_checked[index];
                InvalidateRect(g_checks[index], nullptr, FALSE);
            }
            return 0;
        }

        if (id == ID_SELECT_ALL || id == ID_SELECT_NONE) {
            bool value = id == ID_SELECT_ALL;
            for (int i = 0; i < 10; ++i) {
                g_checked[i] = value;
                InvalidateRect(g_checks[i], nullptr, FALSE);
            }
        }
        else if (id == ID_MINIMIZE) ShowWindow(window, SW_MINIMIZE);
        else if (id == ID_CLOSE) PostMessageW(window, WM_CLOSE, 0, 0);
        else if (id == ID_OPEN_LOG) openLogFolder();
        else if (id == ID_VERIFY) openVerify();
        else if (id == ID_START && !g_running) {
            std::vector<int> selected; for (int i = 0; i < 10; ++i) if (g_checked[i]) selected.push_back(i);
            if (selected.empty()) { MessageBoxW(window, L"Please select at least one cleanup option.", L"Nothing selected", MB_OK | MB_ICONINFORMATION); return 0; }
            int answer = MessageBoxW(window, L"Close games, launchers, browsers, and other programs before starting.\n\nOpen programs can lock files and cause failed deletes. Continue?", L"Before You Start", MB_OKCANCEL | MB_ICONWARNING);
            if (answer != IDOK) { SetWindowTextW(g_status, L"Cancelled"); return 0; }
            g_running = true; EnableWindow(g_start, FALSE); SetWindowTextW(g_log, L""); g_progressValue = 0; InvalidateRect(window, &g_barRect, FALSE); SetWindowTextW(g_percent, L"0%");
            std::thread(cleanupWorker, selected).detach();
        }
        return 0;
    }

    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wParam, lParam);
}

void createUi(HWND window) {
    // Custom title bar
    label(window, L"ZeroTrace", 52, 3, 240, 20, TEXT, true, 10, TITLE_BG);
    label(window, L"Anti-cheat  \u00B7  Launchers  \u00B7  Privacy", 52, 23, 340, 18, MUTED, false, 8, TITLE_BG);
    button(window, L"", ID_MINIMIZE, WIN_W - 78, 8, 28, 28);
    button(window, L"", ID_CLOSE, WIN_W - 44, 8, 28, 28);

    label(window, L"Cleanup options", 20, 62, 300, 24, TEXT, true);
    for (int i = 0; i < 10; ++i) {
        int row = i / 3, col = i % 3;
        g_checks[i] = CreateWindowW(L"BUTTON", OPTIONS[i].label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            S(20 + col * 248), S(94 + row * 30), S(236), S(28), window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_OPTIONS + i)), g_instance, nullptr);
        setFont(g_checks[i], font(9));
        g_checked[i] = true;
    }

    button(window, L"Select all", ID_SELECT_ALL, 20, 224, 112, 30);
    button(window, L"Select none", ID_SELECT_NONE, 140, 224, 112, 30);

    label(window, L"System IDs", 20, 272, 200, 24, TEXT, true);
    g_machineGuid = label(window, (L"MachineGuid  \u00B7  " + machineGuid()).c_str(), 28, 302, 600, 20, MUTED);
    g_systemUuid = label(window, (L"System UUID  \u00B7  " + systemUuid()).c_str(), 28, 326, 600, 20, MUTED);

    label(window, L"Actions", 660, 272, 150, 24, TEXT, true);
    g_start = button(window, L"Start cleanup", ID_START, 660, 302, 260, 42, true);
    button(window, L"Verify / Restore", ID_VERIFY, 660, 352, 260, 34);
    button(window, L"Open log folder", ID_OPEN_LOG, 660, 394, 260, 34);

    g_status = label(window, L"Ready", 20, 444, 620, 22, MUTED);
    g_barRect = RECT{S(20), S(474), S(820), S(482)};
    g_percent = label(window, L"0%", 830, 466, 60, 22, MUTED, false, 9);

    label(window, L"Console", 20, 492, 62, 20, TEXT, true, 9);
    label(window, L"Live output  \u00B7  full log saved to your Desktop", 86, 492, 360, 20, MUTED, false, 9);
    g_log = CreateWindowExW(0, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        S(20), S(516), S(900), S(160), window, reinterpret_cast<HMENU>(ID_LOG), g_instance, nullptr);
    setFont(g_log, CreateFontW(-MulDiv(9, g_fontDpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas"));
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_instance = instance;

    // Per-monitor DPI aware so Windows does not bitmap-stretch (blur) the whole window.
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using SetDpiContextFn = BOOL(WINAPI*)(HANDLE);
        auto setContext = reinterpret_cast<SetDpiContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setContext) setContext(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4)));
    }

    HDC screen = GetDC(nullptr);
    g_fontDpi = GetDeviceCaps(screen, LOGPIXELSY);
    ReleaseDC(nullptr, screen);
    if (g_fontDpi <= 0) g_fontDpi = 96;
    g_scale = MulDiv(g_fontDpi, 100, 96);

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int workHeight = (work.bottom - work.top) - 32;
    while (g_scale > 80 && S(WIN_H) > workHeight) g_scale -= 5;

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&controls);
    GdiplusStartupInput input;
    GdiplusStartup(&g_gdiplusToken, &input, nullptr);

    WNDCLASSW windowClass{};
    windowClass.style = CS_DROPSHADOW;
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = windowProc;
    windowClass.lpszClassName = L"ZeroTraceWindow";
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = loadAvatarIcon();
    windowClass.hbrBackground = CreateSolidBrush(BG);
    RegisterClassW(&windowClass);

    int width = S(WIN_W), height = S(WIN_H);
    int x = work.left + ((work.right - work.left) - width) / 2;
    int y = work.top + ((work.bottom - work.top) - height) / 2;

    // WS_EX_APPWINDOW keeps a taskbar button for the popup-style borderless window.
    g_window = CreateWindowExW(WS_EX_APPWINDOW, windowClass.lpszClassName, L"ZeroTrace",
        WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX, x, y, width, height, nullptr, nullptr, instance, nullptr);
    createUi(g_window);
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(g_window, 20, &darkMode, sizeof(darkMode));
    ShowWindow(g_window, show);
    UpdateWindow(g_window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    GdiplusShutdown(g_gdiplusToken);
    return 0;
}
