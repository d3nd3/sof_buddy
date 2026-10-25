// SoF Buddy — graphical Windows updater (XP+). Wraps update_from_zip.cmd with no console.
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>

#define MINIZ_IMPL
#include "miniminiz.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

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
    constexpr size_t kBuddyDirSuffix = 10; // \sof_buddy or /sof_buddy
    if (dir.size() >= kBuddyDirSuffix) {
        const std::wstring tail = dir.substr(dir.size() - kBuddyDirSuffix);
        if (_wcsicmp(tail.c_str(), L"\\sof_buddy") == 0 || _wcsicmp(tail.c_str(), L"/sof_buddy") == 0)
            g_root = dir.substr(0, dir.size() - kBuddyDirSuffix);
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

std::string wide_to_path(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, &out[0], n, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

bool ensure_parent_dirs_a(const std::string& file_path) {
    std::string normalized = file_path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    size_t slash = 0;
    while ((slash = normalized.find('/', slash)) != std::string::npos) {
        std::string dir = normalized.substr(0, slash++);
        if (dir.empty() || (dir.size() == 2 && dir[1] == ':')) continue;
        if (!CreateDirectoryA(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
            return false;
    }
    return true;
}

bool safe_zip_entry(const std::string& path) {
    return !path.empty() && path.find("..") == std::string::npos;
}

int g_shown_pct = 0;

void ui_progress(int pct, const wchar_t* msg) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    g_shown_pct = pct;
    PostMessageW(g_hwnd, kMsgProgress, pct, msg ? (LPARAM)_wcsdup(msg) : 0);
}

void pace_stage(int target, const wchar_t* status, DWORD min_ms) {
    if (target < g_shown_pct) target = g_shown_pct;
    const DWORD step_ms = 48;
    DWORD elapsed = 0;
    while (g_shown_pct < target || elapsed < min_ms) {
        if (g_shown_pct < target) {
            const int delta = target - g_shown_pct;
            g_shown_pct += (delta > 4) ? 2 : 1;
            if (g_shown_pct > target) g_shown_pct = target;
        }
        ui_progress(g_shown_pct, (elapsed == 0) ? status : nullptr);
        Sleep(step_ms);
        elapsed += step_ms;
    }
    ui_progress(target, status);
}

bool is_wine_runtime() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_unix_file_name") != nullptr;
}

void append_install_log(const char* msg) {
    if (!msg || !*msg) return;
    FILE* f = _wfopen(g_log.c_str(), L"ab");
    if (!f) return;
    std::fputs(msg, f);
    std::fputc('\n', f);
    std::fclose(f);
}

std::wstring read_install_log_text() {
    std::wstring out = L"Log file:\n" + g_log + L"\n\n";
    FILE* f = _wfopen(g_log.c_str(), L"rb");
    if (!f) return out + L"(log empty or missing)";
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf) - 1, f)) > 0) {
        buf[n] = '\0';
        wchar_t wbuf[4096] = {};
        MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, 4096);
        if (!wbuf[0])
            MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf, 4096);
        out += wbuf;
    }
    std::fclose(f);
    return out;
}

bool is_main_dll_entry(const std::string& entry) {
    std::string e = entry;
    std::replace(e.begin(), e.end(), '\\', '/');
    for (auto& c : e)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e == "sof_buddy.dll";
}

std::wstring path_full_wide(const std::wstring& p) {
    wchar_t full[MAX_PATH] = {};
    if (GetFullPathNameW(p.c_str(), MAX_PATH, full, nullptr))
        return full;
    return p;
}

std::wstring path_a_to_wide(const std::string& p) {
    if (p.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_ACP, 0, p.c_str(), -1, nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_ACP, 0, p.c_str(), -1, &out[0], n);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

std::wstring running_module_path() {
    wchar_t mod[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, mod, MAX_PATH)) return std::wstring();
    return path_full_wide(mod);
}

bool is_running_image_path(const std::string& out_a) {
    const std::wstring self = running_module_path();
    if (self.empty()) return false;
    return _wcsicmp(path_full_wide(path_a_to_wide(out_a)).c_str(), self.c_str()) == 0;
}

bool extract_one_file(mz_zip_archive& za, mz_uint idx, const std::string& out, bool main_dll) {
    const int attempts = main_dll ? 24 : 1;
    for (int t = 0; t < attempts; ++t) {
        if (main_dll && t > 0)
            Sleep(250);
        if (mz_zip_reader_extract_to_file(&za, idx, out.c_str(), 0))
            return true;
    }
    char line[512];
    std::snprintf(line, sizeof(line), "extract failed: %s", out.c_str());
    append_install_log(line);
    return false;
}

void prune_sofbuddy_cfg_tree(const std::wstring& dir) {
    std::wstring spec = dir + L"\\*";
    WIN32_FIND_DATAW fd = {};
    HANDLE h = FindFirstFileW(spec.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == L'.' &&
            (fd.cFileName[1] == L'\0' || (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0')))
            continue;
        std::wstring path = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            prune_sofbuddy_cfg_tree(path);
        else if (_wcsicmp(fd.cFileName, L"sofbuddy.cfg") == 0)
            DeleteFileW(path.c_str());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

bool extract_zip_inprocess(const std::wstring& zip_w, const std::wstring& root_w) {
    const std::string zip = wide_to_path(zip_w);
    const std::string root = wide_to_path(root_w);
    if (zip.empty() || root.empty()) {
        append_install_log("extract: empty zip or root path");
        return false;
    }

    mz_zip_archive za = {};
    if (!mz_zip_reader_init_file(&za, zip.c_str(), 0)) {
        append_install_log("extract: mz_zip_reader_init_file failed");
        return false;
    }
    const mz_uint nfiles = mz_zip_reader_get_num_files(&za);
    mz_uint done = 0;
    for (mz_uint pass = 0; pass < 2; ++pass) {
        for (mz_uint i = 0; i < nfiles; ++i) {
            mz_zip_archive_file_stat st = {};
            if (!mz_zip_reader_file_stat(&za, i, &st) || st.m_is_directory) continue;
            std::string entry(st.m_filename);
            std::replace(entry.begin(), entry.end(), '\\', '/');
            while (!entry.empty() && entry.front() == '/') entry.erase(entry.begin());
            if (!safe_zip_entry(entry)) continue;
            const bool main_dll = is_main_dll_entry(entry);
            if ((pass == 0 && main_dll) || (pass == 1 && !main_dll)) continue;
            std::string out = root + "\\" + entry;
            std::replace(out.begin(), out.end(), '/', '\\');
            if (!ensure_parent_dirs_a(out)) {
                mz_zip_reader_end(&za);
                append_install_log("extract: mkdir failed");
                return false;
            }
            if (is_running_image_path(out)) {
                char line[512];
                std::snprintf(line, sizeof(line), "skipped in-use file: %s", out.c_str());
                append_install_log(line);
                continue;
            }
            if (!extract_one_file(za, i, out, main_dll)) {
                mz_zip_reader_end(&za);
                return false;
            }
            ++done;
            if (nfiles > 0) {
                const int cap = 28 + static_cast<int>(done * 44 / nfiles);
                while (g_shown_pct < cap) {
                    ++g_shown_pct;
                    ui_progress(g_shown_pct, L"Extracting update files...");
                    Sleep(42);
                }
            }
        }
    }
    mz_zip_reader_end(&za);
    if (!file_exists(root_w + L"\\sof_buddy.dll")) {
        append_install_log("extract: sof_buddy.dll missing after extract");
        return false;
    }
    return true;
}

bool install_via_cmd(const bool fresh) {
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
                      nullptr, g_root.c_str(), &si, &pi))
        return false;
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
    return exit_code == 0;
}

DWORD WINAPI worker_thread(LPVOID) {
    DeleteFileW(g_progress_file.c_str());
    DeleteFileW(g_log.c_str());
    CreateDirectoryW((g_root + L"\\sof_buddy\\update").c_str(), nullptr);

    if (!g_zip.empty()) {
        wchar_t full[MAX_PATH] = {};
        if (GetFullPathNameW(g_zip.c_str(), MAX_PATH, full, nullptr))
            g_zip = full;
    }

    const bool fresh = (g_fresh && SendMessageW(g_fresh, BM_GETCHECK, 0, 0) == BST_CHECKED);
    g_shown_pct = 5;
    ui_progress(5, L"Preparing installation...");
    if (is_wine_runtime())
        Sleep(400);
    pace_stage(14, L"Reading update package...", 520);
    pace_stage(22, L"Update package ready", 480);

    bool ok = false;
    if (!g_zip.empty() && file_exists(g_zip)) {
        pace_stage(28, L"Extracting update files...", 320);
        ok = extract_zip_inprocess(g_zip, g_root);
    } else {
        append_install_log("extract: update zip not found");
    }
    if (ok) {
        pace_stage(78, L"Verifying installation...", 450);
        pace_stage(86, L"Applying options...", 380);
        if (fresh) prune_sofbuddy_cfg_tree(g_root);
        DeleteFileW(g_zip.c_str());
        DeleteFileW((g_root + L"\\sof_buddy\\update\\pending_update.txt").c_str());
        DeleteFileW((g_root + L"\\sof_buddy\\update\\install_context.env").c_str());
        pace_stage(94, L"Cleaning up...", 360);
        pace_stage(100, L"Installation complete", 500);
    } else if (!is_wine_runtime() && !g_script.empty() && file_exists(g_script)) {
        ok = install_via_cmd(fresh);
    } else if (!ok) {
        append_install_log(is_wine_runtime()
            ? "Wine: in-process extract failed (cmd fallback disabled for spaced install paths)"
            : "in-process extract failed");
    }

    PostMessageW(g_hwnd, ok ? kMsgDone : kMsgFailed, 0, 0);
    return 0;
}

void start_worker() {
    if (g_worker) return;
    g_shown_pct = 0;
    set_status(L"Installing update — please wait...");
    set_percent(0);
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
    case kMsgDone: {
        set_percent(100);
        if (g_headline)
            SetWindowTextW(g_headline, L"Installation complete");
        std::wstring done_msg = L"SoF Buddy ";
        if (!g_version_label.empty())
            done_msg += g_version_label;
        else
            done_msg += L"update";
        done_msg += L" is installed.\r\n\r\nLaunch Soldier of Fortune when you are ready.";
        if (!g_version_label.empty())
            set_status((L"Success — " + g_version_label + L" is ready. Close this window when finished.").c_str());
        else
            set_status(L"Success — close this window when finished.");
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        FlashWindow(hwnd, TRUE);
        MessageBeep(MB_OK);
        MessageBoxW(hwnd, done_msg.c_str(), L"SoF Buddy Update — complete",
            MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
        EnableWindow(g_ok, TRUE);
        SetFocus(g_ok);
        return 0;
    }
    case kMsgFailed: {
        set_status(L"Update failed. See the log for details.");
        set_percent(0);
        EnableWindow(g_ok, TRUE);
        std::wstring text = L"The update could not be applied.\n\n" + read_install_log_text();
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
    SetForegroundWindow(hwnd);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
