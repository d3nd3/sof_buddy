#include "feature_config.h"

#if FEATURE_HTTP_MAPS

#define MINIZ_IMPL
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#include "miniminiz.h"

#include "sof_compat.h"
#include "crc32.h"
#include "util.h"
#include "shared.h"
#if FEATURE_INTERNAL_MENUS
#include "features/internal_menus/shared.h"
#endif

#include <windows.h>
#include <winhttp.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <atomic>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>
#include <random>

cvar_t* _sofbuddy_http_maps = nullptr;
cvar_t* _sofbuddy_http_maps_dl_1 = nullptr;
cvar_t* _sofbuddy_http_maps_dl_2 = nullptr;
cvar_t* _sofbuddy_http_maps_dl_3 = nullptr;
cvar_t* _sofbuddy_http_maps_crc_1 = nullptr;
cvar_t* _sofbuddy_http_maps_crc_2 = nullptr;
cvar_t* _sofbuddy_http_maps_crc_3 = nullptr;

namespace
{
constexpr uintptr_t kHttpMaps_MapNamePtr = 0x201E9DCC;
constexpr const char* kHttpMapsDefaultRepo = "https://raw.githubusercontent.com/plowsof/sof1maps/main";
constexpr size_t kHttpMapsMaxMapNameLen = 256;
constexpr DWORD kHttpMapsRequestTimeoutMs = 5000;

struct FileData { uint32_t crc; std::string filename; };

enum class HttpMapsWorkerResult : int { None = 0, Success = 1, Failure = 2 };

struct HttpMapsRuntimeState
{
	bool waiting = false; // Main-thread only.
	std::string pending_map_bsp;
	detour_CL_Precache_f::tCL_Precache_f continue_callback = nullptr;
	const char* deferred_continue_reason = nullptr; // Main-thread only.
	std::atomic<uint32_t> next_job_id{1};
	std::atomic<uint32_t> active_job_id{0};
	std::atomic<bool> worker_running{false};
	std::atomic<float> worker_progress{0.0f};
	std::atomic<bool> progress_dirty{false};
	std::atomic<int> worker_result{static_cast<int>(HttpMapsWorkerResult::None)};
	bool loading_ui_active = false; // Main-thread only.
	std::string loading_ui_map;
	std::string cached_map_bsp;
	std::atomic<bool> status_dirty{false};
	std::atomic<bool> files_dirty{false};
	std::string pending_status;
	std::vector<std::string> pending_files;
	std::mutex ui_mutex;
};

HttpMapsRuntimeState g_http_maps_state;

static bool http_maps_file_exists(const std::string& path)
{
	if (path.empty()) return false;
	DWORD attr = GetFileAttributesA(path.c_str());
	return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

static std::string http_maps_to_lower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return value;
}

static bool http_maps_has_suffix_case_insensitive(const std::string& value, const char* suffix)
{
	if (!suffix) return false;
	size_t suffix_len = std::strlen(suffix);
	if (value.size() < suffix_len) return false;
	std::string tail = value.substr(value.size() - suffix_len);
	return http_maps_to_lower(tail) == http_maps_to_lower(std::string(suffix));
}

static int http_maps_mode(void)
{
	if (!_sofbuddy_http_maps) return 0;
	if (!_sofbuddy_http_maps->string || _sofbuddy_http_maps->string[0] == '0') return 0;
	int v = static_cast<int>(_sofbuddy_http_maps->value);
	if (v < 0) v = 0;
	if (v > 2) v = 2;
	return v;
}

static bool http_maps_is_enabled(void)
{
	return http_maps_mode() != 0;
}

static float http_maps_clamp_progress(float p)
{
	if (p < 0.0f) return 0.0f;
	if (p > 1.0f) return 1.0f;
	return p;
}

static void http_maps_publish_progress(uint32_t job_id, float p)
{
	if (g_http_maps_state.active_job_id.load(std::memory_order_acquire) != job_id) return;
	g_http_maps_state.worker_progress.store(http_maps_clamp_progress(p), std::memory_order_release);
	g_http_maps_state.progress_dirty.store(true, std::memory_order_release);
}

static std::string http_maps_trim_trailing_slash(std::string s)
{
	while (!s.empty() && (s.back() == '/' || s.back() == '\\')) s.pop_back();
	return s;
}

static std::string http_maps_get_cvar_url(cvar_t* c, const char* def)
{
	if (!c || !c->string || c->string[0] == '\0') return def ? def : "";
	return http_maps_trim_trailing_slash(std::string(c->string));
}

static std::string http_maps_select_dl_base(void)
{
	int mode = http_maps_mode();
	if (mode == 0) return "";
	std::string urls[3];
	urls[0] = http_maps_get_cvar_url(_sofbuddy_http_maps_dl_1, kHttpMapsDefaultRepo);
	urls[1] = http_maps_get_cvar_url(_sofbuddy_http_maps_dl_2, nullptr);
	urls[2] = http_maps_get_cvar_url(_sofbuddy_http_maps_dl_3, nullptr);
	if (mode == 1) return urls[0];
	std::vector<std::string> valid;
	for (int i = 0; i < 3; ++i) if (!urls[i].empty()) valid.push_back(urls[i]);
	if (valid.empty()) return urls[0];
	std::random_device rd;
	return valid[rd() % valid.size()];
}

static std::string http_maps_select_crc_base(void)
{
	int mode = http_maps_mode();
	if (mode == 0) return "";
	std::string urls[3];
	urls[0] = http_maps_get_cvar_url(_sofbuddy_http_maps_crc_1, kHttpMapsDefaultRepo);
	urls[1] = http_maps_get_cvar_url(_sofbuddy_http_maps_crc_2, nullptr);
	urls[2] = http_maps_get_cvar_url(_sofbuddy_http_maps_crc_3, nullptr);
	if (mode == 1) return urls[0];
	std::vector<std::string> valid;
	for (int i = 0; i < 3; ++i) if (!urls[i].empty()) valid.push_back(urls[i]);
	if (valid.empty()) return urls[0];
	std::random_device rd;
	return valid[rd() % valid.size()];
}

static void http_maps_ensure_parent_dirs(const std::string& file_path)
{
	if (file_path.empty()) return;
	std::string normalized = file_path;
	for (size_t i = 0; i < normalized.size(); ++i) if (normalized[i] == '\\') normalized[i] = '/';
	for (size_t i = 0; i < normalized.size(); ++i) {
		if (normalized[i] != '/') continue;
		std::string dir = normalized.substr(0, i);
		if (dir.empty()) continue;
		if (dir.size() == 2 && dir[1] == ':') continue;
		CreateDirectoryA(dir.c_str(), nullptr);
	}
}

static std::string http_maps_to_absolute_path(const std::string& path)
{
	char full[MAX_PATH];
	DWORD len = GetFullPathNameA(path.c_str(), MAX_PATH, full, nullptr);
	if (len == 0 || len >= MAX_PATH) return path;
	return std::string(full);
}

static std::wstring http_maps_to_wide(const std::string& value)
{
	if (value.empty()) return std::wstring();
	int needed = MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, nullptr, 0);
	if (needed <= 0) return std::wstring();
	std::wstring out(static_cast<size_t>(needed - 1), L'\0');
	MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, &out[0], needed);
	return out;
}

static bool http_maps_read_map_from_memory(std::string& out_map_bsp, bool log_errors, bool log_success)
{
	const char* map_name_ptr = reinterpret_cast<const char*>(kHttpMaps_MapNamePtr);
	if (IsBadStringPtrA(map_name_ptr, kHttpMapsMaxMapNameLen)) {
		if (log_errors)
			PrintOut(PRINT_BAD, "http_maps: Map name pointer invalid at 0x%p\n", (void*)kHttpMaps_MapNamePtr);
		return false;
	}
	out_map_bsp = std::string(map_name_ptr);
	if (log_success && !out_map_bsp.empty())
		PrintOut(PRINT_LOG, "http_maps: Map name: %s\n", out_map_bsp.c_str());
	return !out_map_bsp.empty();
}

static bool http_maps_normalize_map_path(std::string& map_bsp_path)
{
	if (map_bsp_path.empty()) return false;
	for (size_t i = 0; i < map_bsp_path.size(); ++i) if (map_bsp_path[i] == '\\') map_bsp_path[i] = '/';
	while (!map_bsp_path.empty() && (map_bsp_path[0] == '/' || map_bsp_path[0] == '\\')) map_bsp_path.erase(0, 1);
	if (map_bsp_path.find("..") != std::string::npos) {
		PrintOut(PRINT_BAD, "http_maps: Refusing unsafe map path: %s\n", map_bsp_path.c_str());
		return false;
	}
	const char* prefix = "maps/";
	if (map_bsp_path.compare(0, 5, prefix) != 0) map_bsp_path = std::string(prefix) + map_bsp_path;
	if (!http_maps_has_suffix_case_insensitive(map_bsp_path, ".bsp")) map_bsp_path += ".bsp";
	return true;
}

static bool http_maps_update_cached_map_from_memory(const char* source, bool update_loading_ui)
{
	std::string map_bsp_path;
	if (!http_maps_read_map_from_memory(map_bsp_path, false, false)) return false;
	if (!http_maps_normalize_map_path(map_bsp_path)) return false;

	if (g_http_maps_state.cached_map_bsp == map_bsp_path) return true;
	g_http_maps_state.cached_map_bsp = map_bsp_path;

	if (source)
		PrintOut(PRINT_LOG, "http_maps: Learned map at %s: %s\n", source, map_bsp_path.c_str());
	else
		PrintOut(PRINT_LOG, "http_maps: Learned map: %s\n", map_bsp_path.c_str());

#if FEATURE_INTERNAL_MENUS
	if (update_loading_ui) loading_set_current(map_bsp_path.c_str());
#else
	(void)update_loading_ui;
#endif
	return true;
}

static bool http_maps_resolve_map_for_precache(std::string& out_map_bsp)
{
	std::string map_bsp_path;
	if (http_maps_read_map_from_memory(map_bsp_path, true, false) &&
		http_maps_normalize_map_path(map_bsp_path)) {
		out_map_bsp = map_bsp_path;
		if (g_http_maps_state.cached_map_bsp != map_bsp_path)
			g_http_maps_state.cached_map_bsp = map_bsp_path;
		PrintOut(PRINT_LOG, "http_maps: Map name: %s\n", map_bsp_path.c_str());
		return true;
	}

	if (!g_http_maps_state.cached_map_bsp.empty()) {
		out_map_bsp = g_http_maps_state.cached_map_bsp;
		PrintOut(PRINT_LOG, "http_maps: Using cached map from CL_ParseConfigString: %s\n", out_map_bsp.c_str());
		return true;
	}

	return false;
}

static bool http_maps_map_exists_locally(const std::string& map_bsp_path)
{
	const std::string map_lower = http_maps_to_lower(map_bsp_path);
	const std::string user_map = std::string("user/") + map_bsp_path;
	const std::string user_map_lower = std::string("user/") + map_lower;
	if (http_maps_file_exists(map_bsp_path)) return true;
	if (http_maps_file_exists(map_lower)) return true;
	if (http_maps_file_exists(user_map)) return true;
	if (http_maps_file_exists(user_map_lower)) return true;
	return false;
}

static std::string http_maps_bsp_to_zip_relpath(const std::string& map_bsp_path)
{
	std::string zip_rel = map_bsp_path;
	if (zip_rel.compare(0, 5, "maps/") == 0) zip_rel.erase(0, 5);
	size_t dot = zip_rel.find_last_of('.');
	if (dot != std::string::npos) zip_rel.replace(dot, zip_rel.size() - dot, ".zip");
	else zip_rel += ".zip";
	return zip_rel;
}

static bool http_maps_find_local_map_file(const std::string& map_bsp_path, std::string& out_path)
{
	const std::string map_lower = http_maps_to_lower(map_bsp_path);
	const std::string user_map = std::string("user/") + map_bsp_path;
	const std::string user_map_lower = std::string("user/") + map_lower;
	if (http_maps_file_exists(map_bsp_path)) { out_path = map_bsp_path; return true; }
	if (http_maps_file_exists(map_lower)) { out_path = map_lower; return true; }
	if (http_maps_file_exists(user_map)) { out_path = user_map; return true; }
	if (http_maps_file_exists(user_map_lower)) { out_path = user_map_lower; return true; }
	out_path.clear();
	return false;
}

static bool http_maps_calculate_file_crc32(const std::string& path, uint32_t& out_crc)
{
	FILE* fp = std::fopen(path.c_str(), "rb");
	if (!fp) {
		PrintOut(PRINT_BAD, "http_maps: Failed to open file for CRC: %s\n", path.c_str());
		return false;
	}
	uint32_t crc = 0;
	unsigned char buffer[4096];
	size_t read_bytes = 0;
	while ((read_bytes = std::fread(buffer, 1, sizeof(buffer), fp)) > 0) crc32(buffer, read_bytes, &crc);
	const bool ok = std::ferror(fp) == 0;
	std::fclose(fp);
	if (!ok) {
		PrintOut(PRINT_BAD, "http_maps: Error reading file while computing CRC: %s\n", path.c_str());
		return false;
	}
	out_crc = crc;
	return true;
}

static uint16_t http_maps_read_le16(const unsigned char* data)
{
	return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

static uint32_t http_maps_read_le32(const unsigned char* data)
{
	return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
		(static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

static std::string http_maps_normalize_path_for_compare(std::string value)
{
	for (size_t i = 0; i < value.size(); ++i) if (value[i] == '\\') value[i] = '/';
	while (!value.empty() && (value[0] == '/' || value[0] == '\\')) value.erase(0, 1);
	while (value.size() >= 2 && value[0] == '.' && value[1] == '/') value.erase(0, 2);
	return http_maps_to_lower(value);
}

static bool http_maps_try_get_zip_map_crc(const std::string& zip_path, const std::string& expected_map_bsp, uint32_t& out_crc)
{
	std::ifstream zip(zip_path, std::ios::binary);
	if (!zip.is_open()) {
		PrintOut(PRINT_BAD, "http_maps: Failed to open zip for CRC lookup: %s\n", zip_path.c_str());
		return false;
	}
	zip.seekg(0, std::ios::end);
	std::streamoff file_size_stream = zip.tellg();
	if (file_size_stream < 22) {
		PrintOut(PRINT_BAD, "http_maps: Zip is too small/corrupt: %s\n", zip_path.c_str());
		return false;
	}
	const uint64_t file_size = static_cast<uint64_t>(file_size_stream);
	const uint64_t kMaxEocdWindow = 65535ULL + 22ULL;
	const uint64_t tail_size_u64 = file_size < kMaxEocdWindow ? file_size : kMaxEocdWindow;
	const uint64_t tail_offset_u64 = file_size - tail_size_u64;
	const size_t tail_size = static_cast<size_t>(tail_size_u64);
	std::vector<unsigned char> tail(tail_size);
	zip.seekg(static_cast<std::streamoff>(tail_offset_u64), std::ios::beg);
	zip.read(reinterpret_cast<char*>(tail.data()), static_cast<std::streamsize>(tail_size));
	if (!zip || static_cast<size_t>(zip.gcount()) != tail_size) {
		PrintOut(PRINT_BAD, "http_maps: Failed reading zip tail for EOCD: %s\n", zip_path.c_str());
		return false;
	}
	size_t eocd_pos = 0;
	bool found_eocd = false;
	for (size_t i = tail_size - 22;; --i) {
		if (tail[i] == 0x50 && tail[i + 1] == 0x4B && tail[i + 2] == 0x05 && tail[i + 3] == 0x06) {
			uint16_t comment_len = http_maps_read_le16(&tail[i + 20]);
			if (i + 22 + comment_len <= tail_size) {
				eocd_pos = i;
				found_eocd = true;
				break;
			}
		}
		if (i == 0) break;
	}
	if (!found_eocd) {
		PrintOut(PRINT_BAD, "http_maps: EOCD not found in zip: %s\n", zip_path.c_str());
		return false;
	}
	const unsigned char* eocd = &tail[eocd_pos];
	const uint32_t central_dir_size = http_maps_read_le32(&eocd[12]);
	const uint32_t central_dir_offset = http_maps_read_le32(&eocd[16]);
	const uint64_t central_dir_end = static_cast<uint64_t>(central_dir_offset) + static_cast<uint64_t>(central_dir_size);
	if (central_dir_end > file_size) {
		PrintOut(PRINT_BAD, "http_maps: Zip central directory exceeds file bounds: %s\n", zip_path.c_str());
		return false;
	}
	std::vector<unsigned char> central_dir(static_cast<size_t>(central_dir_size));
	zip.clear();
	zip.seekg(static_cast<std::streamoff>(central_dir_offset), std::ios::beg);
	if (central_dir.empty()) {
		PrintOut(PRINT_BAD, "http_maps: Zip central directory is empty: %s\n", zip_path.c_str());
		return false;
	}
	zip.read(reinterpret_cast<char*>(central_dir.data()), static_cast<std::streamsize>(central_dir.size()));
	if (!zip || static_cast<size_t>(zip.gcount()) != central_dir.size()) {
		PrintOut(PRINT_BAD, "http_maps: Failed reading central directory from zip: %s\n", zip_path.c_str());
		return false;
	}
	const std::string wanted = http_maps_normalize_path_for_compare(expected_map_bsp);
	std::string wanted_without_maps;
	if (wanted.compare(0, 5, "maps/") == 0) wanted_without_maps = wanted.substr(5);
	PrintOut(PRINT_LOG, "http_maps: zip CRC lookup expected wanted=%s wanted_without_maps=%s\n", wanted.c_str(), wanted_without_maps.c_str());
	bool saw_bsp = false;
	uint32_t first_bsp_crc = 0;
	size_t bsp_count = 0;
	size_t pos = 0;
	while (pos + 46 <= central_dir.size()) {
		if (http_maps_read_le32(&central_dir[pos]) != 0x02014B50) { ++pos; continue; }
		const uint32_t entry_crc = http_maps_read_le32(&central_dir[pos + 16]);
		const uint16_t name_len = http_maps_read_le16(&central_dir[pos + 28]);
		const uint16_t extra_len = http_maps_read_le16(&central_dir[pos + 30]);
		const uint16_t comment_len = http_maps_read_le16(&central_dir[pos + 32]);
		const size_t entry_size = 46u + static_cast<size_t>(name_len) + static_cast<size_t>(extra_len) + static_cast<size_t>(comment_len);
		if (pos + entry_size > central_dir.size()) {
			PrintOut(PRINT_BAD, "http_maps: Corrupt central directory entry in zip: %s\n", zip_path.c_str());
			return false;
		}
		std::string entry_name(reinterpret_cast<const char*>(&central_dir[pos + 46]), name_len);
		std::string entry_normalized = http_maps_normalize_path_for_compare(entry_name);
		if (!entry_normalized.empty() && entry_normalized.back() != '/' && http_maps_has_suffix_case_insensitive(entry_normalized, ".bsp")) {
			PrintOut(PRINT_LOG, "http_maps: zip BSP entry: \"%s\"\n", entry_name.c_str());
			if (!saw_bsp) { first_bsp_crc = entry_crc; saw_bsp = true; }
			++bsp_count;
			if (entry_normalized == wanted || (!wanted_without_maps.empty() && entry_normalized == wanted_without_maps)) {
				out_crc = entry_crc;
				return true;
			}
		}
		pos += entry_size;
	}
	if (saw_bsp && bsp_count == 1) {
		PrintOut(PRINT_LOG, "http_maps: Exact map entry not found in zip, using only BSP entry CRC\n");
		out_crc = first_bsp_crc;
		return true;
	}
	PrintOut(PRINT_BAD, "http_maps: Could not find expected map BSP CRC in zip: %s\n", zip_path.c_str());
	return false;
}

static bool http_maps_validate_extracted_map_crc(const std::string& zip_path, const std::string& expected_map_bsp)
{
	uint32_t expected_crc = 0;
	if (!http_maps_try_get_zip_map_crc(zip_path, expected_map_bsp, expected_crc)) return false;
	std::string extracted_map_path;
	if (!http_maps_find_local_map_file(expected_map_bsp, extracted_map_path)) {
		PrintOut(PRINT_BAD, "http_maps: Expected extracted map is missing: %s (checked: %s, user/%s, and lower)\n",
			expected_map_bsp.c_str(), expected_map_bsp.c_str(), expected_map_bsp.c_str());
		return false;
	}
	uint32_t extracted_crc = 0;
	if (!http_maps_calculate_file_crc32(extracted_map_path, extracted_crc)) return false;
	if (extracted_crc != expected_crc) {
		PrintOut(PRINT_BAD, "http_maps: CRC mismatch for %s (zip=%08X disk=%08X)\n",
			extracted_map_path.c_str(), expected_crc, extracted_crc);
		return false;
	}
	return true;
}

static int getCentralDirectoryOffset(const unsigned char* blob, int blob_size, int& found_offset, int& abs_dir_offset)
{
	for (int i = blob_size - 4; i >= 0; --i) {
		if (memcmp(blob + i, "\x50\x4b\x05\x06", 4) == 0) {
			abs_dir_offset = *reinterpret_cast<const int*>(blob + i + 16);
			int central_directory_size = *reinterpret_cast<const int*>(blob + i + 12);
			found_offset = i - central_directory_size;
			return central_directory_size;
		}
	}
	return 0;
}

static void extractCentralDirectory(const unsigned char* central_dir, int cd_size, std::vector<FileData>* files)
{
	files->clear();
	int k = 0;
	while (k < cd_size) {
		if (memcmp(central_dir + k, "\x50\x4b\x01\x02", 4) == 0) {
			uint32_t crc = http_maps_read_le32(central_dir + k + 16);
			uint16_t name_len = http_maps_read_le16(central_dir + k + 28);
			uint16_t extra_len = http_maps_read_le16(central_dir + k + 30);
			uint16_t comment_len = http_maps_read_le16(central_dir + k + 32);
			std::string name(reinterpret_cast<const char*>(central_dir + k + 46), name_len);
			files->push_back({ crc, name });
			k += 46 + name_len + extra_len + comment_len;
		} else ++k;
	}
}

static int partial_header_content_length(const wchar_t* urlw)
{
	int file_size = 0;
	URL_COMPONENTS uc = {};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256] = {}, path[2048] = {};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;

	if (!WinHttpCrackUrl(urlw, 0, 0, &uc)) return 0;

	HINTERNET hSession = WinHttpOpen(L"SoFBuddy", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) return 0;
	DWORD to_ms = kHttpMapsRequestTimeoutMs;
	WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &to_ms, sizeof(to_ms));
	WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &to_ms, sizeof(to_ms));
	WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &to_ms, sizeof(to_ms));

	HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
	if (!hConnect) {
		WinHttpCloseHandle(hSession);
		return 0;
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"HEAD", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest) {
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	if (uc.nScheme == INTERNET_SCHEME_HTTPS) {
		DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
		WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
	}

	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	if (!WinHttpReceiveResponse(hRequest, nullptr)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	DWORD status = 0;
	DWORD statusLen = sizeof(status);
	if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX) && status == 200) {
		wchar_t buf[64];
		DWORD bufLen = sizeof(buf);
		if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, buf, &bufLen, WINHTTP_NO_HEADER_INDEX))
			file_size = _wtoi(buf);
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);
	return file_size;
}

static bool partial_http_range(const wchar_t* urlw, int range_start, int range_end, std::vector<unsigned char>* out)
{
	URL_COMPONENTS uc = {};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256] = {}, path[2048] = {};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;

	if (!WinHttpCrackUrl(urlw, 0, 0, &uc)) return false;

	HINTERNET hSession = WinHttpOpen(L"SoFBuddy", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) return false;
	DWORD to_ms = kHttpMapsRequestTimeoutMs;
	WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &to_ms, sizeof(to_ms));
	WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &to_ms, sizeof(to_ms));
	WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &to_ms, sizeof(to_ms));

	HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
	if (!hConnect) {
		WinHttpCloseHandle(hSession);
		return false;
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest) {
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (uc.nScheme == INTERNET_SCHEME_HTTPS) {
		DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
		WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
	}

	wchar_t rangeHdr[64];
	swprintf(rangeHdr, 64, L"Range: bytes=%d-%d", range_start, range_end);
	if (!WinHttpAddRequestHeaders(hRequest, rangeHdr, -1, WINHTTP_ADDREQ_FLAG_ADD)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (!WinHttpReceiveResponse(hRequest, nullptr)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	DWORD status = 0;
	DWORD statusLen = sizeof(status);
	if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX) || (status != 206 && status != 200)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	out->clear();
	DWORD avail = 0;
	unsigned char buf[4096];
	while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
		DWORD read = 0;
		if (!WinHttpReadData(hRequest, buf, (avail > sizeof(buf)) ? sizeof(buf) : avail, &read) || read == 0) break;
		out->insert(out->end(), buf, buf + read);
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);
	return true;
}

static std::wstring http_maps_utf8_to_wide(const std::string& utf8)
{
	if (utf8.empty()) return std::wstring();
	int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
	if (needed <= 0) return std::wstring();
	std::wstring out(static_cast<size_t>(needed - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], needed);
	return out;
}

static bool partial_http_blobs(const char* url, std::vector<FileData>* zip_content)
{
	std::wstring urlw = http_maps_utf8_to_wide(url);
	if (urlw.empty()) return false;

	int file_size = partial_header_content_length(urlw.c_str());
	if (file_size <= 0) {
		PrintOut(PRINT_BAD, "http_maps: Map not found for url: %s\n", url);
		return false;
	}
	const int CHUNK = 100;
	int upper = file_size;
	bool forced_range = false;
	int forced_lower = 0, forced_upper = 0, cd_size = 0;
	std::vector<unsigned char> blob;

	while (true) {
		blob.clear();
		int range_start, range_end;
		if (forced_range) {
			range_start = forced_lower;
			range_end = forced_upper;
		} else {
			range_start = upper - CHUNK;
			range_end = upper;
			upper = range_start - 1;
		}

		std::vector<unsigned char> chunk;
		if (!partial_http_range(urlw.c_str(), range_start, range_end, &chunk)) {
			PrintOut(PRINT_BAD, "http_maps: Partial download failed\n");
			return false;
		}

		blob = std::move(chunk);

		if (forced_range) {
			extractCentralDirectory(blob.data(), static_cast<int>(blob.size()), zip_content);
			return true;
		}

		int found_offset, abs_dir_offset;
		cd_size = getCentralDirectoryOffset(blob.data(), static_cast<int>(blob.size()), found_offset, abs_dir_offset);
		if (cd_size > 0) {
			if (found_offset > 0) {
				extractCentralDirectory(blob.data() + found_offset, cd_size, zip_content);
				return true;
			}
			forced_range = true;
			forced_lower = abs_dir_offset;
			forced_upper = abs_dir_offset + cd_size;
			continue;
		}

		if (upper < CHUNK) break;
	}

	PrintOut(PRINT_BAD, "http_maps: Cannot find Central Directory in HTTP blob\n");
	return false;
}

static void http_maps_set_progress(uint32_t job_id, float p)
{
	http_maps_publish_progress(job_id, p);
}

#if FEATURE_INTERNAL_MENUS
static std::string http_maps_loading_file_label(std::string path)
{
	for (size_t i = 0; i < path.size(); ++i)
		if (path[i] == '\\') path[i] = '/';
	if (path.compare(0, 5, "maps/") == 0) path.erase(0, 5);

	const size_t kMaxLabelLen = 46;
	if (path.size() > kMaxLabelLen) {
		const size_t keep = kMaxLabelLen - 3;
		path = std::string("...") + path.substr(path.size() - keep);
	}
	return path;
}
#endif

static void http_maps_clear_pending_ui_updates(void)
{
#if FEATURE_INTERNAL_MENUS
	{
		std::lock_guard<std::mutex> lock(g_http_maps_state.ui_mutex);
		g_http_maps_state.pending_status.clear();
		g_http_maps_state.pending_files.clear();
	}
	g_http_maps_state.status_dirty.store(false, std::memory_order_release);
	g_http_maps_state.files_dirty.store(false, std::memory_order_release);
#endif
}

static void http_maps_queue_status(uint32_t job_id, const char* status)
{
#if FEATURE_INTERNAL_MENUS
	if (!status || !status[0]) return;
	if (g_http_maps_state.active_job_id.load(std::memory_order_acquire) != job_id) return;

	{
		std::lock_guard<std::mutex> lock(g_http_maps_state.ui_mutex);
		g_http_maps_state.pending_status = status;
	}
	g_http_maps_state.status_dirty.store(true, std::memory_order_release);
#else
	(void)job_id;
	(void)status;
#endif
}

static void http_maps_queue_files(uint32_t job_id, const std::vector<FileData>& zip_content)
{
#if FEATURE_INTERNAL_MENUS
	if (g_http_maps_state.active_job_id.load(std::memory_order_acquire) != job_id) return;

	std::vector<std::string> labels;
	labels.reserve(8);
	const size_t max_rows = 8;
	size_t file_limit = zip_content.size();
	if (file_limit >= max_rows) file_limit = max_rows - 1;

	for (size_t i = 0; i < file_limit; ++i) {
		if (zip_content[i].filename.empty()) continue;
		labels.push_back(http_maps_loading_file_label(zip_content[i].filename));
	}

	if (zip_content.size() > file_limit) {
		char extra[48];
		std::snprintf(extra, sizeof(extra), "... +%d more", static_cast<int>(zip_content.size() - file_limit));
		labels.push_back(extra);
	}

	{
		std::lock_guard<std::mutex> lock(g_http_maps_state.ui_mutex);
		g_http_maps_state.pending_files = labels;
	}
	g_http_maps_state.files_dirty.store(true, std::memory_order_release);
#else
	(void)job_id;
	(void)zip_content;
#endif
}

static void http_maps_flush_pending_ui_updates(void)
{
#if FEATURE_INTERNAL_MENUS
	if (g_http_maps_state.status_dirty.exchange(false, std::memory_order_acq_rel)) {
		std::string status;
		{
			std::lock_guard<std::mutex> lock(g_http_maps_state.ui_mutex);
			status = g_http_maps_state.pending_status;
		}
		if (!status.empty()) loading_push_status(status.c_str());
	}

	if (g_http_maps_state.files_dirty.exchange(false, std::memory_order_acq_rel)) {
		std::vector<std::string> files;
		{
			std::lock_guard<std::mutex> lock(g_http_maps_state.ui_mutex);
			files = g_http_maps_state.pending_files;
		}

		std::vector<const char*> file_ptrs;
		file_ptrs.reserve(files.size());
		for (size_t i = 0; i < files.size(); ++i)
			file_ptrs.push_back(files[i].c_str());
		loading_set_files(file_ptrs.empty() ? nullptr : file_ptrs.data(), static_cast<int>(file_ptrs.size()));
	}
#endif
}

static void http_maps_console_progress(float p, const char* map_bsp)
{
	static int s_last_pct = -1;
	const int pct = static_cast<int>(http_maps_clamp_progress(p) * 100);
	if (pct == s_last_pct) return;
	s_last_pct = (pct >= 100) ? -1 : pct;
	const int bar_len = 20;
	int filled = (pct * bar_len) / 100;
	if (filled > bar_len) filled = bar_len;
	char bar[32];
	bar[0] = '[';
	for (int i = 1; i <= bar_len; ++i) bar[i] = (i <= filled) ? '=' : ' ';
	bar[bar_len + 1] = ']';
	bar[bar_len + 2] = '\0';
	PrintOut(PRINT_LOG, "http_maps: %s %s %d%%\n", map_bsp && map_bsp[0] ? map_bsp : "download", bar, pct);
}

static void http_maps_apply_progress_cvar(float p)
{
#if FEATURE_INTERNAL_MENUS
	if (!orig_Cvar_Set2) return;
	char val[32];
	snprintf(val, sizeof(val), "%.2f", http_maps_clamp_progress(p));
	orig_Cvar_Set2(const_cast<char*>("_sofbuddy_loading_progress"), val, true);
#endif
}

static void http_maps_clear_loading_cvars(void)
{
	http_maps_clear_pending_ui_updates();
	if (!orig_Cvar_Set2) return;
#if FEATURE_INTERNAL_MENUS
	orig_Cvar_Set2(const_cast<char*>("_sofbuddy_loading_progress"), const_cast<char*>("0"), true);
	for (int i = 1; i <= 4; ++i) {
		char name[48];
		std::snprintf(name, sizeof(name), "_sofbuddy_loading_status_%d", i);
		orig_Cvar_Set2(name, const_cast<char*>(""), true);
	}
	loading_set_files(nullptr, 0);
#endif
}

static bool http_maps_download_zip_winhttp(const std::string& remote_url, const std::string& local_zip_path, uint32_t job_id)
{
	http_maps_ensure_parent_dirs(local_zip_path);
	DeleteFileA(local_zip_path.c_str());
	PrintOut(PRINT_LOG, "http_maps: Downloading %s\n", remote_url.c_str());
	http_maps_set_progress(job_id, 0);

	std::wstring urlw = http_maps_utf8_to_wide(remote_url);
	if (urlw.empty()) return false;

	URL_COMPONENTS uc = {};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256] = {}, path[2048] = {};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;

	if (!WinHttpCrackUrl(urlw.c_str(), 0, 0, &uc)) return false;

	HINTERNET hSession = WinHttpOpen(L"SoFBuddy", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) return false;
	DWORD req_timeout_ms = kHttpMapsRequestTimeoutMs;
	WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &req_timeout_ms, sizeof(req_timeout_ms));
	WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &req_timeout_ms, sizeof(req_timeout_ms));
	WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &req_timeout_ms, sizeof(req_timeout_ms));

	HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
	if (!hConnect) {
		WinHttpCloseHandle(hSession);
		return false;
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest) {
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (uc.nScheme == INTERNET_SCHEME_HTTPS) {
		DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
		WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
	}

	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (!WinHttpReceiveResponse(hRequest, nullptr)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	DWORD status = 0;
	DWORD statusLen = sizeof(status);
	if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX) || status != 200) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	DWORD content_len = 0;
	wchar_t cl_buf[64];
	DWORD cl_len = sizeof(cl_buf);
	if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, cl_buf, &cl_len, WINHTTP_NO_HEADER_INDEX))
		content_len = _wtoi(cl_buf);

	FILE* fp = fopen(local_zip_path.c_str(), "wb");
	if (!fp) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	DWORD avail = 0;
	unsigned char buf[8192];
	DWORD total_read = 0;
	while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
		DWORD read = 0;
		if (!WinHttpReadData(hRequest, buf, (avail > sizeof(buf)) ? sizeof(buf) : avail, &read) || read == 0) break;
		fwrite(buf, 1, read, fp);
		total_read += read;
		if (content_len > 0) http_maps_set_progress(job_id, static_cast<float>(total_read) / content_len);
	}

	fclose(fp);
	http_maps_set_progress(job_id, 1);
	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);

	if (!http_maps_file_exists(local_zip_path)) {
		PrintOut(PRINT_BAD, "http_maps: Download reported success but zip missing: %s\n", local_zip_path.c_str());
		return false;
	}
	return true;
}

static bool http_maps_extract_zip_miniz(const std::string& zip_path, const std::string& dest_dir)
{
	mz_zip_archive za;
	memset(&za, 0, sizeof(za));
	if (!mz_zip_reader_init_file(&za, zip_path.c_str(), 0)) {
		PrintOut(PRINT_BAD, "http_maps: miniz failed to open zip: %s\n", zip_path.c_str());
		return false;
	}
	bool ok = true;
	const mz_uint num_files = mz_zip_reader_get_num_files(&za);
	for (mz_uint i = 0; i < num_files && ok; ++i) {
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(&za, i, &stat)) continue;
		if (stat.m_is_directory) continue;
		std::string out_path = dest_dir + "/" + stat.m_filename;
		PrintOut(PRINT_LOG, "http_maps: extract zip entry \"%s\" -> %s\n", stat.m_filename, out_path.c_str());
		http_maps_ensure_parent_dirs(out_path);
		if (!mz_zip_reader_extract_to_file(&za, i, out_path.c_str(), 0)) {
			PrintOut(PRINT_BAD, "http_maps: miniz extract failed: %s\n", stat.m_filename);
			ok = false;
		}
	}
	mz_zip_reader_end(&za);
	return ok;
}

static bool http_maps_extract_and_validate_zip(const std::string& zip_path, const std::string& map_bsp_path)
{
	if (!http_maps_extract_zip_miniz(zip_path, "user")) {
		PrintOut(PRINT_BAD, "http_maps: Extraction failed for %s\n", zip_path.c_str());
		return false;
	}
	if (!http_maps_validate_extracted_map_crc(zip_path, map_bsp_path)) {
		PrintOut(PRINT_BAD, "http_maps: CRC validation failed for %s\n", zip_path.c_str());
		return false;
	}
	return true;
}

static std::string http_maps_temp_zip_path(const std::string& map_bsp_path)
{
	std::string p = std::string("user/") + map_bsp_path;
	size_t dot = p.rfind('.');
	if (dot != std::string::npos) p.replace(dot, p.size() - dot, ".zip.tmp");
	else p += ".zip.tmp";
	return p;
}

static void http_maps_download_worker(std::string map_bsp_path, std::string zip_rel_path, uint32_t job_id)
{
	bool success = false;
	http_maps_set_progress(job_id, 0.0f);
	http_maps_queue_status(job_id, "Fetching zip CRC list...");
	do {
		if (!http_maps_is_enabled()) {
			PrintOut(PRINT_LOG, "http_maps: Worker aborted (feature disabled)\n");
			http_maps_queue_status(job_id, "HTTP assist disabled.");
			break;
		}
		const std::string dl_base = http_maps_select_dl_base();
		const std::string crc_base = http_maps_select_crc_base();

		if (dl_base.empty() || crc_base.empty()) {
			PrintOut(PRINT_BAD, "http_maps: Mode 0 or no valid URLs\n");
			http_maps_queue_status(job_id, "HTTP URLs are not configured.");
			break;
		}

		const std::string crc_url = crc_base + "/" + zip_rel_path;
		const std::string temp_zip_path = http_maps_temp_zip_path(map_bsp_path);

		std::vector<FileData> zip_content;
		if (!partial_http_blobs(crc_url.c_str(), &zip_content)) {
			http_maps_queue_status(job_id, "Failed to fetch zip CRC list.");
			break;
		}
		http_maps_queue_files(job_id, zip_content);
		http_maps_queue_status(job_id, "Comparing local CRCs...");

		bool do_download = false;
		const std::string userdir = "user";
		for (const auto& fd : zip_content) {
			std::string full_path = userdir + "/" + fd.filename;
			if (!http_maps_file_exists(full_path)) {
				do_download = true;
				break;
			}
			uint32_t disk_crc = 0;
			if (!http_maps_calculate_file_crc32(full_path, disk_crc)) {
				do_download = true;
				break;
			}
			if (disk_crc != fd.crc) {
				do_download = true;
				break;
			}
		}

		if (!do_download) {
			PrintOut(PRINT_LOG, "http_maps: All CRCs match on disk, skipping download\n");
			http_maps_set_progress(job_id, 1.0f);
			http_maps_queue_status(job_id, "CRC match. Download skipped.");
			success = true;
			break;
		}

		DeleteFileA(temp_zip_path.c_str());
		const std::string dl_url = dl_base + "/" + zip_rel_path;
		http_maps_queue_status(job_id, "Downloading map zip...");
		if (http_maps_download_zip_winhttp(dl_url, temp_zip_path, job_id)) {
			http_maps_queue_status(job_id, "Extracting zip to user/...");
			if (http_maps_extract_and_validate_zip(temp_zip_path, map_bsp_path)) {
				DeleteFileA(temp_zip_path.c_str());
				PrintOut(PRINT_GOOD, "http_maps: Downloaded and extracted %s\n", map_bsp_path.c_str());
				http_maps_queue_status(job_id, "HTTP map ready.");
				success = true;
			} else {
				DeleteFileA(temp_zip_path.c_str());
				PrintOut(PRINT_BAD, "http_maps: Extraction failed for %s\n", map_bsp_path.c_str());
				http_maps_queue_status(job_id, "Zip extraction or CRC validation failed.");
			}
		} else {
			DeleteFileA(temp_zip_path.c_str());
			PrintOut(PRINT_BAD, "http_maps: Download failed for %s\n", map_bsp_path.c_str());
			http_maps_queue_status(job_id, "HTTP download failed.");
		}
	} while (false);

	if (g_http_maps_state.active_job_id.load(std::memory_order_acquire) == job_id) {
		g_http_maps_state.worker_result.store(
			success ? static_cast<int>(HttpMapsWorkerResult::Success) : static_cast<int>(HttpMapsWorkerResult::Failure),
			std::memory_order_release
		);
		g_http_maps_state.worker_running.store(false, std::memory_order_release);
	}
}

} // namespace

void create_http_maps_cvars(void)
{
	_sofbuddy_http_maps = orig_Cvar_Get("_sofbuddy_http_maps", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_dl_1 = orig_Cvar_Get("_sofbuddy_http_maps_dl_1", kHttpMapsDefaultRepo, CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_dl_2 = orig_Cvar_Get("_sofbuddy_http_maps_dl_2", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_dl_3 = orig_Cvar_Get("_sofbuddy_http_maps_dl_3", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_crc_1 = orig_Cvar_Get("_sofbuddy_http_maps_crc_1", kHttpMapsDefaultRepo, CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_crc_2 = orig_Cvar_Get("_sofbuddy_http_maps_crc_2", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_crc_3 = orig_Cvar_Get("_sofbuddy_http_maps_crc_3", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
	g_http_maps_state.cached_map_bsp.clear();
	http_maps_clear_loading_cvars();
}

void http_maps_on_parse_configstring_post(void)
{
	http_maps_update_cached_map_from_memory("CL_ParseConfigString", true);
}

void http_maps_try_begin_precache(detour_CL_Precache_f::tCL_Precache_f original)
{
	SOFBUDDY_ASSERT(original != nullptr);

	PrintOut(PRINT_LOG, "http_maps: CL_Precache_f intercepted\n");
	if (!http_maps_is_enabled()) {
		PrintOut(PRINT_LOG, "http_maps: Disabled (mode=%d), passing through\n", http_maps_mode());
		http_maps_clear_loading_cvars();
		original();
		return;
	}

	std::string map_bsp_path;
	if (!http_maps_resolve_map_for_precache(map_bsp_path)) {
		PrintOut(PRINT_LOG, "http_maps: Failed to resolve map name, passing through\n");
		http_maps_clear_loading_cvars();
		original();
		return;
	}

	if (http_maps_map_exists_locally(map_bsp_path)) {
		PrintOut(PRINT_LOG, "http_maps: Map exists locally: %s, passing through\n", map_bsp_path.c_str());
		http_maps_clear_loading_cvars();
		original();
		return;
	}

	if (g_http_maps_state.waiting && g_http_maps_state.pending_map_bsp == map_bsp_path) {
		PrintOut(PRINT_LOG, "http_maps: Already waiting on %s\n", map_bsp_path.c_str());
		return;
	}
	if (g_http_maps_state.waiting) {
		PrintOut(PRINT_LOG, "http_maps: Preempting previous map wait (%s -> %s)\n",
			g_http_maps_state.pending_map_bsp.c_str(), map_bsp_path.c_str());
	}

	const std::string zip_rel_path = http_maps_bsp_to_zip_relpath(map_bsp_path);
	const uint32_t job_id = g_http_maps_state.next_job_id.fetch_add(1, std::memory_order_acq_rel);
	g_http_maps_state.active_job_id.store(job_id, std::memory_order_release);
	g_http_maps_state.worker_running.store(true, std::memory_order_release);

	g_http_maps_state.waiting = true;
	g_http_maps_state.pending_map_bsp = map_bsp_path;
	g_http_maps_state.continue_callback = original;
	g_http_maps_state.deferred_continue_reason = nullptr;
	g_http_maps_state.worker_result.store(static_cast<int>(HttpMapsWorkerResult::None), std::memory_order_release);
	g_http_maps_state.worker_progress.store(0.0f, std::memory_order_release);
	g_http_maps_state.progress_dirty.store(true, std::memory_order_release);
	g_http_maps_state.loading_ui_active = true;
	g_http_maps_state.loading_ui_map = map_bsp_path;
	http_maps_clear_pending_ui_updates();

	PrintOut(PRINT_LOG, "http_maps: Preparing map %s (zip: %s)\n", map_bsp_path.c_str(), zip_rel_path.c_str());

#if FEATURE_INTERNAL_MENUS
	http_maps_apply_progress_cvar(0.0f);
	loading_set_current(map_bsp_path.c_str());
	loading_set_files(nullptr, 0);
	loading_push_status("HTTP map assist active.");
	loading_push_status("Checking local files...");
#endif

	try {
		std::thread(http_maps_download_worker, map_bsp_path, zip_rel_path, job_id).detach();
	} catch (...) {
		PrintOut(PRINT_BAD, "http_maps: Failed to start worker thread for %s\n", map_bsp_path.c_str());
		if (g_http_maps_state.active_job_id.load(std::memory_order_acquire) == job_id)
			g_http_maps_state.active_job_id.store(0, std::memory_order_release);
		g_http_maps_state.waiting = false;
		g_http_maps_state.pending_map_bsp.clear();
		g_http_maps_state.continue_callback = nullptr;
		g_http_maps_state.deferred_continue_reason = nullptr;
		g_http_maps_state.worker_running.store(false, std::memory_order_release);
		g_http_maps_state.loading_ui_active = false;
		g_http_maps_state.loading_ui_map.clear();
		http_maps_clear_loading_cvars();
		original();
	}
}

void http_maps_pump(void)
{
	if (g_http_maps_state.progress_dirty.exchange(false, std::memory_order_acq_rel)) {
		const float p = g_http_maps_state.worker_progress.load(std::memory_order_acquire);
		http_maps_console_progress(p, g_http_maps_state.pending_map_bsp.c_str());
		http_maps_apply_progress_cvar(p);
	}
	http_maps_flush_pending_ui_updates();

	if (!g_http_maps_state.waiting) return;
	if (g_http_maps_state.worker_running.load(std::memory_order_acquire)) return;

	const HttpMapsWorkerResult result = static_cast<HttpMapsWorkerResult>(
		g_http_maps_state.worker_result.exchange(static_cast<int>(HttpMapsWorkerResult::None), std::memory_order_acq_rel)
	);
	if (result == HttpMapsWorkerResult::Success) {
		g_http_maps_state.deferred_continue_reason = "http download success";
#if FEATURE_INTERNAL_MENUS
		loading_push_status("HTTP map ready. Continuing precache.");
#endif
	} else if (result == HttpMapsWorkerResult::Failure) {
		g_http_maps_state.deferred_continue_reason = "http download failed";
#if FEATURE_INTERNAL_MENUS
		loading_push_status("HTTP assist failed. Falling back to engine.");
#endif
	} else {
		g_http_maps_state.deferred_continue_reason = "http worker ended without result";
#if FEATURE_INTERNAL_MENUS
		loading_push_status("HTTP worker ended unexpectedly.");
#endif
	}
}

static void http_maps_continue_precache(const char* reason)
{
	detour_CL_Precache_f::tCL_Precache_f callback = g_http_maps_state.continue_callback;
	std::string map_name = g_http_maps_state.pending_map_bsp;
#if FEATURE_INTERNAL_MENUS
	if (reason && std::strstr(reason, "success")) {
		loading_push_history(map_name.c_str());
	}
#endif
	g_http_maps_state.waiting = false;
	g_http_maps_state.pending_map_bsp.clear();
	g_http_maps_state.continue_callback = nullptr;
	g_http_maps_state.deferred_continue_reason = nullptr;
	g_http_maps_state.active_job_id.store(0, std::memory_order_release);
	g_http_maps_state.worker_running.store(false, std::memory_order_release);
	g_http_maps_state.worker_result.store(static_cast<int>(HttpMapsWorkerResult::None), std::memory_order_release);
	http_maps_clear_loading_cvars();
	g_http_maps_state.loading_ui_active = false;
	g_http_maps_state.loading_ui_map.clear();
	g_http_maps_state.progress_dirty.store(false, std::memory_order_release);

	if (!callback) return;

	if (reason) PrintOut(PRINT_LOG, "http_maps: Continuing CL_Precache_f (%s) for %s\n", reason, map_name.c_str());
	else PrintOut(PRINT_LOG, "http_maps: Continuing CL_Precache_f for %s\n", map_name.c_str());
	callback();
}

void http_maps_run_deferred_continue_if_pending(void)
{
	if (!g_http_maps_state.deferred_continue_reason) return;
	const char* reason = g_http_maps_state.deferred_continue_reason;
	g_http_maps_state.deferred_continue_reason = nullptr;
	http_maps_continue_precache(reason);
}

#endif // FEATURE_HTTP_MAPS
