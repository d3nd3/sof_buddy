#include "update_command.h"
#include "feature_config.h"

#include "sof_compat.h"
#include "util.h"
#include "version.h"

#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr const char* kUpdateApiUrlGitHub = "https://api.github.com/repos/d3nd3/sof_buddy/releases/latest";
constexpr const char* kUpdateReleasesUrlGitHub = "https://github.com/d3nd3/sof_buddy/releases/latest";
constexpr const char* kUpdateApiUrlSofVault = "http://sofvault.org/sof_buddy/releases/latest.json";
constexpr const char* kUpdateReleasesUrlSofVault = "http://sofvault.org/sof_buddy/releases/latest";

#if defined(SOFBUDDY_XP_BUILD)
constexpr const char* kUpdateApiUrl = kUpdateApiUrlSofVault;
constexpr const char* kUpdateReleasesUrl = kUpdateReleasesUrlSofVault;
#else
constexpr const char* kUpdateApiUrl = kUpdateApiUrlGitHub;
constexpr const char* kUpdateReleasesUrl = kUpdateReleasesUrlGitHub;
#endif
constexpr const char* kUpdateOutputDir = "sof_buddy/update";
constexpr DWORD kUpdateTimeoutMs = 8000;

constexpr const char* kCvarUpdateStatus = "_sofbuddy_update_status";
constexpr const char* kCvarUpdateLatest = "_sofbuddy_update_latest";
constexpr const char* kCvarUpdateDownloadPath = "_sofbuddy_update_download_path";
constexpr const char* kCvarUpdateCheckedUtc = "_sofbuddy_update_checked_utc";
constexpr const char* kCvarUpdateCheckStartup = "_sofbuddy_update_check_startup";
constexpr const char* kCvarUpdateApiUrl = "_sofbuddy_update_api_url";
constexpr const char* kCvarUpdateReleasesUrl = "_sofbuddy_update_releases_url";
constexpr const char* kCvarOpenUrlStatus = "_sofbuddy_openurl_status";
constexpr const char* kCvarOpenUrlLast = "_sofbuddy_openurl_last";
bool g_startup_update_prompt_pending = false;

constexpr const char* kAllowedSocialHosts[] = {
    "github.com",
    "discord.com",
    "discord.gg",
    "sof1.megalag.org",
    "sof1.org",
    "www.sof1.org",
};

struct ParsedUrl {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool secure = true;
};

struct ReleaseInfo {
    std::string tag_name;
    std::string html_url;
    std::string zip_url;
};

bool parse_url(const std::string& url, ParsedUrl& out, std::string& error);

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return std::wstring();
    int needed = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (needed <= 0) return std::wstring();
    std::wstring out(static_cast<size_t>(needed), L'\0');
    int written = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &out[0], needed);
    if (written <= 0) return std::wstring();
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) return std::string();
    int needed = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return std::string();
    std::string out(static_cast<size_t>(needed), '\0');
    int written = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &out[0], needed, nullptr, nullptr);
    if (written <= 0) return std::string();
    return out;
}

std::string format_error_code(const char* prefix, DWORD code) {
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "%s (Win32 error %lu)", prefix, static_cast<unsigned long>(code));
    return std::string(buffer);
}

bool is_tls_secure_failure_error(DWORD code) {
    return code == ERROR_WINHTTP_SECURE_FAILURE;
}

std::string get_update_cvar_or_default(const char* name, const char* fallback) {
    if (!fallback) fallback = "";
    if (!name || !name[0] || !orig_Cvar_Get) return std::string(fallback);

    cvar_t* c = orig_Cvar_Get(name, fallback, CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (!c || !c->string || !c->string[0]) return std::string(fallback);
    return std::string(c->string);
}

std::string get_update_api_url() {
    return get_update_cvar_or_default(kCvarUpdateApiUrl, kUpdateApiUrl);
}

std::string get_update_releases_url() {
    return get_update_cvar_or_default(kCvarUpdateReleasesUrl, kUpdateReleasesUrl);
}

bool cvar_is_empty_or_value(cvar_t* c, const char* expected) {
    const char* s = (c && c->string) ? c->string : "";
    if (!s[0]) return true;
    if (!expected) return false;
    return std::strcmp(s, expected) == 0;
}

void append_tls_failure_hint(std::string& error, const ParsedUrl& parsed) {
    if (!parsed.secure) return;

    const std::string host = to_lower_ascii(wide_to_utf8(parsed.host));
    if (host.find("github.com") == std::string::npos &&
        host.find("githubusercontent.com") == std::string::npos) {
        return;
    }

    error += ". TLS handshake/certificate failed. GitHub requires modern TLS; on Windows XP WinHTTP this often fails. ";
    error += "Use manual update, or set _sofbuddy_update_api_url to an XP-compatible mirror/proxy feed.";
}

bool ensure_parent_dirs(const std::string& file_path) {
    std::string normalized = file_path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    size_t slash = 0;
    while ((slash = normalized.find('/', slash)) != std::string::npos) {
        std::string dir = normalized.substr(0, slash);
        ++slash;
        if (dir.empty()) continue;
        if (dir.size() == 2 && dir[1] == ':') continue;
        if (!CreateDirectoryA(dir.c_str(), nullptr)) {
            DWORD e = GetLastError();
            if (e != ERROR_ALREADY_EXISTS) return false;
        }
    }
    return true;
}

std::string current_utc_timestamp() {
    SYSTEMTIME st = {};
    GetSystemTime(&st);
    char out[64];
    std::snprintf(out, sizeof(out), "%04u-%02u-%02uT%02u:%02u:%02uZ",
        static_cast<unsigned>(st.wYear),
        static_cast<unsigned>(st.wMonth),
        static_cast<unsigned>(st.wDay),
        static_cast<unsigned>(st.wHour),
        static_cast<unsigned>(st.wMinute),
        static_cast<unsigned>(st.wSecond));
    return std::string(out);
}

void set_update_cvar(const char* name, const std::string& value) {
    if (!name || !orig_Cvar_Set2) return;
    orig_Cvar_Set2(const_cast<char*>(name), const_cast<char*>(value.c_str()), true);
}

void set_update_status(const std::string& status) {
    set_update_cvar(kCvarUpdateStatus, status);
}

void set_openurl_status(const std::string& status) {
    set_update_cvar(kCvarOpenUrlStatus, status);
}

bool host_matches_or_subdomain(const std::string& host_lower, const char* allowed) {
    if (!allowed || !allowed[0]) return false;
    const std::string allow = to_lower_ascii(std::string(allowed));
    if (host_lower == allow) return true;
    if (host_lower.size() <= allow.size()) return false;
    const size_t suffix_pos = host_lower.size() - allow.size();
    if (host_lower.compare(suffix_pos, allow.size(), allow) != 0) return false;
    return host_lower[suffix_pos - 1] == '.';
}

bool is_allowed_social_host(const std::string& host_lower) {
    for (size_t i = 0; i < sizeof(kAllowedSocialHosts) / sizeof(kAllowedSocialHosts[0]); ++i) {
        if (host_matches_or_subdomain(host_lower, kAllowedSocialHosts[i])) return true;
    }
    return false;
}

bool validate_social_url(const std::string& url, std::string& out_host_lower, std::string& error) {
    if (url.empty()) {
        error = "URL is empty";
        return false;
    }
    if (url.size() > 1024) {
        error = "URL is too long";
        return false;
    }

    ParsedUrl parsed;
    if (!parse_url(url, parsed, error)) return false;
    if (!parsed.secure) {
        error = "Only https:// links are allowed";
        return false;
    }

    std::string host = to_lower_ascii(wide_to_utf8(parsed.host));
    if (host.empty()) {
        error = "URL host is invalid";
        return false;
    }
    if (!is_allowed_social_host(host)) {
        error = "Host is not in trusted social links list";
        return false;
    }

    out_host_lower = host;
    return true;
}

bool parse_url(const std::string& url, ParsedUrl& out, std::string& error) {
    std::wstring wide = utf8_to_wide(url);
    if (wide.empty()) {
        error = "URL conversion failed";
        return false;
    }

    URL_COMPONENTSW comp = {};
    comp.dwStructSize = sizeof(comp);
    comp.dwSchemeLength = static_cast<DWORD>(-1);
    comp.dwHostNameLength = static_cast<DWORD>(-1);
    comp.dwUrlPathLength = static_cast<DWORD>(-1);
    comp.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &comp)) {
        error = format_error_code("WinHttpCrackUrl failed", GetLastError());
        return false;
    }

    if (!comp.lpszHostName || comp.dwHostNameLength == 0) {
        error = "URL host missing";
        return false;
    }

    out.host.assign(comp.lpszHostName, comp.dwHostNameLength);

    std::wstring path;
    if (comp.lpszUrlPath && comp.dwUrlPathLength > 0)
        path.assign(comp.lpszUrlPath, comp.dwUrlPathLength);
    else
        path = L"/";
    if (comp.lpszExtraInfo && comp.dwExtraInfoLength > 0)
        path.append(comp.lpszExtraInfo, comp.dwExtraInfoLength);

    out.path = path;
    out.port = comp.nPort;
    out.secure = (comp.nScheme == INTERNET_SCHEME_HTTPS);
    return true;
}

bool http_begin_get(
    const std::string& url,
    bool github_api_headers,
    HINTERNET& out_session,
    HINTERNET& out_connect,
    HINTERNET& out_request,
    DWORD& out_status_code,
    std::string& error
) {
    out_session = nullptr;
    out_connect = nullptr;
    out_request = nullptr;
    out_status_code = 0;

    ParsedUrl parsed;
    if (!parse_url(url, parsed, error)) return false;

    HINTERNET session = WinHttpOpen(
        L"SoFBuddyUpdater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!session) {
        error = format_error_code("WinHttpOpen failed", GetLastError());
        return false;
    }

    WinHttpSetTimeouts(
        session,
        static_cast<int>(kUpdateTimeoutMs),
        static_cast<int>(kUpdateTimeoutMs),
        static_cast<int>(kUpdateTimeoutMs),
        static_cast<int>(kUpdateTimeoutMs)
    );

    HINTERNET connect = WinHttpConnect(session, parsed.host.c_str(), parsed.port, 0);
    if (!connect) {
        error = format_error_code("WinHttpConnect failed", GetLastError());
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD open_flags = parsed.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(
        connect,
        L"GET",
        parsed.path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        open_flags
    );
    if (!request) {
        error = format_error_code("WinHttpOpenRequest failed", GetLastError());
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    const wchar_t* headers = WINHTTP_NO_ADDITIONAL_HEADERS;
    DWORD headers_len = 0;
    if (github_api_headers) {
        headers = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
        headers_len = static_cast<DWORD>(-1);
    }

    if (!WinHttpSendRequest(request, headers, headers_len, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        const DWORD e = GetLastError();
        error = format_error_code("WinHttpSendRequest failed", e);
        if (is_tls_secure_failure_error(e))
            append_tls_failure_hint(error, parsed);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        const DWORD e = GetLastError();
        error = format_error_code("WinHttpReceiveResponse failed", e);
        if (is_tls_secure_failure_error(e))
            append_tls_failure_hint(error, parsed);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD status = 0;
    DWORD status_len = sizeof(status);
    if (!WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &status_len,
            WINHTTP_NO_HEADER_INDEX
        )) {
        error = format_error_code("WinHttpQueryHeaders(status) failed", GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    out_session = session;
    out_connect = connect;
    out_request = request;
    out_status_code = status;
    return true;
}

bool http_read_all(HINTERNET request, std::vector<uint8_t>& out, std::string& error) {
    out.clear();
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            error = format_error_code("WinHttpQueryDataAvailable failed", GetLastError());
            return false;
        }
        if (available == 0) return true;

        size_t old_size = out.size();
        out.resize(old_size + static_cast<size_t>(available));

        DWORD total_read = 0;
        while (total_read < available) {
            DWORD just_read = 0;
            if (!WinHttpReadData(
                    request,
                    out.data() + old_size + total_read,
                    available - total_read,
                    &just_read
                )) {
                error = format_error_code("WinHttpReadData failed", GetLastError());
                return false;
            }
            if (just_read == 0) break;
            total_read += just_read;
        }

        if (total_read < available)
            out.resize(old_size + static_cast<size_t>(total_read));

        if (total_read == 0) return true;
    }
}

bool http_write_all(HINTERNET request, FILE* fp, size_t& out_bytes, std::string& error) {
    out_bytes = 0;
    std::vector<uint8_t> buffer;

    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            error = format_error_code("WinHttpQueryDataAvailable failed", GetLastError());
            return false;
        }
        if (available == 0) return true;

        buffer.resize(static_cast<size_t>(available));

        DWORD total_read = 0;
        while (total_read < available) {
            DWORD just_read = 0;
            if (!WinHttpReadData(
                    request,
                    buffer.data() + total_read,
                    available - total_read,
                    &just_read
                )) {
                error = format_error_code("WinHttpReadData failed", GetLastError());
                return false;
            }
            if (just_read == 0) break;
            total_read += just_read;
        }

        if (total_read == 0) return true;

        size_t wrote = std::fwrite(buffer.data(), 1, static_cast<size_t>(total_read), fp);
        if (wrote != static_cast<size_t>(total_read)) {
            error = "Failed writing downloaded bytes to disk";
            return false;
        }
        out_bytes += wrote;
    }
}

bool json_read_string(const std::string& json, size_t quote_pos, std::string& out, size_t* out_next_pos) {
    if (quote_pos >= json.size() || json[quote_pos] != '"') return false;

    out.clear();
    size_t i = quote_pos + 1;
    while (i < json.size()) {
        char ch = json[i++];
        if (ch == '"') {
            if (out_next_pos) *out_next_pos = i;
            return true;
        }
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }
        if (i >= json.size()) return false;

        char esc = json[i++];
        switch (esc) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u':
                if (i + 4 > json.size()) return false;
                // Keep parser lightweight; placeholders are fine for current fields.
                out.push_back('?');
                i += 4;
                break;
            default:
                out.push_back(esc);
                break;
        }
    }
    return false;
}

bool json_find_string_field(
    const std::string& json,
    const char* key,
    size_t start_pos,
    std::string& out,
    size_t* out_next_search
) {
    if (!key || !key[0]) return false;

    std::string needle = "\"";
    needle += key;
    needle += "\"";

    size_t key_pos = json.find(needle, start_pos);
    if (key_pos == std::string::npos) return false;

    size_t colon = json.find(':', key_pos + needle.size());
    if (colon == std::string::npos) return false;

    size_t value_pos = colon + 1;
    while (value_pos < json.size() && std::isspace(static_cast<unsigned char>(json[value_pos])))
        ++value_pos;
    if (value_pos >= json.size() || json[value_pos] != '"') return false;

    size_t next_pos = 0;
    if (!json_read_string(json, value_pos, out, &next_pos)) return false;
    if (out_next_search) *out_next_search = next_pos;
    return true;
}

std::string zip_asset_name_from_url(const std::string& url) {
    if (url.empty()) return std::string();

    size_t end = url.find_first_of("?#");
    if (end == std::string::npos) end = url.size();
    if (end == 0) return std::string();

    size_t slash = url.rfind('/', end - 1);
    size_t start = (slash == std::string::npos) ? 0 : (slash + 1);
    if (start >= end) return std::string();

    return to_lower_ascii(url.substr(start, end - start));
}

int score_release_zip_candidate(const std::string& url) {
    const std::string name = zip_asset_name_from_url(url);
    if (name.size() < 4 || name.compare(name.size() - 4, 4, ".zip") != 0)
        return std::numeric_limits<int>::min();

    const bool is_debug = (name.find("debug") != std::string::npos);
    const bool is_release = (name.find("release") != std::string::npos);
    const bool has_sofbuddy = (name.find("sof_buddy") != std::string::npos) ||
                              (name.find("sofbuddy") != std::string::npos);
    const bool is_windows = (name.find("windows") != std::string::npos);
    const bool is_linux_wine = (name.find("linux") != std::string::npos) ||
                               (name.find("wine") != std::string::npos);
    const bool is_xp = (name.find("xp") != std::string::npos);

    int score = 0;
    if (is_release) score += 400;
    if (has_sofbuddy) score += 120;
    if (is_windows) score += 30;
    if (is_linux_wine) score += 20;
    if (is_xp) score -= 40;
    if (is_debug) score -= 500;

    if (name == "release_windows.zip") score += 200;
    else if (name == "release_linux_wine.zip") score += 180;
    else if (name == "release_windows_xp.zip") score += 100;
    else if (name == "debug_windows.zip") score -= 100;
    else if (name == "debug_windows_xp.zip") score -= 150;

    return score;
}

std::vector<int> parse_version_parts(const std::string& version_text) {
    std::vector<int> parts;
    size_t i = 0;
    while (i < version_text.size() && !std::isdigit(static_cast<unsigned char>(version_text[i]))) ++i;
    if (i >= version_text.size()) return parts;

    while (i < version_text.size()) {
        if (!std::isdigit(static_cast<unsigned char>(version_text[i]))) break;
        int value = 0;
        while (i < version_text.size() && std::isdigit(static_cast<unsigned char>(version_text[i]))) {
            value = (value * 10) + (version_text[i] - '0');
            ++i;
        }
        parts.push_back(value);
        if (i >= version_text.size() || version_text[i] != '.') break;
        ++i;
    }

    while (!parts.empty() && parts.back() == 0) parts.pop_back();
    return parts;
}

int compare_versions(const std::string& local_version, const std::string& remote_version) {
    std::vector<int> local_parts = parse_version_parts(local_version);
    std::vector<int> remote_parts = parse_version_parts(remote_version);
    size_t max_len = std::max(local_parts.size(), remote_parts.size());
    for (size_t i = 0; i < max_len; ++i) {
        int a = (i < local_parts.size()) ? local_parts[i] : 0;
        int b = (i < remote_parts.size()) ? remote_parts[i] : 0;
        if (a < b) return -1;
        if (a > b) return 1;
    }
    return 0;
}

std::string sanitize_tag_for_filename(const std::string& tag_name) {
    if (tag_name.empty()) return "latest";
    std::string out = tag_name;
    for (size_t i = 0; i < out.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(out[i]);
        if (std::isalnum(c) || c == '.' || c == '-' || c == '_') continue;
        out[i] = '_';
    }
    return out;
}

std::string make_update_zip_path(const std::string& tag_name) {
    std::string safe = sanitize_tag_for_filename(tag_name);
    return std::string(kUpdateOutputDir) + "/sof_buddy-" + safe + ".zip";
}

bool write_update_readme(const ReleaseInfo& info, const std::string& zip_path) {
    const std::string releases_url = get_update_releases_url();
    const std::string readme_path = std::string(kUpdateOutputDir) + "/README.txt";
    if (!ensure_parent_dirs(readme_path)) return false;

    FILE* fp = std::fopen(readme_path.c_str(), "wb");
    if (!fp) return false;

    std::string text;
    text += "SoF Buddy update downloaded.\n\n";
    text += "Current loaded version: ";
    text += SOFBUDDY_VERSION;
    text += "\nLatest release tag: ";
    text += info.tag_name.empty() ? "unknown" : info.tag_name;
    text += "\nRelease page: ";
    text += info.html_url.empty() ? releases_url : info.html_url;
    text += "\nDownloaded zip: ";
    text += zip_path;
    text += "\n\n";
    text += "Apply steps:\n";
    text += "1. Close Soldier of Fortune completely.\n";
    text += "2. Extract the zip into your SoF root and overwrite existing files.\n";
    text += "3. Launch the game again using your normal launcher/arguments.\n";

    std::fwrite(text.data(), 1, text.size(), fp);
    std::fclose(fp);
    return true;
}

bool fetch_latest_release(ReleaseInfo& out, std::string& error) {
    const std::string api_url = get_update_api_url();
    const std::string releases_url = get_update_releases_url();

    HINTERNET session = nullptr;
    HINTERNET connect = nullptr;
    HINTERNET request = nullptr;
    DWORD status = 0;

    if (!http_begin_get(api_url, true, session, connect, request, status, error))
        return false;

    std::vector<uint8_t> payload;
    bool ok = http_read_all(request, payload, error);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    if (!ok) return false;

    std::string body(payload.begin(), payload.end());
    if (status != 200) {
        if (body.size() > 200) body.resize(200);
        error = "GitHub API returned HTTP " + std::to_string(status) + ": " + body;
        return false;
    }

    if (!json_find_string_field(body, "tag_name", 0, out.tag_name, nullptr) &&
        !json_find_string_field(body, "version", 0, out.tag_name, nullptr) &&
        !json_find_string_field(body, "tag", 0, out.tag_name, nullptr)) {
        error = "Update feed missing version tag (expected tag_name/version/tag)";
        return false;
    }
    if (!json_find_string_field(body, "html_url", 0, out.html_url, nullptr) &&
        !json_find_string_field(body, "release_url", 0, out.html_url, nullptr)) {
        out.html_url = releases_url;
    }

    out.zip_url.clear();
    int best_score = std::numeric_limits<int>::min();
    size_t search = 0;
    std::string candidate;
    while (json_find_string_field(body, "browser_download_url", search, candidate, &search)) {
        const int score = score_release_zip_candidate(candidate);
        if (score <= best_score) continue;
        best_score = score;
        out.zip_url = candidate;
    }
    if (out.zip_url.empty()) {
        json_find_string_field(body, "zip_url", 0, out.zip_url, nullptr) ||
            json_find_string_field(body, "download_url", 0, out.zip_url, nullptr) ||
            json_find_string_field(body, "asset_url", 0, out.zip_url, nullptr);
    }

    return true;
}

bool download_release_zip(const std::string& url, const std::string& out_path, size_t& out_bytes, std::string& error) {
    if (url.empty()) {
        error = "No release zip URL found";
        return false;
    }
    if (!ensure_parent_dirs(out_path)) {
        error = format_error_code("Failed creating update directory", GetLastError());
        return false;
    }

    FILE* fp = std::fopen(out_path.c_str(), "wb");
    if (!fp) {
        error = "Failed opening output zip path for writing";
        return false;
    }

    HINTERNET session = nullptr;
    HINTERNET connect = nullptr;
    HINTERNET request = nullptr;
    DWORD status = 0;

    if (!http_begin_get(url, false, session, connect, request, status, error)) {
        std::fclose(fp);
        DeleteFileA(out_path.c_str());
        return false;
    }

    bool ok = true;
    if (status != 200) {
        error = "Download server returned HTTP " + std::to_string(status);
        ok = false;
    } else {
        ok = http_write_all(request, fp, out_bytes, error);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    std::fclose(fp);

    if (!ok) {
        DeleteFileA(out_path.c_str());
        return false;
    }
    return true;
}

void print_update_usage() {
    PrintOut(PRINT_DEV, "sofbuddy_update: checks GitHub releases for updates.\n");
    PrintOut(PRINT_DEV, "Usage:\n");
    PrintOut(PRINT_DEV, "  sofbuddy_update                (check only)\n");
    PrintOut(PRINT_DEV, "  sofbuddy_update download       (check + download if newer)\n");
    PrintOut(PRINT_DEV, "  sofbuddy_update download force (always download latest zip)\n");
    PrintOut(PRINT_DEV, "Optional overrides (for XP/proxy mirrors):\n");
    PrintOut(PRINT_DEV, "  set _sofbuddy_update_api_url <url>\n");
    PrintOut(PRINT_DEV, "  set _sofbuddy_update_releases_url <url>\n");
}

void queue_startup_update_prompt() {
#if FEATURE_INTERNAL_MENUS
    g_startup_update_prompt_pending = true;
#endif
}

} // namespace

static void sofbuddy_run_update_flow(bool do_download, bool force_download, bool startup_check) {
    const std::string releases_url = get_update_releases_url();
    set_update_status("checking github releases");
    set_update_cvar(kCvarUpdateCheckedUtc, current_utc_timestamp());

    ReleaseInfo latest;
    std::string error;
    if (!fetch_latest_release(latest, error)) {
        set_update_status("check failed");
        PrintOut(PRINT_BAD, "sofbuddy_update: %s\n", error.c_str());
        PrintOut(PRINT_BAD, "You can still check manually: %s\n", releases_url.c_str());
        return;
    }

    set_update_cvar(kCvarUpdateLatest, latest.tag_name);

    int cmp = compare_versions(SOFBUDDY_VERSION, latest.tag_name);
    if (cmp < 0) {
        std::string status = "update available (" + latest.tag_name + ")";
        set_update_status(status);
        if (startup_check)
            PrintOut(PRINT_DEV, "Startup check: update available: %s -> %s\n", SOFBUDDY_VERSION, latest.tag_name.c_str());
        else
            PrintOut(PRINT_DEV, "Update available: %s -> %s\n", SOFBUDDY_VERSION, latest.tag_name.c_str());
        if (!latest.html_url.empty())
            PrintOut(PRINT_DEV, "Release page: %s\n", latest.html_url.c_str());
        else
            PrintOut(PRINT_DEV, "Release page: %s\n", releases_url.c_str());
        if (startup_check)
            queue_startup_update_prompt();
    } else if (cmp == 0) {
        set_update_status("up to date");
        if (startup_check)
            PrintOut(PRINT_DEV, "Startup check: SoF Buddy is up to date (%s).\n", SOFBUDDY_VERSION);
        else
            PrintOut(PRINT_DEV, "SoF Buddy is up to date (%s).\n", SOFBUDDY_VERSION);
    } else {
        set_update_status("local build is newer");
        if (startup_check) {
            PrintOut(PRINT_DEV, "Startup check: local build (%s) is newer than latest tagged release (%s).\n",
                SOFBUDDY_VERSION, latest.tag_name.c_str());
        } else {
            PrintOut(PRINT_DEV, "Local build (%s) is newer than latest tagged release (%s).\n",
                SOFBUDDY_VERSION, latest.tag_name.c_str());
        }
    }

    if (!do_download) {
        if (cmp < 0) {
            PrintOut(PRINT_DEV, "Run `sofbuddy_update download` to fetch the latest zip now.\n");
        }
        return;
    }

    if (!force_download && cmp >= 0) {
        PrintOut(PRINT_DEV, "Skipping download (no newer release). Use `sofbuddy_update download force` if you still want the zip.\n");
        return;
    }

    if (latest.zip_url.empty()) {
        set_update_status("no zip asset found");
        PrintOut(PRINT_BAD, "Latest release does not expose a downloadable .zip asset.\n");
        if (!latest.html_url.empty())
            PrintOut(PRINT_BAD, "Check assets manually: %s\n", latest.html_url.c_str());
        return;
    }

    const std::string zip_path = make_update_zip_path(latest.tag_name);
    size_t written_bytes = 0;
    set_update_status("downloading release zip");
    PrintOut(PRINT_DEV, "Downloading %s\n", latest.zip_url.c_str());
    if (!download_release_zip(latest.zip_url, zip_path, written_bytes, error)) {
        set_update_status("download failed");
        PrintOut(PRINT_BAD, "sofbuddy_update download failed: %s\n", error.c_str());
        return;
    }

    set_update_cvar(kCvarUpdateDownloadPath, zip_path);
    set_update_status("downloaded, close game to install");

    write_update_readme(latest, zip_path);

    PrintOut(PRINT_DEV, "Downloaded %zu bytes to %s\n", written_bytes, zip_path.c_str());
    PrintOut(PRINT_DEV, "Close SoF completely before replacing sof_buddy.dll (it is currently loaded).\n");
    PrintOut(PRINT_DEV, "Then run sof_buddy/update_from_zip.cmd (Windows) or sof_buddy/update_from_zip.sh (Linux/Wine).\n");
    PrintOut(PRINT_DEV, "If you prefer manual install: extract that zip into your SoF root and relaunch normally.\n");
}

void sofbuddy_update_init(void) {
    if (!orig_Cvar_Get) return;
    orig_Cvar_Get(kCvarUpdateStatus, "idle", 0, nullptr);
    orig_Cvar_Get(kCvarUpdateLatest, "", 0, nullptr);
    orig_Cvar_Get(kCvarUpdateDownloadPath, "", 0, nullptr);
    orig_Cvar_Get(kCvarUpdateCheckedUtc, "", 0, nullptr);
    orig_Cvar_Get(kCvarUpdateCheckStartup, "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    cvar_t* api_url_cvar = orig_Cvar_Get(kCvarUpdateApiUrl, kUpdateApiUrl, CVAR_SOFBUDDY_ARCHIVE, nullptr);
    cvar_t* releases_url_cvar = orig_Cvar_Get(kCvarUpdateReleasesUrl, kUpdateReleasesUrl, CVAR_SOFBUDDY_ARCHIVE, nullptr);

#if defined(SOFBUDDY_XP_BUILD)
    // XP builds should default to the sofvault mirror. Preserve explicit user customizations.
    if (orig_Cvar_Set2) {
        if (cvar_is_empty_or_value(api_url_cvar, kUpdateApiUrlGitHub))
            orig_Cvar_Set2(const_cast<char*>(kCvarUpdateApiUrl), const_cast<char*>(kUpdateApiUrlSofVault), true);
        if (cvar_is_empty_or_value(releases_url_cvar, kUpdateReleasesUrlGitHub))
            orig_Cvar_Set2(const_cast<char*>(kCvarUpdateReleasesUrl), const_cast<char*>(kUpdateReleasesUrlSofVault), true);
    }
#endif

    orig_Cvar_Get(kCvarOpenUrlStatus, "idle", 0, nullptr);
    orig_Cvar_Get(kCvarOpenUrlLast, "", 0, nullptr);
}

void sofbuddy_update_maybe_check_startup(void) {
    static bool s_checked_startup = false;
    if (s_checked_startup) return;
    s_checked_startup = true;

    if (!orig_Cvar_Get) return;
    cvar_t* startup_check = orig_Cvar_Get(kCvarUpdateCheckStartup, "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (!startup_check || startup_check->value == 0.0f) return;

    PrintOut(PRINT_DEV, "sofbuddy_update: startup check is enabled.\n");
    sofbuddy_run_update_flow(false, false, true);
}

bool sofbuddy_update_consume_startup_prompt_request(void) {
#if FEATURE_INTERNAL_MENUS
    if (!g_startup_update_prompt_pending)
        return false;
    g_startup_update_prompt_pending = false;
    return true;
#else
    return false;
#endif
}

void Cmd_SoFBuddy_Update_f(void) {
    if (!orig_Cmd_Argc || !orig_Cmd_Argv) return;

    bool do_download = false;
    bool force_download = false;

    int argc = orig_Cmd_Argc();
    for (int i = 1; i < argc; ++i) {
        const char* arg = orig_Cmd_Argv(i);
        if (!arg || !arg[0]) continue;
        if (!std::strcmp(arg, "download") || !std::strcmp(arg, "install")) {
            do_download = true;
        } else if (!std::strcmp(arg, "force")) {
            force_download = true;
        } else if (!std::strcmp(arg, "help") || !std::strcmp(arg, "-h") || !std::strcmp(arg, "--help")) {
            print_update_usage();
            return;
        } else {
            PrintOut(PRINT_BAD, "Unknown sofbuddy_update argument: %s\n", arg);
            print_update_usage();
            return;
        }
    }

    sofbuddy_run_update_flow(do_download, force_download, false);
}

static bool open_url(const std::string& url, std::string& error) {
    std::wstring wide = utf8_to_wide(url);
    if (wide.empty()) {
        error = "URL conversion failed";
        return false;
    }

    HINSTANCE ret = ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(ret) <= 32) {
        error = "ShellExecute failed";
        return false;
    }
    return true;
}

void Cmd_SoFBuddy_OpenUrl_f(void) {
    if (!orig_Cmd_Argc || !orig_Cmd_Argv) return;
    if (orig_Cmd_Argc() < 2) {
        PrintOut(PRINT_DEV, "Usage: sofbuddy_openurl <url>\n");
        PrintOut(PRINT_DEV, "Note: only trusted https social links are allowed.\n");
        return;
    }

    const char* raw_url = orig_Cmd_Argv(1);
    if (!raw_url || !raw_url[0]) {
        set_openurl_status("invalid url");
        PrintOut(PRINT_BAD, "sofbuddy_openurl: invalid url\n");
        return;
    }

    std::string url(raw_url);
    std::string host;
    std::string error;
    if (!validate_social_url(url, host, error)) {
        set_openurl_status("blocked: " + error);
        set_update_cvar(kCvarOpenUrlLast, url);
        PrintOut(PRINT_BAD, "sofbuddy_openurl: %s\n", error.c_str());
        return;
    }

    if (!open_url(url, error)) {
        set_openurl_status("open failed");
        set_update_cvar(kCvarOpenUrlLast, url);
        PrintOut(PRINT_BAD, "sofbuddy_openurl: %s\n", error.c_str());
        return;
    }

    set_update_cvar(kCvarOpenUrlLast, url);
    set_openurl_status("opened: " + host);
    PrintOut(PRINT_DEV, "Opened %s in your default browser.\n", host.c_str());
}
