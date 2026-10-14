// SoF Buddy — graphical Windows updater (XP+). Wraps update_from_zip.cmd with no console.
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr int kIdProgress = 1001;
constexpr int kIdStatus = 1002;
constexpr int kIdFresh = 1003;
constexpr int kIdOk = 1004;
constexpr int kIdHeadline = 1005;
constexpr int kIdNotes = 1006;
constexpr UINT kMsgProgress = WM_APP + 1;
constexpr UINT kMsgDone = WM_APP + 2;
constexpr UINT kMsgFailed = WM_APP + 3;

HWND g_hwnd = nullptr;
HWND g_progress = nullptr;
HWND g_status = nullptr;
HWND g_fresh = nullptr;
HWND g_ok = nullptr;
HWND g_headline = nullptr;
HWND g_notes = nullptr;
std::wstring g_root;
std::wstring g_version_label;
std::wstring g_zip;
std::wstring g_script;
std::wstring g_log;
std::wstring g_progress_file;
HANDLE g_worker = nullptr;

std::wstring utf8_to_wide(const char* s) {
    if (!s || !*s) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &out[0], n);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

std::wstring quote(const std::wstring& s) {
    return L"\"" + s + L"\"";
}

bool file_exists(const std::wstring& path) {
    return PathFileExistsW(path.c_str()) == TRUE;
}

std::wstring narrow_to_wide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) {
        n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
        if (n <= 0) return std::wstring();
        std::wstring out(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &out[0], n);
        if (!out.empty() && out.back() == L'\0') out.pop_back();
        return out;
    }
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], n);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

std::wstring tag_from_zip_path(const std::wstring& zip) {
    if (zip.empty()) return L"";
    size_t slash = zip.find_last_of(L"\\/");
    std::wstring base = (slash == std::wstring::npos) ? zip : zip.substr(slash + 1);
    if (base.size() > 4 && _wcsicmp(base.substr(base.size() - 4).c_str(), L".zip") == 0)
        base.resize(base.size() - 4);
    const wchar_t kPrefix[] = L"sof_buddy-";
    const size_t plen = sizeof(kPrefix) / sizeof(kPrefix[0]) - 1;
    if (base.size() > plen && _wcsnicmp(base.c_str(), kPrefix, plen) == 0)
        return base.substr(plen);
    return base;
}

bool load_pending_update_info(std::wstring& tag, std::wstring& notes) {
    tag.clear();
    notes.clear();
    const std::wstring path = g_root + L"\\sof_buddy\\update\\pending_update.txt";
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;

    std::string raw;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        raw.append(buf, n);
    std::fclose(f);
    if (raw.empty()) return false;

    const size_t sep = raw.find("\n---\n");
    const std::string header = (sep == std::string::npos) ? raw : raw.substr(0, sep);
    std::string body = (sep == std::string::npos) ? std::string() : raw.substr(sep + 5);

    size_t pos = 0;
    while (pos < header.size()) {
        size_t end = header.find('\n', pos);
        if (end == std::string::npos) end = header.size();
        std::string line = header.substr(pos, end - pos);
        pos = end + 1;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const std::string val = line.substr(eq + 1);
        if (key == "tag" && !val.empty())
            tag = narrow_to_wide(val);
    }
    while (!body.empty() && (body.back() == '\n' || body.back() == '\r'))
        body.pop_back();
    if (!body.empty())
        notes = narrow_to_wide(body);
    return !tag.empty() || !notes.empty();
}

void apply_version_ui() {
    std::wstring tag;
    std::wstring notes;
    if (!load_pending_update_info(tag, notes)) {
        tag = tag_from_zip_path(g_zip);
        if (!tag.empty())
            notes = L"No release notes were saved for this package.\r\n"
                    L"Run Check Update and Download in-game before Install+Exit to fetch notes from GitHub.";
    }
    if (tag.empty())
        tag = L"unknown";
    g_version_label = tag;
    const std::wstring headline = L"Installing SoF Buddy " + tag;
    if (g_headline)
        SetWindowTextW(g_headline, headline.c_str());
    if (g_hwnd) {
        const std::wstring title = L"SoF Buddy Update — " + tag;
        SetWindowTextW(g_hwnd, title.c_str());
    }
    if (g_notes)
        SetWindowTextW(g_notes, notes.empty() ? L"(No release notes available.)" : notes.c_str());
}

bool resolve_paths(const std::wstring& zip_arg) {
    wchar_t module[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, module, MAX_PATH)) return false;
    std::wstring dir = module;
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return false;
    dir.resize(slash);
    if (dir.size() >= 9) {
        const std::wstring tail = dir.substr(dir.size() - 9);
        if (_wcsicmp(tail.c_str(), L"\\sof_buddy") == 0 || _wcsicmp(tail.c_str(), L"/sof_buddy") == 0)
            g_root = dir.substr(0, dir.size() - 9);
        else
            g_root = dir;
    } else {
        g_root = dir;
    }
    while (!g_root.empty() && (g_root.back() == L'\\' || g_root.back() == L'/'))
        g_root.pop_back();

    g_script = g_root + L"\\sof_buddy\\update_from_zip.cmd";
    g_log = g_root + L"\\sof_buddy\\update\\.install.log";
    g_progress_file = g_root + L"\\sof_buddy\\update\\.progress";

    if (!zip_arg.empty()) {
        g_zip = zip_arg;
        if (g_zip.size() > 1 && g_zip[1] != L':')
            g_zip = g_root + L"\\" + g_zip;
        wchar_t full[MAX_PATH];
        if (GetFullPathNameW(g_zip.c_str(), MAX_PATH, full, nullptr))
            g_zip = full;
    }
    return file_exists(g_script);
}

void set_status(const wchar_t* text) {
    if (g_status) SetWindowTextW(g_status, text);
}

void set_percent(int pct) {
    if (!g_progress) return;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    SendMessageW(g_progress, PBM_SETPOS, pct, 0);
}

bool env_set(const wchar_t* name, const wchar_t* value) {
    return SetEnvironmentVariableW(name, value) != FALSE;
}

DWORD WINAPI worker_thread(LPVOID) {
    DeleteFileW(g_progress_file.c_str());
    CreateDirectoryW((g_root + L"\\sof_buddy\\update").c_str(), nullptr);

    const bool fresh = (g_fresh && SendMessageW(g_fresh, BM_GETCHECK, 0, 0) == BST_CHECKED);
    env_set(L"SOFBUDDY_UPDATER_NO_PAUSE", L"1");
    env_set(L"SOFBUDDY_UPDATER_SILENT", L"1");
    env_set(L"SOFBUDDY_UPDATER_FRESH", fresh ? L"1" : L"0");

    std::wstring cmdline = L"cmd.exe /c " + quote(g_script);
    if (!g_zip.empty())
        cmdline += L" " + quote(g_zip);
    cmdline += L" > " + quote(g_log) + L" 2>&1";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::wstring mutable_cmd = cmdline;
    mutable_cmd.push_back(L'\0');
    if (!CreateProcessW(nullptr, &mutable_cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                      nullptr, g_root.c_str(), &si, &pi)) {
        PostMessageW(g_hwnd, kMsgFailed, 0, 0);
        return 0;
    }
    CloseHandle(pi.hThread);

    int last_pct = 0;
    DWORD wait = 0;
    for (;;) {
        const DWORD wr = WaitForSingleObject(pi.hProcess, 200);
        FILE* f = _wfopen(g_progress_file.c_str(), L"r");
        if (f) {
            char line[512] = {};
            if (fgets(line, sizeof(line), f)) {
                int pct = 0;
                char msg[256] = {};
                if (std::sscanf(line, "%d|%255[^\r\n]", &pct, msg) >= 1) {
                    if (pct > last_pct) last_pct = pct;
                    if (msg[0]) {
                        wchar_t wmsg[256] = {};
                        MultiByteToWideChar(CP_UTF8, 0, msg, -1, wmsg, 256);
                        PostMessageW(g_hwnd, kMsgProgress, last_pct, (LPARAM)_wcsdup(wmsg));
                    } else {
                        PostMessageW(g_hwnd, kMsgProgress, last_pct, 0);
                    }
                }
            }
            fclose(f);
        } else if (wait < 70) {
            wait += 2;
            const int pulse = 20 + (wait % 50);
            PostMessageW(g_hwnd, kMsgProgress, pulse, (LPARAM)_wcsdup(L"Extracting update files..."));
        }
        if (wr == WAIT_OBJECT_0) break;
    }

    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    PostMessageW(g_hwnd, exit_code == 0 ? kMsgDone : kMsgFailed, 0, 0);
    return 0;
}

void start_worker() {
    if (g_worker) return;
    set_status(L"Installing update — please wait...");
    set_percent(5);
    g_worker = CreateThread(nullptr, 0, worker_thread, nullptr, 0, nullptr);
}

void layout_controls(HWND hwnd, int w, int h) {
    const int pad = 12;
    const int bar_h = 20;
    const int btn_w = 88;
    const int btn_h = 26;
    const int bottom = h - pad - btn_h;
    const int fresh_y = bottom - 24;
    const int progress_y = fresh_y - bar_h - 8;
    const int status_y = progress_y - 22;
    const int notes_y = 34;
    const int notes_h = status_y - notes_y - 8;
    if (g_headline)
        MoveWindow(g_headline, pad, pad, w - pad * 2, 22, TRUE);
    if (g_notes)
        MoveWindow(g_notes, pad, notes_y, w - pad * 2, notes_h > 60 ? notes_h : 60, TRUE);
    if (g_status)
        MoveWindow(g_status, pad, status_y, w - pad * 2, 20, TRUE);
    if (g_progress)
        MoveWindow(g_progress, pad, progress_y, w - pad * 2, bar_h, TRUE);
    if (g_fresh)
        MoveWindow(g_fresh, pad, fresh_y, w - pad * 2, 20, TRUE);
    if (g_ok)
        MoveWindow(g_ok, w - pad - btn_w, bottom, btn_w, btn_h, TRUE);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_PROGRESS_CLASS};
        InitCommonControlsEx(&icc);
        HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        g_headline = CreateWindowW(L"STATIC", L"Installing SoF Buddy...",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, hwnd, (HMENU)kIdHeadline, nullptr, nullptr);
        g_notes = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
            0, 0, 10, 10, hwnd, (HMENU)kIdNotes, nullptr, nullptr);
        g_status = CreateWindowW(L"STATIC", L"Preparing installation...",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, hwnd, (HMENU)kIdStatus, nullptr, nullptr);
        if (font) {
            SendMessageW(g_headline, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(g_notes, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        g_progress = CreateWindowW(PROGRESS_CLASSW, nullptr,
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 0, 0, 10, 10, hwnd, (HMENU)kIdProgress, nullptr, nullptr);
        SendMessageW(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(g_progress, PBM_SETBARCOLOR, 0, RGB(46, 160, 67));
        g_fresh = CreateWindowW(L"BUTTON", L"Start with fresh settings (removes sofbuddy.cfg)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 10, 10, hwnd, (HMENU)kIdFresh, nullptr, nullptr);
        g_ok = CreateWindowW(L"BUTTON", L"Close",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED, 0, 0, 10, 10, hwnd, (HMENU)kIdOk, nullptr, nullptr);
        RECT rc;
        GetClientRect(hwnd, &rc);
        layout_controls(hwnd, rc.right, rc.bottom);
        apply_version_ui();
        start_worker();
        return 0;
    }
    case WM_SIZE:
        layout_controls(hwnd, LOWORD(lp), HIWORD(lp));
        return 0;
    case kMsgProgress: {
        set_percent(static_cast<int>(wp));
        if (lp) {
            set_status(reinterpret_cast<wchar_t*>(lp));
            free(reinterpret_cast<void*>(lp));
        }
        return 0;
    }
    case kMsgDone:
        set_percent(100);
        if (!g_version_label.empty())
            set_status((L"Update to " + g_version_label + L" complete. Launch Soldier of Fortune.").c_str());
        else
            set_status(L"Update complete. You can launch Soldier of Fortune.");
        EnableWindow(g_ok, TRUE);
        SetFocus(g_ok);
        return 0;
    case kMsgFailed: {
        set_status(L"Update failed. See the log for details.");
        set_percent(0);
        EnableWindow(g_ok, TRUE);
        std::wstring text = L"The update could not be applied.\n\nLog:\n" + g_log;
        MessageBoxW(hwnd, text.c_str(), L"SoF Buddy Update", MB_ICONERROR | MB_OK);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == kIdOk && HIWORD(wp) == BN_CLICKED) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (!g_ok || !IsWindowEnabled(g_ok)) return 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd_line, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring zip;
    if (argv && argc >= 2) zip = argv[1];
    if (argv) LocalFree(argv);

    if (!resolve_paths(zip)) {
        MessageBoxW(nullptr, L"Could not find sof_buddy\\update_from_zip.cmd next to the game folder.",
            L"SoF Buddy Update", MB_ICONERROR | MB_OK);
        return 1;
    }
    if (!zip.empty() && !file_exists(g_zip)) {
        MessageBoxW(nullptr, L"The update package was not found.", L"SoF Buddy Update", MB_ICONERROR | MB_OK);
        return 1;
    }

    WNDCLASSW wc = {};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);
    wc.lpszClassName = L"SoFBuddyUpdateWnd";
    RegisterClassW(&wc);

    const int win_w = 520;
    const int win_h = 440;
    RECT desk;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &desk, 0);
    const int x = desk.left + ((desk.right - desk.left) - win_w) / 2;
    const int y = desk.top + ((desk.bottom - desk.top) - win_h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"SoF Buddy Update",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, win_w, win_h, nullptr, nullptr, inst, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
