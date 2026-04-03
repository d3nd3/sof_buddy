#include "feature_config.h"

#if FEATURE_HTTP_MAPS

#include "generated_detours.h"

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

// Older SDKs may omit TLS 1.2 flags used by WinHttpSetOption(SECURE_PROTOCOLS).
#ifndef WINHTTP_OPTION_SECURE_PROTOCOLS
#define WINHTTP_OPTION_SECURE_PROTOCOLS 84
#endif
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 0x00000800
#endif
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 0x00000200
#endif
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 0x00000080
#endif
// Schannel "soft" failures (offline CRL, EKU mismatch) often surface as 12157 unless ignored.
#ifndef SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE
#define SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE 0x00000200
#endif
#ifndef SECURITY_FLAG_IGNORE_REVOCATION
#define SECURITY_FLAG_IGNORE_REVOCATION 0x00000080
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <atomic>
#include <fstream>
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
constexpr uintptr_t kHttpMaps_NetMessageRva = 0x367EBC;
constexpr int kHttpMaps_ConfigStringMapModelIndex = 36;
constexpr const char* kHttpMapsDefaultRepo = "https://sofvault.org/sof1maps";
constexpr const char* kHttpMapsSecondaryRepo = "https://raw.githubusercontent.com/plowsof/sof1maps/main";
constexpr size_t kHttpMapsMaxMapNameLen = 256;
constexpr DWORD kHttpMapsRequestTimeoutMs = 5000;
constexpr DWORD kHttpMapsContinueWatchdogMs = 45000;

struct FileData { uint32_t crc; std::string filename; };

enum class HttpMapsWorkerResult : int { None = 0, Success = 1, Failure = 2 };

// Forward declarations
static bool http_maps_map_exists_locally(const std::string& map_bsp_path);
static std::string http_maps_bsp_to_zip_relpath(const std::string& map_bsp_path);
static void http_maps_download_worker(std::string map_bsp_path, std::string zip_rel_path, uint32_t job_id);

struct HttpMapsRuntimeState
{
	bool waiting = false; // Main-thread only.
	DWORD waiting_started_ms = 0; // Main-thread only.
	std::string pending_map_bsp;
	detour_CL_Precache_f::tCL_Precache_f continue_callback = nullptr;
	const char* deferred_continue_reason = nullptr; // Main-thread only.
	std::atomic<uint32_t> next_job_id{1};
	std::atomic<uint32_t> active_job_id{0};
	std::atomic<bool> worker_running{false};
	std::atomic<float> worker_progress{0.0f};
	std::atomic<bool> progress_dirty{false};
	std::atomic<int> worker_result{static_cast<int>(HttpMapsWorkerResult::None)};
	std::atomic<bool> worker_success_no_crc_list{false}; // Success but zip not in repo; pump sets status from local file.
	std::atomic<bool> worker_did_download{false};
	bool loading_ui_active = false; // Main-thread only.
	std::string loading_ui_map;
	std::string cached_map_bsp;
	std::string completed_map_bsp; // Map that has finished its proactive check/download.
};

HttpMapsRuntimeState g_http_maps_state;

static bool http_maps_job_is_active(uint32_t job_id)
{
	return g_http_maps_state.active_job_id.load(std::memory_order_acquire) == job_id &&
		g_http_maps_state.worker_running.load(std::memory_order_acquire);
}

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
	if (v > 3) v = 3;
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

struct HttpMapsProviderSelection
{
	std::string dl_base;
	std::string crc_base;
	int provider_index = -1;
	int mode = 0;
};

static const char* http_maps_mode_label(int mode)
{
	switch (mode) {
	case 1: return "primary";
	case 2: return "random";
	case 3: return "rotate";
	default: return "off";
	}
}

static bool http_maps_select_provider(HttpMapsProviderSelection& out)
{
	out = HttpMapsProviderSelection();
	const int mode = http_maps_mode();
	out.mode = mode;
	if (mode == 0) return false;

	std::string dl_urls[3];
	std::string crc_urls[3];
	dl_urls[0] = http_maps_get_cvar_url(_sofbuddy_http_maps_dl_1, kHttpMapsDefaultRepo);
	dl_urls[1] = http_maps_get_cvar_url(_sofbuddy_http_maps_dl_2, nullptr);
	dl_urls[2] = http_maps_get_cvar_url(_sofbuddy_http_maps_dl_3, nullptr);
	crc_urls[0] = http_maps_get_cvar_url(_sofbuddy_http_maps_crc_1, kHttpMapsDefaultRepo);
	crc_urls[1] = http_maps_get_cvar_url(_sofbuddy_http_maps_crc_2, nullptr);
	crc_urls[2] = http_maps_get_cvar_url(_sofbuddy_http_maps_crc_3, nullptr);

	std::vector<int> valid_indices;
	for (int i = 0; i < 3; ++i) {
		if (!dl_urls[i].empty() && !crc_urls[i].empty())
			valid_indices.push_back(i);
	}
	if (valid_indices.empty()) return false;

	int chosen = valid_indices.front();
	if (mode == 1) {
		// Primary-only mode: prefer provider #1, fallback to first valid.
		if (!dl_urls[0].empty() && !crc_urls[0].empty())
			chosen = 0;
	} else if (mode == 2) {
		// Random mode across configured providers.
		std::random_device rd;
		chosen = valid_indices[rd() % valid_indices.size()];
	} else if (mode == 3) {
		// Rotate mode across configured providers.
		static size_t s_rr_cursor = 0;
		chosen = valid_indices[s_rr_cursor % valid_indices.size()];
		++s_rr_cursor;
	}

	out.provider_index = chosen;
	out.dl_base = dl_urls[chosen];
	out.crc_base = crc_urls[chosen];
	return true;
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

static bool http_maps_resolve_map_for_precache(std::string& out_map_bsp)
{
	std::string map_bsp_path;
	if (http_maps_read_map_from_memory(map_bsp_path, true, false) &&
		http_maps_normalize_map_path(map_bsp_path)) {
		out_map_bsp = map_bsp_path;
		if (g_http_maps_state.cached_map_bsp != map_bsp_path)
			g_http_maps_state.cached_map_bsp = map_bsp_path;
		return true;
	}
	if (!g_http_maps_state.cached_map_bsp.empty()) {
		out_map_bsp = g_http_maps_state.cached_map_bsp;
		return true;
	}

	return false;
}

static bool http_maps_map_exists_locally(const std::string& map_bsp_path)
{
	const std::string map_lower = http_maps_to_lower(map_bsp_path);
	const std::string user_map = std::string("user/") + map_bsp_path;
	const std::string user_map_lower = std::string("user/") + map_lower;
	return http_maps_file_exists(map_bsp_path) || http_maps_file_exists(map_lower) ||
		http_maps_file_exists(user_map) || http_maps_file_exists(user_map_lower);
}

static bool http_maps_map_exists_via_engine(const std::string& map_bsp_path)
{
	if (!detour_FS_LoadFile::oFS_LoadFile) return http_maps_map_exists_locally(map_bsp_path);
	char path[256];
	snprintf(path, sizeof(path), "%s", map_bsp_path.c_str());
	return detour_FS_LoadFile::oFS_LoadFile(path, nullptr, false) != -1;
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
		PrintOut(PRINT_BAD, "http_maps: CRC open failed %s\n", path.c_str());
		return false;
	}
	uint32_t crc = 0;
	unsigned char buffer[4096];
	size_t read_bytes = 0;
	while ((read_bytes = std::fread(buffer, 1, sizeof(buffer), fp)) > 0) crc32(buffer, read_bytes, &crc);
	const bool ok = std::ferror(fp) == 0;
	std::fclose(fp);
	if (!ok) {
		PrintOut(PRINT_BAD, "http_maps: CRC read failed %s\n", path.c_str());
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
		PrintOut(PRINT_BAD, "http_maps: zip open failed %s\n", zip_path.c_str());
		return false;
	}
	zip.seekg(0, std::ios::end);
	std::streamoff file_size_stream = zip.tellg();
	if (file_size_stream < 22) {
		PrintOut(PRINT_BAD, "http_maps: zip too small %s\n", zip_path.c_str());
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
		PrintOut(PRINT_BAD, "http_maps: zip tail read failed %s\n", zip_path.c_str());
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
		PrintOut(PRINT_BAD, "http_maps: zip EOCD missing %s\n", zip_path.c_str());
		return false;
	}
	const unsigned char* eocd = &tail[eocd_pos];
	const uint32_t central_dir_size = http_maps_read_le32(&eocd[12]);
	const uint32_t central_dir_offset = http_maps_read_le32(&eocd[16]);
	const uint64_t central_dir_end = static_cast<uint64_t>(central_dir_offset) + static_cast<uint64_t>(central_dir_size);
	if (central_dir_end > file_size) {
		PrintOut(PRINT_BAD, "http_maps: zip CD out of bounds %s\n", zip_path.c_str());
		return false;
	}
	std::vector<unsigned char> central_dir(static_cast<size_t>(central_dir_size));
	zip.clear();
	zip.seekg(static_cast<std::streamoff>(central_dir_offset), std::ios::beg);
	if (central_dir.empty()) {
		PrintOut(PRINT_BAD, "http_maps: zip CD empty %s\n", zip_path.c_str());
		return false;
	}
	zip.read(reinterpret_cast<char*>(central_dir.data()), static_cast<std::streamsize>(central_dir.size()));
	if (!zip || static_cast<size_t>(zip.gcount()) != central_dir.size()) {
		PrintOut(PRINT_BAD, "http_maps: zip CD read failed %s\n", zip_path.c_str());
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
			PrintOut(PRINT_BAD, "http_maps: zip CD corrupt %s\n", zip_path.c_str());
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
	PrintOut(PRINT_BAD, "http_maps: map BSP not in zip %s\n", zip_path.c_str());
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

static void http_maps_winhttp_apply_session_options(HINTERNET hSession)
{
	DWORD to_ms = kHttpMapsRequestTimeoutMs;
	WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &to_ms, sizeof(to_ms));
	WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &to_ms, sizeof(to_ms));
	WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &to_ms, sizeof(to_ms));
	DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));
	// Modern HTTPS (e.g. GitHub Pages) requires TLS 1.2+; default WinHTTP on older Windows may negotiate SSL3/TLS1.0 only.
	DWORD secure_protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1;
	WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secure_protocols, sizeof(secure_protocols));
}

// Per-request TLS + cert flags. 12157 (SECURE_CHANNEL_ERROR) on WinHttpSendRequest: repeat TLS bitmask on
// the request (some builds only honor it here) and relax Schannel checks (CRL/EKU) that still fail with IGNORE_UNKNOWN_CA alone.
static void http_maps_winhttp_apply_https_request_options(HINTERNET hRequest)
{
	DWORD secure_protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1;
	(void)WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURE_PROTOCOLS, &secure_protocols, sizeof(secure_protocols));
	DWORD sec_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
		SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE | SECURITY_FLAG_IGNORE_REVOCATION;
	(void)WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &sec_flags, sizeof(sec_flags));
}

static void http_maps_discard_response_body(HINTERNET hRequest)
{
	DWORD avail = 0;
	unsigned char buf[16384];
	while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
		DWORD read = 0;
		if (!WinHttpReadData(hRequest, buf, (avail > sizeof(buf)) ? sizeof(buf) : avail, &read) || read == 0) break;
	}
}

static void http_maps_dev_err(const char* stage, DWORD err, const char* url)
{
	PrintOut(PRINT_DEV, "http_maps: %s err=%lu %s\n", stage, static_cast<unsigned long>(err), url && url[0] ? url : "");
}

static void http_maps_dev_err_rng(int range_lo, int range_hi, const char* what, DWORD err, const char* url)
{
	char stage[112];
	std::snprintf(stage, sizeof(stage), "range[%d-%d] %s", range_lo, range_hi, what);
	http_maps_dev_err(stage, err, url);
}

// Parses "bytes 0-0/1317119" or "bytes 0-99/1317119" (value from Content-Range header).
static int http_maps_parse_content_range_total_w(const wchar_t* cr)
{
	if (!cr || !*cr) return 0;
	const wchar_t* slash = nullptr;
	for (const wchar_t* p = cr; *p; ++p)
		if (*p == L'/') slash = p;
	if (!slash || !slash[1]) return 0;
	if (slash[1] == L'*') return 0;
	const long long n = std::wcstoll(slash + 1, nullptr, 10);
	if (n <= 0 || n > static_cast<long long>(INT_MAX)) return 0;
	return static_cast<int>(n);
}

// GET with Range: bytes=0-0 — works when HEAD fails (proxies, WinHTTP quirks) or omits Content-Length.
static int http_maps_probe_file_size_via_get_range(const wchar_t* urlw, const char* log_url)
{
	URL_COMPONENTS uc = {};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256] = {}, path[2048] = {};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;

	if (!WinHttpCrackUrl(urlw, 0, 0, &uc)) {
		http_maps_dev_err("size[Range] CrackUrl", GetLastError(), log_url);
		return 0;
	}

	HINTERNET hSession = WinHttpOpen(L"SoFBuddy/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) {
		http_maps_dev_err("size[Range] WinHttpOpen", GetLastError(), log_url);
		return 0;
	}
	http_maps_winhttp_apply_session_options(hSession);

	HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
	if (!hConnect) {
		http_maps_dev_err("size[Range] Connect", GetLastError(), log_url);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest) {
		http_maps_dev_err("size[Range] OpenRequest", GetLastError(), log_url);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	if (uc.nScheme == INTERNET_SCHEME_HTTPS)
		http_maps_winhttp_apply_https_request_options(hRequest);

	if (!WinHttpAddRequestHeaders(hRequest, L"Range: bytes=0-0", -1, WINHTTP_ADDREQ_FLAG_ADD)) {
		http_maps_dev_err("size[Range] Add Range hdr", GetLastError(), log_url);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		http_maps_dev_err("size[Range] SendRequest", GetLastError(), log_url);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	if (!WinHttpReceiveResponse(hRequest, nullptr)) {
		http_maps_dev_err("size[Range] ReceiveResponse", GetLastError(), log_url);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	DWORD status = 0;
	DWORD statusLen = sizeof(status);
	if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX)) {
		http_maps_dev_err("size[Range] QueryStatus", GetLastError(), log_url);
		http_maps_discard_response_body(hRequest);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	int file_size = 0;
	if (status == 206) {
		wchar_t crBuf[512];
		DWORD crLen = sizeof(crBuf);
		if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, L"Content-Range", crBuf, &crLen, WINHTTP_NO_HEADER_INDEX))
			file_size = http_maps_parse_content_range_total_w(crBuf);
		if (file_size <= 0)
			PrintOut(PRINT_DEV, "http_maps: size[Range] HTTP 206, no Content-Range %s\n", log_url && log_url[0] ? log_url : "");
	} else if (status == 200) {
		wchar_t lenBuf[64];
		DWORD lenLen = sizeof(lenBuf);
		if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, lenBuf, &lenLen, WINHTTP_NO_HEADER_INDEX))
			file_size = _wtoi(lenBuf);
	} else {
		PrintOut(PRINT_DEV, "http_maps: size[Range] HTTP %lu (need 206 or 200) %s\n", static_cast<unsigned long>(status), log_url && log_url[0] ? log_url : "");
	}

	http_maps_discard_response_body(hRequest);
	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);
	return file_size;
}

// GET without Range: read Content-Length from a 200 response, then discard the body (HEAD/Range sometimes fail on proxies or old stacks).
static int http_maps_probe_file_size_via_full_get(const wchar_t* urlw, const char* log_url)
{
	URL_COMPONENTS uc = {};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256] = {}, path[2048] = {};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;

	if (!WinHttpCrackUrl(urlw, 0, 0, &uc)) {
		http_maps_dev_err("size[GET] CrackUrl", GetLastError(), log_url);
		return 0;
	}

	HINTERNET hSession = WinHttpOpen(L"SoFBuddy/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) {
		http_maps_dev_err("size[GET] WinHttpOpen", GetLastError(), log_url);
		return 0;
	}
	http_maps_winhttp_apply_session_options(hSession);

	HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
	if (!hConnect) {
		http_maps_dev_err("size[GET] Connect", GetLastError(), log_url);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest) {
		http_maps_dev_err("size[GET] OpenRequest", GetLastError(), log_url);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	if (uc.nScheme == INTERNET_SCHEME_HTTPS)
		http_maps_winhttp_apply_https_request_options(hRequest);

	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		http_maps_dev_err("size[GET] SendRequest", GetLastError(), log_url);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	if (!WinHttpReceiveResponse(hRequest, nullptr)) {
		http_maps_dev_err("size[GET] ReceiveResponse", GetLastError(), log_url);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	DWORD status = 0;
	DWORD statusLen = sizeof(status);
	if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX)) {
		http_maps_dev_err("size[GET] QueryStatus", GetLastError(), log_url);
		http_maps_discard_response_body(hRequest);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	int file_size = 0;
	if (status == 200) {
		wchar_t lenBuf[64];
		DWORD lenLen = sizeof(lenBuf);
		if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, lenBuf, &lenLen, WINHTTP_NO_HEADER_INDEX))
			file_size = _wtoi(lenBuf);
		if (file_size <= 0)
			PrintOut(PRINT_DEV, "http_maps: size[GET] HTTP 200, no Content-Length %s\n", log_url && log_url[0] ? log_url : "");
	} else {
		PrintOut(PRINT_DEV, "http_maps: size[GET] HTTP %lu (need 200) %s\n", static_cast<unsigned long>(status), log_url && log_url[0] ? log_url : "");
	}

	http_maps_discard_response_body(hRequest);
	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);
	return file_size;
}

static int http_maps_head_content_length(const wchar_t* urlw, const char* log_url)
{
	int file_size = 0;
	URL_COMPONENTS uc = {};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256] = {}, path[2048] = {};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;

	if (!WinHttpCrackUrl(urlw, 0, 0, &uc)) {
		http_maps_dev_err("HEAD CrackUrl", GetLastError(), log_url);
		return 0;
	}

	HINTERNET hSession = WinHttpOpen(L"SoFBuddy/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) {
		http_maps_dev_err("HEAD WinHttpOpen", GetLastError(), log_url);
		return 0;
	}
	http_maps_winhttp_apply_session_options(hSession);

	HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
	if (!hConnect) {
		http_maps_dev_err("HEAD Connect", GetLastError(), log_url);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"HEAD", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest) {
		http_maps_dev_err("HEAD OpenRequest", GetLastError(), log_url);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	if (uc.nScheme == INTERNET_SCHEME_HTTPS)
		http_maps_winhttp_apply_https_request_options(hRequest);

	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		http_maps_dev_err("HEAD SendRequest", GetLastError(), log_url);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	if (!WinHttpReceiveResponse(hRequest, nullptr)) {
		http_maps_dev_err("HEAD ReceiveResponse", GetLastError(), log_url);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return 0;
	}

	DWORD status = 0;
	DWORD statusLen = sizeof(status);
	const bool got_status = WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX);
	if (got_status && status == 200) {
		wchar_t buf[64];
		DWORD bufLen = sizeof(buf);
		if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, buf, &bufLen, WINHTTP_NO_HEADER_INDEX))
			file_size = _wtoi(buf);
		if (file_size <= 0)
			PrintOut(PRINT_DEV, "http_maps: HEAD HTTP 200, no Content-Length %s\n", log_url && log_url[0] ? log_url : "");
	} else if (got_status) {
		PrintOut(PRINT_DEV, "http_maps: HEAD HTTP %lu %s\n", static_cast<unsigned long>(status), log_url && log_url[0] ? log_url : "");
	} else {
		http_maps_dev_err("HEAD QueryStatus", GetLastError(), log_url);
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);
	return file_size;
}

static int http_maps_get_remote_file_size(const wchar_t* urlw, const char* log_url)
{
	int n = http_maps_head_content_length(urlw, log_url);
	if (n > 0) {
		PrintOut(PRINT_DEV, "http_maps: zip size %d (HEAD) %s\n", n, log_url && log_url[0] ? log_url : "");
		return n;
	}
	n = http_maps_probe_file_size_via_get_range(urlw, log_url);
	if (n > 0) {
		PrintOut(PRINT_DEV, "http_maps: zip size %d (Range 0-0) %s\n", n, log_url && log_url[0] ? log_url : "");
		return n;
	}
	n = http_maps_probe_file_size_via_full_get(urlw, log_url);
	if (n > 0)
		PrintOut(PRINT_DEV, "http_maps: zip size %d (GET+discard) %s\n", n, log_url && log_url[0] ? log_url : "");
	return n;
}

static bool partial_http_range(const wchar_t* urlw, int range_start, int range_end, std::vector<unsigned char>* out, const char* log_url)
{
	URL_COMPONENTS uc = {};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256] = {}, path[2048] = {};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;

	if (!WinHttpCrackUrl(urlw, 0, 0, &uc)) {
		http_maps_dev_err_rng(range_start, range_end, "CrackUrl", GetLastError(), log_url);
		return false;
	}

	HINTERNET hSession = WinHttpOpen(L"SoFBuddy/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) {
		http_maps_dev_err_rng(range_start, range_end, "WinHttpOpen", GetLastError(), log_url);
		return false;
	}
	http_maps_winhttp_apply_session_options(hSession);

	HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
	if (!hConnect) {
		http_maps_dev_err_rng(range_start, range_end, "Connect", GetLastError(), log_url);
		WinHttpCloseHandle(hSession);
		return false;
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest) {
		http_maps_dev_err_rng(range_start, range_end, "OpenRequest", GetLastError(), log_url);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (uc.nScheme == INTERNET_SCHEME_HTTPS)
		http_maps_winhttp_apply_https_request_options(hRequest);

	wchar_t rangeHdr[64];
	swprintf(rangeHdr, 64, L"Range: bytes=%d-%d", range_start, range_end);
	if (!WinHttpAddRequestHeaders(hRequest, rangeHdr, -1, WINHTTP_ADDREQ_FLAG_ADD)) {
		http_maps_dev_err_rng(range_start, range_end, "Add Range hdr", GetLastError(), log_url);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		http_maps_dev_err_rng(range_start, range_end, "SendRequest", GetLastError(), log_url);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (!WinHttpReceiveResponse(hRequest, nullptr)) {
		http_maps_dev_err_rng(range_start, range_end, "ReceiveResponse", GetLastError(), log_url);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	DWORD status = 0;
	DWORD statusLen = sizeof(status);
	if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX)) {
		http_maps_dev_err_rng(range_start, range_end, "QueryStatus", GetLastError(), log_url);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}
	if (status != 206 && status != 200) {
		PrintOut(PRINT_DEV, "http_maps: range[%d-%d] HTTP %lu (need 206/200) %s\n", range_start, range_end, static_cast<unsigned long>(status), log_url && log_url[0] ? log_url : "");
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	out->clear();
	DWORD avail = 0;
	unsigned char buf[4096];
	size_t total_chunk = 0;
	while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
		DWORD read = 0;
		if (!WinHttpReadData(hRequest, buf, (avail > sizeof(buf)) ? sizeof(buf) : avail, &read) || read == 0) break;
		out->insert(out->end(), buf, buf + read);
		total_chunk += read;
	}
	const int span = range_end - range_start + 1;
	if (span > 400)
		PrintOut(PRINT_DEV, "http_maps: range[%d-%d] ok %zu B HTTP %lu %s\n", range_start, range_end, total_chunk, static_cast<unsigned long>(status), log_url && log_url[0] ? log_url : "");

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

static bool partial_http_blobs(const char* url, std::vector<FileData>* zip_content, uint32_t job_id)
{
	if (!http_maps_job_is_active(job_id)) return false;
	std::wstring urlw = http_maps_utf8_to_wide(url);
	if (urlw.empty()) return false;

	PrintOut(PRINT_DEV, "http_maps: zip index fetch %s\n", url);
	int file_size = http_maps_get_remote_file_size(urlw.c_str(), url);
	if (file_size <= 0) {
		PrintOut(PRINT_DEV, "http_maps: remote zip size unavailable (HTTPS). %s\n", url);
		return false;
	}
	PrintOut(PRINT_DEV, "http_maps: zip index %d bytes, tail scan step 100 %s\n", file_size, url);
	const int CHUNK = 100;
	int upper = file_size;
	bool forced_range = false;
	int forced_lower = 0, forced_upper = 0, cd_size = 0;
	std::vector<unsigned char> blob;

	while (true) {
		if (!http_maps_job_is_active(job_id)) return false;
		blob.clear();
		int range_start, range_end;
		if (forced_range) {
			range_start = forced_lower;
			range_end = forced_upper;
			PrintOut(PRINT_DEV, "http_maps: zip index range %d-%d (cd)\n", range_start, range_end);
		} else {
			range_start = upper - CHUNK;
			range_end = upper;
			PrintOut(PRINT_DEV, "http_maps: zip index tail %d-%d\n", range_start, range_end);
			upper = range_start - 1;
		}

		std::vector<unsigned char> chunk;
		if (!partial_http_range(urlw.c_str(), range_start, range_end, &chunk, url)) {
			PrintOut(PRINT_BAD, "http_maps: zip index range request failed %s\n", url);
			return false;
		}

		blob = std::move(chunk);

		if (forced_range) {
			extractCentralDirectory(blob.data(), static_cast<int>(blob.size()), zip_content);
			PrintOut(PRINT_DEV, "http_maps: zip index %zu entries %s\n", zip_content->size(), url);
			return true;
		}

		int found_offset, abs_dir_offset;
		cd_size = getCentralDirectoryOffset(blob.data(), static_cast<int>(blob.size()), found_offset, abs_dir_offset);
		if (cd_size > 0) {
			if (found_offset > 0) {
				PrintOut(PRINT_DEV, "http_maps: zip index CD at +%d (%d B)\n", found_offset, cd_size);
				extractCentralDirectory(blob.data() + found_offset, cd_size, zip_content);
				PrintOut(PRINT_DEV, "http_maps: zip index %zu entries %s\n", zip_content->size(), url);
				return true;
			}
			PrintOut(PRINT_DEV, "http_maps: zip index CD split, fetch %d-%d (%d B)\n", abs_dir_offset, abs_dir_offset + cd_size, cd_size);
			forced_range = true;
			forced_lower = abs_dir_offset;
			forced_upper = abs_dir_offset + cd_size;
			continue;
		}

		if (upper < CHUNK) break;
	}

	PrintOut(PRINT_BAD, "http_maps: zip index: central directory not found %s\n", url);
	return false;
}

static void http_maps_set_progress(uint32_t job_id, float p)
{
	http_maps_publish_progress(job_id, p);
}

static void http_maps_clear_pending_ui_updates(void)
{
	// Slim loading UI: no queued status/history/file preview buffers.
}

static void http_maps_queue_status(uint32_t job_id, const char* status)
{
	(void)job_id;
	(void)status;
}

static void http_maps_queue_files(uint32_t job_id, const std::vector<FileData>& zip_content)
{
	(void)job_id;
	(void)zip_content;
}

static void http_maps_flush_pending_ui_updates(void)
{
	// Slim loading UI: no queued status/history/file preview buffers.
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
	PrintOut(PRINT_DEV, "http_maps: dl %s %s %d%%\n", map_bsp && map_bsp[0] ? map_bsp : "?", bar, pct);
}

static void http_maps_apply_progress_cvar(float p)
{
#if FEATURE_INTERNAL_MENUS
	if (!detour_Cvar_Set2::oCvar_Set2) return;
	const float clamped = http_maps_clamp_progress(p);
	char val[32];
	snprintf(val, sizeof(val), "%.2f", clamped);
	PrintOut(PRINT_DEV, "http_maps: UI _sofbuddy_loading_progress %s\n", val);
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_loading_progress"), val, true);
#endif
}

#if FEATURE_INTERNAL_MENUS
static void http_maps_set_loading_status(const char* status)
{
	PrintOut(PRINT_DEV, "http_maps: UI _sofbuddy_loading_status %s\n", status);
	if (detour_Cvar_Set2::oCvar_Set2) detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_loading_status"), const_cast<char*>(status), true);
}

static void http_maps_loading_ui_show_map(const char* map_bsp_path)
{
	PrintOut(PRINT_DEV, "http_maps: UI loading_show_ui; _sofbuddy_loading_current %s\n", map_bsp_path && map_bsp_path[0] ? map_bsp_path : "?");
	loading_show_ui();
	loading_set_current(map_bsp_path);
}
#endif

static void http_maps_clear_loading_cvars(bool reset_status = true)
{
	http_maps_clear_pending_ui_updates();
	if (!detour_Cvar_Set2::oCvar_Set2) return;
#if FEATURE_INTERNAL_MENUS
	PrintOut(PRINT_DEV, "http_maps: UI _sofbuddy_loading_progress (clear)\n");
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_loading_progress"), const_cast<char*>(""), true);
	PrintOut(PRINT_DEV, "http_maps: UI _sofbuddy_loading_current (unknown)\n");
	loading_reset_current_map_unknown();
	if (reset_status) http_maps_set_loading_status("CHECKING");
#endif
}

static bool http_maps_download_zip_winhttp(const std::string& remote_url, const std::string& local_zip_path, uint32_t job_id)
{
	if (!http_maps_job_is_active(job_id)) return false;
	http_maps_ensure_parent_dirs(local_zip_path);
	DeleteFileA(local_zip_path.c_str());
	PrintOut(PRINT_DEV, "http_maps: download %s\n", remote_url.c_str());
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

	if (!WinHttpCrackUrl(urlw.c_str(), 0, 0, &uc)) {
		http_maps_dev_err("dl CrackUrl", GetLastError(), remote_url.c_str());
		return false;
	}

	HINTERNET hSession = WinHttpOpen(L"SoFBuddy/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) {
		http_maps_dev_err("dl WinHttpOpen", GetLastError(), remote_url.c_str());
		return false;
	}
	http_maps_winhttp_apply_session_options(hSession);

	HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
	if (!hConnect) {
		http_maps_dev_err("dl Connect", GetLastError(), remote_url.c_str());
		WinHttpCloseHandle(hSession);
		return false;
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest) {
		http_maps_dev_err("dl OpenRequest", GetLastError(), remote_url.c_str());
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (uc.nScheme == INTERNET_SCHEME_HTTPS)
		http_maps_winhttp_apply_https_request_options(hRequest);

	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		http_maps_dev_err("dl SendRequest", GetLastError(), remote_url.c_str());
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (!WinHttpReceiveResponse(hRequest, nullptr)) {
		http_maps_dev_err("dl ReceiveResponse", GetLastError(), remote_url.c_str());
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	DWORD status = 0;
	DWORD statusLen = sizeof(status);
	if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX)) {
		http_maps_dev_err("dl QueryStatus", GetLastError(), remote_url.c_str());
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}
	if (status != 200) {
		PrintOut(PRINT_DEV, "http_maps: dl HTTP %lu %s\n", static_cast<unsigned long>(status), remote_url.c_str());
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
	PrintOut(PRINT_DEV, "http_maps: dl HTTP 200, %lu B -> %s\n", static_cast<unsigned long>(content_len), local_zip_path.c_str());

	FILE* fp = fopen(local_zip_path.c_str(), "wb");
	if (!fp) {
		PrintOut(PRINT_DEV, "http_maps: dl fopen %s\n", local_zip_path.c_str());
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	DWORD avail = 0;
	unsigned char buf[8192];
	DWORD total_read = 0;
	while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
		if (!http_maps_job_is_active(job_id)) {
			PrintOut(PRINT_DEV, "http_maps: dl cancelled at %lu B %s\n", static_cast<unsigned long>(total_read), remote_url.c_str());
			fclose(fp);
			DeleteFileA(local_zip_path.c_str());
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}
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

	PrintOut(PRINT_DEV, "http_maps: dl done %lu B %s\n", static_cast<unsigned long>(total_read), local_zip_path.c_str());

	if (!http_maps_file_exists(local_zip_path)) {
		PrintOut(PRINT_BAD, "http_maps: download ok but file missing: %s\n", local_zip_path.c_str());
		return false;
	}
	return true;
}

static bool http_maps_extract_zip_miniz(const std::string& zip_path, const std::string& dest_dir, uint32_t job_id)
{
	mz_zip_archive za;
	memset(&za, 0, sizeof(za));
	if (!mz_zip_reader_init_file(&za, zip_path.c_str(), 0)) {
		PrintOut(PRINT_BAD, "http_maps: zip unreadable %s\n", zip_path.c_str());
		return false;
	}
	bool ok = true;
	const mz_uint num_files = mz_zip_reader_get_num_files(&za);
	for (mz_uint i = 0; i < num_files && ok; ++i) {
		if (!http_maps_job_is_active(job_id)) {
			ok = false;
			break;
		}
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(&za, i, &stat)) continue;
		if (stat.m_is_directory) continue;
		std::string out_path = dest_dir + "/" + stat.m_filename;
		PrintOut(PRINT_LOG, "http_maps: extract zip entry \"%s\" -> %s\n", stat.m_filename, out_path.c_str());
		http_maps_ensure_parent_dirs(out_path);
		if (!mz_zip_reader_extract_to_file(&za, i, out_path.c_str(), 0)) {
			PrintOut(PRINT_BAD, "http_maps: extract entry failed %s\n", stat.m_filename);
			ok = false;
		}
	}
	mz_zip_reader_end(&za);
	return ok;
}

static bool http_maps_extract_and_validate_zip(const std::string& zip_path, const std::string& map_bsp_path, uint32_t job_id)
{
	if (!http_maps_extract_zip_miniz(zip_path, "user", job_id)) {
		PrintOut(PRINT_BAD, "http_maps: extract failed %s\n", zip_path.c_str());
		return false;
	}
	if (!http_maps_job_is_active(job_id)) return false;
	if (!http_maps_validate_extracted_map_crc(zip_path, map_bsp_path)) {
		PrintOut(PRINT_BAD, "http_maps: CRC check failed %s\n", zip_path.c_str());
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
		if (!http_maps_job_is_active(job_id)) break;
		if (!http_maps_is_enabled()) {
			PrintOut(PRINT_LOG, "http_maps: Worker aborted (feature disabled)\n");
			http_maps_queue_status(job_id, "HTTP assist disabled.");
			break;
		}
		HttpMapsProviderSelection provider;
		if (!http_maps_select_provider(provider)) {
			PrintOut(PRINT_BAD, "http_maps: no HTTP provider URL configured\n");
			http_maps_queue_status(job_id, "HTTP URLs are not configured.");
			break;
		}
		char provider_status[96];
		std::snprintf(provider_status, sizeof(provider_status), "Provider #%d selected (%s).",
			provider.provider_index + 1, http_maps_mode_label(provider.mode));
		http_maps_queue_status(job_id, provider_status);

		const std::string crc_url = provider.crc_base + "/" + zip_rel_path;
		const std::string temp_zip_path = http_maps_temp_zip_path(map_bsp_path);
		PrintOut(PRINT_DEV, "http_maps: worker %s zip=%s (%s)\n", map_bsp_path.c_str(), zip_rel_path.c_str(), http_maps_mode_label(provider.mode));

		std::vector<FileData> zip_content;
		if (!partial_http_blobs(crc_url.c_str(), &zip_content, job_id)) {
			http_maps_queue_status(job_id, "Failed to fetch zip CRC list.");
			PrintOut(PRINT_LOG, "http_maps: Could not get remote CRC list, assuming PAK/Base\n");
			// Worker thread cannot safely use engine FS probe. However, a positive local-file hit is
			// trustworthy and lets us show NOT NEEDED much earlier. For misses, stay on CHECKING and
			// let main-thread pump resolve NOT NEEDED vs UDP Downloading via engine FS.
#if FEATURE_INTERNAL_MENUS
			if (detour_Cvar_Set2::oCvar_Set2) {
				const bool map_present_local = http_maps_map_exists_locally(map_bsp_path);
				http_maps_set_loading_status(map_present_local ? "NOT NEEDED" : "CHECKING");
			}
#endif
			if (g_http_maps_state.active_job_id.load(std::memory_order_acquire) == job_id)
				g_http_maps_state.worker_success_no_crc_list.store(true, std::memory_order_release);
			success = true;
			break;
		}
		http_maps_queue_files(job_id, zip_content);
		http_maps_queue_status(job_id, "Comparing local CRCs...");

		bool do_download = false;
		const std::string userdir = "user";
		for (const auto& fd : zip_content) {
			if (!http_maps_job_is_active(job_id)) break;
			std::string full_path = userdir + "/" + fd.filename;
			if (!http_maps_file_exists(full_path)) {
				PrintOut(PRINT_DEV, "http_maps: CRC compare %s remote=%08X local=(missing) path=%s\n",
					fd.filename.c_str(), fd.crc, full_path.c_str());
				PrintOut(PRINT_LOG, "http_maps: Local file missing: %s\n", full_path.c_str());
				do_download = true;
				break;
			}
			uint32_t disk_crc = 0;
			if (!http_maps_calculate_file_crc32(full_path, disk_crc)) {
				PrintOut(PRINT_DEV, "http_maps: CRC compare %s remote=%08X local=(read failed) path=%s\n",
					fd.filename.c_str(), fd.crc, full_path.c_str());
				PrintOut(PRINT_LOG, "http_maps: Failed to calculate local CRC for %s\n", full_path.c_str());
				do_download = true;
				break;
			}
			PrintOut(PRINT_DEV, "http_maps: CRC compare %s remote=%08X local=%08X %s\n",
				fd.filename.c_str(), fd.crc, disk_crc, disk_crc == fd.crc ? "match" : "mismatch");
			if (disk_crc != fd.crc) {
				PrintOut(PRINT_LOG, "http_maps: CRC mismatch for %s (local: %08X, remote: %08X)\n", full_path.c_str(), disk_crc, fd.crc);
				do_download = true;
				break;
			}
		}

		if (!http_maps_job_is_active(job_id)) break;

		if (!do_download) {
			PrintOut(PRINT_LOG, "http_maps: All CRCs match on disk, skipping download (status unchanged)\n");
			http_maps_set_progress(job_id, 1.0f);
			http_maps_queue_status(job_id, "CRC match. Download skipped.");
			success = true;
			break;
		}

			DeleteFileA(temp_zip_path.c_str());
			const std::string dl_url = provider.dl_base + "/" + zip_rel_path;
		PrintOut(PRINT_DEV, "http_maps: worker GET %s\n", dl_url.c_str());
		http_maps_queue_status(job_id, "Downloading map zip...");
#if FEATURE_INTERNAL_MENUS
		http_maps_set_loading_status("HTTP Downloading...");
#else
		PrintOut(PRINT_DEV, "http_maps: UI _sofbuddy_loading_status HTTP Downloading...\n");
		if (detour_Cvar_Set2::oCvar_Set2)
			detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_loading_status"), const_cast<char*>("HTTP Downloading..."), true);
#endif
		if (http_maps_download_zip_winhttp(dl_url, temp_zip_path, job_id)) {
			http_maps_queue_status(job_id, "Extracting zip to user/...");
			PrintOut(PRINT_DEV, "http_maps: extract %s\n", map_bsp_path.c_str());
			if (http_maps_extract_and_validate_zip(temp_zip_path, map_bsp_path, job_id)) {
				DeleteFileA(temp_zip_path.c_str());
				PrintOut(PRINT_DEV, "http_maps: ok %s\n", map_bsp_path.c_str());
				http_maps_queue_status(job_id, "HTTP map ready.");
				g_http_maps_state.worker_did_download.store(true, std::memory_order_release);
				success = true;
			} else {
				DeleteFileA(temp_zip_path.c_str());
				PrintOut(PRINT_BAD, "http_maps: extract failed %s\n", map_bsp_path.c_str());
				http_maps_queue_status(job_id, "Zip extraction or CRC validation failed.");
			}
		} else {
			DeleteFileA(temp_zip_path.c_str());
			PrintOut(PRINT_BAD, "http_maps: download failed %s\n", map_bsp_path.c_str());
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
	_sofbuddy_http_maps = detour_Cvar_Get::oCvar_Get("_sofbuddy_http_maps", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_dl_1 = detour_Cvar_Get::oCvar_Get("_sofbuddy_http_maps_dl_1", kHttpMapsDefaultRepo, CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_dl_2 = detour_Cvar_Get::oCvar_Get("_sofbuddy_http_maps_dl_2", kHttpMapsSecondaryRepo, CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_dl_3 = detour_Cvar_Get::oCvar_Get("_sofbuddy_http_maps_dl_3", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_crc_1 = detour_Cvar_Get::oCvar_Get("_sofbuddy_http_maps_crc_1", kHttpMapsDefaultRepo, CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_crc_2 = detour_Cvar_Get::oCvar_Get("_sofbuddy_http_maps_crc_2", kHttpMapsSecondaryRepo, CVAR_SOFBUDDY_ARCHIVE, nullptr);
	_sofbuddy_http_maps_crc_3 = detour_Cvar_Get::oCvar_Get("_sofbuddy_http_maps_crc_3", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
	detour_Cvar_Get::oCvar_Get("_sofbuddy_http_show_providers", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
	http_maps_reset_state();
}

void http_maps_reset_state(void)
{
	g_http_maps_state.waiting = false;
	g_http_maps_state.waiting_started_ms = 0;
	g_http_maps_state.pending_map_bsp.clear();
	g_http_maps_state.continue_callback = nullptr;
	g_http_maps_state.deferred_continue_reason = nullptr;
	g_http_maps_state.active_job_id.store(0, std::memory_order_release);
	g_http_maps_state.worker_running.store(false, std::memory_order_release);
	g_http_maps_state.worker_result.store(static_cast<int>(HttpMapsWorkerResult::None), std::memory_order_release);
	g_http_maps_state.loading_ui_active = false;
	g_http_maps_state.loading_ui_map.clear();
	g_http_maps_state.cached_map_bsp.clear();
	g_http_maps_state.completed_map_bsp.clear();
	g_http_maps_state.worker_success_no_crc_list.store(false, std::memory_order_release);
	g_http_maps_state.worker_did_download.store(false, std::memory_order_release);
	g_http_maps_state.progress_dirty.store(false, std::memory_order_release);
	http_maps_clear_loading_cvars(true);
}

static void http_maps_start_worker(const std::string& map_bsp_path, detour_CL_Precache_f::tCL_Precache_f continue_callback)
{
	const std::string zip_rel_path = http_maps_bsp_to_zip_relpath(map_bsp_path);
	const uint32_t job_id = g_http_maps_state.next_job_id.fetch_add(1, std::memory_order_acq_rel);
	g_http_maps_state.active_job_id.store(job_id, std::memory_order_release);
	g_http_maps_state.worker_running.store(true, std::memory_order_release);
	g_http_maps_state.worker_did_download.store(false, std::memory_order_release);
	g_http_maps_state.waiting = true;
	g_http_maps_state.waiting_started_ms = GetTickCount();
	g_http_maps_state.pending_map_bsp = map_bsp_path;
	g_http_maps_state.continue_callback = continue_callback;
	g_http_maps_state.deferred_continue_reason = nullptr;
	g_http_maps_state.worker_result.store(static_cast<int>(HttpMapsWorkerResult::None), std::memory_order_release);
	g_http_maps_state.worker_progress.store(0.0f, std::memory_order_release);
	g_http_maps_state.progress_dirty.store(true, std::memory_order_release);
	g_http_maps_state.loading_ui_active = true;
	g_http_maps_state.loading_ui_map = map_bsp_path;
	http_maps_clear_pending_ui_updates();
#if FEATURE_INTERNAL_MENUS
	http_maps_apply_progress_cvar(0.0f);
	http_maps_loading_ui_show_map(map_bsp_path.c_str());
#endif
	try {
		std::thread(http_maps_download_worker, map_bsp_path, zip_rel_path, job_id).detach();
	} catch (...) {
		PrintOut(PRINT_BAD, "http_maps: worker thread failed %s\n", map_bsp_path.c_str());
#if FEATURE_INTERNAL_MENUS
		PrintOut(PRINT_DEV, "http_maps: UI _sofbuddy_loading_zip_indicator loading/loading_zip_red\n");
		if (detour_Cvar_Set2::oCvar_Set2)
			detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_loading_zip_indicator"), const_cast<char*>("loading/loading_zip_red"), true);
#endif
		if (g_http_maps_state.active_job_id.load(std::memory_order_acquire) == job_id)
			g_http_maps_state.active_job_id.store(0, std::memory_order_release);
		g_http_maps_state.waiting = false;
		g_http_maps_state.waiting_started_ms = 0;
		g_http_maps_state.pending_map_bsp.clear();
		g_http_maps_state.continue_callback = nullptr;
		g_http_maps_state.deferred_continue_reason = nullptr;
		g_http_maps_state.worker_running.store(false, std::memory_order_release);
		g_http_maps_state.loading_ui_active = false;
		g_http_maps_state.loading_ui_map.clear();
		http_maps_clear_loading_cvars(false);
		if (continue_callback) continue_callback();
	}
}

static void http_maps_try_start_worker_early(const std::string& map_bsp_path)
{
	if (!http_maps_is_enabled()) return;
	if (g_http_maps_state.waiting && g_http_maps_state.pending_map_bsp == map_bsp_path) return;
	if (g_http_maps_state.completed_map_bsp == map_bsp_path) return;
	http_maps_start_worker(map_bsp_path, nullptr);
}

struct HttpMapsSizebuf
{
	int unknown;
	byte* data;
	int maxsize;
	int cursize;
	int readcount;
};

static int s_configstring_index = -1;

static bool http_maps_peek_configstring_index(int& out_index)
{
	HttpMapsSizebuf* msg = reinterpret_cast<HttpMapsSizebuf*>(rvaToAbsExe(reinterpret_cast<void*>(kHttpMaps_NetMessageRva)));
	if (!msg || !msg->data) return false;
	const int readcount = msg->readcount;
	const int cursize = msg->cursize;
	if (readcount < 0 || readcount + 2 > cursize) {
		out_index = -1;
		return true;
	}
	out_index = static_cast<int>(static_cast<short>(msg->data[readcount] | (msg->data[readcount + 1] << 8)));
	return true;
}

void http_maps_cl_parseconfigstring_pre_callback(void)
{
	int index = -1;
	s_configstring_index = http_maps_peek_configstring_index(index) ? index : -1;
}
bool http_maps_should_run_on_configstring_post(void) { return s_configstring_index == kHttpMaps_ConfigStringMapModelIndex; }

void http_maps_on_parse_configstring_post(void)
{
	std::string map_bsp_path;
	if (!http_maps_read_map_from_memory(map_bsp_path, false, false)) return;
	if (!http_maps_normalize_map_path(map_bsp_path)) return;
	// After CL_Precache completes, http_maps sets completed_map_bsp. The server often sends a second
	// full configstring wave right before/during spawn (after Serverdata). That must not call
	// loading_show_ui again or restart the early worker — it looks like the loading screen returns
	// and can disrupt the connection.
	if (!g_http_maps_state.completed_map_bsp.empty() &&
	    g_http_maps_state.completed_map_bsp == map_bsp_path) {
		return;
	}
	if (g_http_maps_state.cached_map_bsp == map_bsp_path) return;
	PrintOut(PRINT_LOG, "http_maps: ParseConfigString post: map %s (early)\n", map_bsp_path.c_str());
	g_http_maps_state.cached_map_bsp = map_bsp_path;
	// Dream fast-path: if engine FS already resolves the BSP (including pak/base), mark as present
	// immediately and skip early HTTP worker altogether.
	if (http_maps_map_exists_via_engine(map_bsp_path)) {
#if FEATURE_INTERNAL_MENUS
		http_maps_loading_ui_show_map(map_bsp_path.c_str());
		http_maps_set_loading_status("NOT NEEDED");
#endif
		g_http_maps_state.completed_map_bsp = map_bsp_path;
		return;
	}
	http_maps_try_start_worker_early(map_bsp_path);
}

bool http_maps_should_skip_loading_plaque_menu(void)
{
	if (!http_maps_is_enabled()) return false;
	std::string map_bsp_path;
	if (!http_maps_read_map_from_memory(map_bsp_path, false, false)) return false;
	if (!http_maps_normalize_map_path(map_bsp_path)) return false;
	// After precache, a second SCR_BeginLoadingPlaque often runs; killmenu+push flashes the loading
	// UI and can destabilize the client right before spawn.
	return !g_http_maps_state.completed_map_bsp.empty() &&
	       g_http_maps_state.completed_map_bsp == map_bsp_path;
}

void http_maps_try_begin_precache(detour_CL_Precache_f::tCL_Precache_f original)
{
	SOFBUDDY_ASSERT(original != nullptr);

	if (!http_maps_is_enabled()) {
		http_maps_clear_loading_cvars();
		original();
		return;
	}

	std::string map_bsp_path;
	if (!http_maps_resolve_map_for_precache(map_bsp_path)) {
		http_maps_clear_loading_cvars();
		original();
		return;
	}
	PrintOut(PRINT_LOG, "http_maps: CL_Precache_f: map %s\n", map_bsp_path.c_str());
	if (g_http_maps_state.completed_map_bsp == map_bsp_path) {
		original();
		return;
	}
	// If we get here without early completion, still short-circuit on engine FS availability.
	if (!(g_http_maps_state.waiting && g_http_maps_state.pending_map_bsp == map_bsp_path) &&
		http_maps_map_exists_via_engine(map_bsp_path)) {
#if FEATURE_INTERNAL_MENUS
		http_maps_loading_ui_show_map(map_bsp_path.c_str());
		http_maps_set_loading_status("NOT NEEDED");
#endif
		g_http_maps_state.completed_map_bsp = map_bsp_path;
		original();
		return;
	}

#if FEATURE_INTERNAL_MENUS
	if (!(g_http_maps_state.waiting && g_http_maps_state.pending_map_bsp == map_bsp_path))
		http_maps_set_loading_status("CHECKING");
#endif

	if (g_http_maps_state.waiting && g_http_maps_state.pending_map_bsp == map_bsp_path) {
		PrintOut(PRINT_LOG, "http_maps: CL_Precache_f: already waiting for %s (UI set here)\n", map_bsp_path.c_str());
		g_http_maps_state.continue_callback = original;
		g_http_maps_state.loading_ui_active = true;
		g_http_maps_state.loading_ui_map = map_bsp_path;
		// deferred_continue_reason is only set in http_maps_pump() when the worker has finished.
		// If the worker completed before this CL_Precache_f hook ran, run_deferred_continue_if_pending()
		// alone would no-op forever; pump consumes worker_result and then continues precache.
		http_maps_pump();
		return;
	}

	PrintOut(PRINT_LOG, "http_maps: CL_Precache_f: starting worker for %s (late)\n", map_bsp_path.c_str());
	http_maps_start_worker(map_bsp_path, original);
}

// cls.state can glitch to ca_disconnected for a single frame during map load / configstring churn.
// Resetting http_maps there clears cached_map_bsp / completed_map_bsp while still connected, which
// retriggers early worker + loading UI and looks like a stuck reconnect loop.
// Require two consecutive observations of ca_disconnected (call from one site per frame only).
static int s_prev_cls_state_for_http_maps = -1;
static bool s_http_maps_reset_done_while_disconnected = false;

void http_maps_on_frame_cls_state(int state)
{
	const int disc = static_cast<int>(ca_disconnected);
	if (state == disc) {
		if (s_prev_cls_state_for_http_maps == disc && !s_http_maps_reset_done_while_disconnected) {
			http_maps_reset_state();
			s_http_maps_reset_done_while_disconnected = true;
		}
	} else {
		s_http_maps_reset_done_while_disconnected = false;
	}
	s_prev_cls_state_for_http_maps = state;
}

bool http_maps_frame_work_pending(void)
{
	return g_http_maps_state.waiting || g_http_maps_state.deferred_continue_reason != nullptr;
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
	if (g_http_maps_state.waiting_started_ms != 0) {
		const DWORD elapsed = GetTickCount() - g_http_maps_state.waiting_started_ms;
		// Watchdog only applies once CL_Precache_f has registered continue_callback. Early worker
		// (ParseConfigString before precache) can finish with callback still null — do not force continue.
		if (elapsed >= kHttpMapsContinueWatchdogMs && !g_http_maps_state.deferred_continue_reason &&
			g_http_maps_state.continue_callback) {
			PrintOut(PRINT_BAD, "http_maps: wait timeout %lu ms, continuing precache\n", static_cast<unsigned long>(elapsed));
			g_http_maps_state.deferred_continue_reason = "http wait watchdog timeout";
		}
	}

	if (g_http_maps_state.worker_running.load(std::memory_order_acquire)) {
		http_maps_run_deferred_continue_if_pending();
		return;
	}

	// ParseConfigString may start the download worker with continue_callback == nullptr. If the worker
	// finishes before CL_Precache_f runs, keep worker_result until the callback exists or we never
	// run the real precache (and mark the map complete without loading), which breaks the client and
	// can look like a connect/reconnect loop — especially when loading UI timing differs (e.g. unlocked input).
	if (!g_http_maps_state.continue_callback) {
		const int wr = g_http_maps_state.worker_result.load(std::memory_order_acquire);
		if (wr != static_cast<int>(HttpMapsWorkerResult::None)) {
			// Worker may have already concluded "assume local/PAK" (e.g. CRC list unavailable).
			// Even while we defer continue until CL_Precache_f registers callback, keep UI status
			// truthful by checking engine FS now on the main thread.
#if FEATURE_INTERNAL_MENUS
			if (detour_Cvar_Set2::oCvar_Set2 && !g_http_maps_state.pending_map_bsp.empty()) {
				const bool map_present = http_maps_map_exists_via_engine(g_http_maps_state.pending_map_bsp);
				const bool did_http_dl = g_http_maps_state.worker_did_download.load(std::memory_order_acquire);
				if (map_present && did_http_dl) http_maps_set_loading_status("DOWNLOADED");
				else if (map_present) http_maps_set_loading_status("NOT NEEDED");
				else http_maps_set_loading_status("UDP Downloading...");
			}
#endif
			http_maps_run_deferred_continue_if_pending();
			return;
		}
	}

	const HttpMapsWorkerResult result = static_cast<HttpMapsWorkerResult>(
		g_http_maps_state.worker_result.exchange(static_cast<int>(HttpMapsWorkerResult::None), std::memory_order_acq_rel)
	);
	if (result == HttpMapsWorkerResult::None) {
		http_maps_run_deferred_continue_if_pending();
		return;
	}
#if FEATURE_INTERNAL_MENUS
	if (detour_Cvar_Set2::oCvar_Set2) {
		// Clear worker flags; map-present vs HTTP-downloaded disambiguates NOT NEEDED vs DOWNLOADED.
		(void)g_http_maps_state.worker_success_no_crc_list.exchange(false, std::memory_order_acq_rel);
		const bool did_http_download = g_http_maps_state.worker_did_download.exchange(false, std::memory_order_acq_rel);
		const bool map_present = http_maps_map_exists_via_engine(g_http_maps_state.pending_map_bsp);
		const char* end_status =
			(map_present && did_http_download) ? "DOWNLOADED" : (map_present ? "NOT NEEDED" : "UDP Downloading...");
		if (result == HttpMapsWorkerResult::Success || result == HttpMapsWorkerResult::Failure) {
			// Success: CRC skip / no-CRC-list fallback → NOT NEEDED if FS sees map; HTTP download+extract → DOWNLOADED.
			// Failure: HTTP path failed — still show NOT NEEDED if map is already in a pak / on disk.
			http_maps_set_loading_status(end_status);
		} else {
			cvar_t* cur = findCvar(const_cast<char*>("_sofbuddy_loading_status"));
			if (cur && cur->string && strcmp(cur->string, "CHECKING") == 0)
				http_maps_set_loading_status(end_status);
		}
	}
#endif
	if (result == HttpMapsWorkerResult::Success) {
		g_http_maps_state.deferred_continue_reason = "http download success";
	} else if (result == HttpMapsWorkerResult::Failure) {
		g_http_maps_state.deferred_continue_reason = "http download failed";
	} else {
		g_http_maps_state.deferred_continue_reason = "http worker ended without result";
	}

	http_maps_run_deferred_continue_if_pending();
}

static void http_maps_continue_precache(const char* reason)
{
	(void)reason;
	detour_CL_Precache_f::tCL_Precache_f callback = g_http_maps_state.continue_callback;
	if (!callback) {
		// Defensive: should not clear state without running the engine precache path.
		g_http_maps_state.deferred_continue_reason = reason;
		return;
	}
	std::string map_name = g_http_maps_state.pending_map_bsp;
	g_http_maps_state.completed_map_bsp = map_name; // Mark this map as fully processed.
	g_http_maps_state.waiting = false;
	g_http_maps_state.waiting_started_ms = 0;
	g_http_maps_state.pending_map_bsp.clear();
	g_http_maps_state.continue_callback = nullptr;
	g_http_maps_state.deferred_continue_reason = nullptr;
	g_http_maps_state.active_job_id.store(0, std::memory_order_release);
	g_http_maps_state.worker_running.store(false, std::memory_order_release);
	g_http_maps_state.worker_result.store(static_cast<int>(HttpMapsWorkerResult::None), std::memory_order_release);
	g_http_maps_state.loading_ui_active = false;
	g_http_maps_state.loading_ui_map.clear();
	g_http_maps_state.progress_dirty.store(false, std::memory_order_release);

	if (!callback) return;
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
