#include "runtime_features.h"

#include "feature_config.h"
#include "default_feature_list.inc"
#include "util.h"

#include <windows.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace RuntimeFeatures {

std::uint32_t g_enabled_mask = 0;

namespace {

#define COMPILED_FEATURE(macro, name, label) (macro != 0),
const bool kCompiledFeatures[] = {
    FEATURE_LIST(COMPILED_FEATURE)
    (FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU)
};
#undef COMPILED_FEATURE

constexpr size_t kFeatureCount = static_cast<size_t>(Feature::Count);
constexpr size_t kScaledUiBaseIndex =
    static_cast<size_t>(Feature::RUNTIME_FEATURE_SCALED_UI_BASE);
static_assert(kFeatureCount <= 32, "Runtime feature mask is full");
constexpr const char* kFeatureCvarPrefix = "_sofbuddy_feature_";

#define DEFAULT_FEATURE(name, enabled) (enabled != 0),
const bool kDefaultFeatures[] = {
    DEFAULT_FEATURE_LIST(DEFAULT_FEATURE)
};
#undef DEFAULT_FEATURE

static_assert(sizeof(kCompiledFeatures) / sizeof(bool) == kFeatureCount,
              "Compiled feature list is out of sync");
static_assert(sizeof(kDefaultFeatures) / sizeof(bool) == kScaledUiBaseIndex,
              "Default feature list is out of sync");
bool g_loaded = false;

std::string trim(std::string value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

Feature feature_from_name(const char* name) {
    if (!name) return Feature::Count;
#define FEATURE_NAME(macro, feature_name, label) \
    if (std::strcmp(name, feature_name) == 0) return Feature::RUNTIME_##macro;
    FEATURE_LIST(FEATURE_NAME)
#undef FEATURE_NAME
    return Feature::Count;
}

void set_enabled(Feature feature, bool enabled) {
    const size_t index = static_cast<size_t>(feature);
    if (index >= kFeatureCount || !kCompiledFeatures[index]) return;
    const std::uint32_t bit = std::uint32_t(1) << index;
    if (enabled) {
        g_enabled_mask |= bit;
    } else {
        g_enabled_mask &= ~bit;
    }
}

std::wstring module_directory() {
    HMODULE module = nullptr;
    const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (!GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(&Load), &module)) {
        return L"";
    }

    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(module, path, ARRAYSIZE(path));
    if (!length || length >= ARRAYSIZE(path)) return L"";

    std::wstring result(path, length);
    const size_t slash = result.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"" : result.substr(0, slash);
}

std::wstring config_path() {
    const std::wstring dir = module_directory();
    return dir.empty() ? L"sof_buddy\\features.cfg" : dir + L"\\sof_buddy\\features.cfg";
}

std::wstring widen(const char* value) {
    if (!value || !value[0]) return L"";
    const int length = MultiByteToWideChar(CP_ACP, 0, value, -1, nullptr, 0);
    if (length <= 1) return L"";
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_ACP, 0, value, -1, &result[0], length);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

void add_config_candidate(std::vector<std::wstring>& paths, std::wstring base,
                          const wchar_t* filename) {
    while (base.size() > 1 && (base.back() == L'\\' || base.back() == L'/'))
        base.pop_back();
    if (base.empty() || !filename || !filename[0]) return;
    const std::wstring path = base + L"\\base\\" + filename;
    for (const std::wstring& existing : paths)
        if (existing == path) return;
    paths.push_back(path);
}

std::vector<std::wstring> config_candidates(const wchar_t* filename) {
    std::vector<std::wstring> candidates;
    cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
    if (basedir && basedir->string) add_config_candidate(candidates, widen(basedir->string), filename);

    const std::wstring module_dir = module_directory();
    add_config_candidate(candidates, module_dir, filename);

    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, path, ARRAYSIZE(path))) {
        std::wstring exe(path);
        const size_t slash = exe.find_last_of(L"\\/");
        if (slash != std::wstring::npos) add_config_candidate(candidates, exe.substr(0, slash), filename);
    }

    DWORD length = GetCurrentDirectoryW(ARRAYSIZE(path), path);
    if (length && length < ARRAYSIZE(path)) add_config_candidate(candidates, path, filename);
    return candidates;
}

bool candidate_exists(const std::vector<std::wstring>& paths) {
    for (const std::wstring& path : paths) {
        const DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) return true;
    }
    return false;
}

bool candidate_has_feature_lines(const std::vector<std::wstring>& paths) {
    for (const std::wstring& path : paths) {
        FILE* file = _wfopen(path.c_str(), L"rb");
        if (!file) continue;
        bool found = false;
        char buffer[512];
        while (std::fgets(buffer, sizeof(buffer), file)) {
            if (std::strstr(buffer, "set _sofbuddy_feature_")) {
                found = true;
                break;
            }
        }
        std::fclose(file);
        if (found) return true;
    }
    return false;
}

bool config_is_runtime_generated() {
    FILE* file = _wfopen(config_path().c_str(), L"rb");
    if (!file) return false;
    char buffer[512] = {};
    const bool generated = std::fgets(buffer, sizeof(buffer), file) &&
                           std::strstr(buffer, "Generated from _sofbuddy_feature_");
    std::fclose(file);
    return generated;
}

bool read_feature_defaults_file(const std::wstring& path,
                                std::map<std::string, bool>& out) {
    FILE* file = _wfopen(path.c_str(), L"rb");
    if (!file) return false;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), file)) {
        std::string line = trim(buffer);
        if (line.empty()) continue;
        bool enabled = true;
        if (line.compare(0, 2, "//") == 0) {
            enabled = false;
            line = trim(line.substr(2));
        } else if (line[0] == '#') {
            enabled = false;
            line = trim(line.substr(1));
        }
        const size_t comment = line.find_first_of("#");
        if (comment != std::string::npos) line = trim(line.substr(0, comment));
        const size_t slash = line.find("//");
        if (slash != std::string::npos) line = trim(line.substr(0, slash));
        if (line.empty()) continue;
        if (feature_from_name(line.c_str()) != Feature::Count)
            out[line] = enabled;
    }
    std::fclose(file);
    return true;
}

bool read_archive_values(const std::wstring& path, std::map<std::string, int>& out) {
    FILE* file = _wfopen(path.c_str(), L"rb");
    if (!file) return false;
    char buffer[512];
    const char* prefix = "set _sofbuddy_feature_";
    while (std::fgets(buffer, sizeof(buffer), file)) {
        std::string line = trim(buffer);
        if (line.compare(0, std::strlen(prefix), prefix) != 0) continue;
        const size_t value_start = line.find_first_of(" \t", std::strlen(prefix));
        if (value_start == std::string::npos) continue;
        const std::string name = line.substr(std::strlen(prefix),
                                             value_start - std::strlen(prefix));
        std::string value = trim(line.substr(value_start));
        if (value.size() >= 2 && value.front() == '"') {
            const size_t end = value.find('"', 1);
            if (end == std::string::npos) continue;
            value = value.substr(1, end - 1);
        } else {
            const size_t end = value.find_first_of(" \t");
            if (end != std::string::npos) value.resize(end);
        }
        char* end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (!end || *end || parsed < 0 || parsed > 2) continue;
        out[name] = static_cast<int>(parsed);
    }
    std::fclose(file);
    return true;
}

void write_feature_state();

bool merge_previous_feature_defaults() {
    const std::wstring prev_path = config_path() + L".prev";
    const DWORD prev_attrs = GetFileAttributesW(prev_path.c_str());
    if (prev_attrs == INVALID_FILE_ATTRIBUTES || (prev_attrs & FILE_ATTRIBUTE_DIRECTORY))
        return false;

    std::map<std::string, bool> old_defaults;
    std::map<std::string, bool> new_defaults;
    if (!read_feature_defaults_file(prev_path, old_defaults)) {
        DeleteFileW(prev_path.c_str());
        return false;
    }
    read_feature_defaults_file(config_path(), new_defaults);

    std::map<std::string, int> archive;
    for (const std::wstring& candidate : config_candidates(L"sofbuddy_features.cfg")) {
        if (read_archive_values(candidate, archive)) break;
    }

#define MERGE_FEATURE(macro, name, label) { \
        const bool old_default = old_defaults.count(name) ? old_defaults[name] : false; \
        const bool new_default = new_defaults.count(name) ? new_defaults[name] : false; \
        const auto it = archive.find(name); \
        const bool explicit_override = it != archive.end() && it->second != 2 && \
                                       ((it->second != 0) != old_default); \
        set_enabled(Feature::RUNTIME_##macro, explicit_override ? it->second != 0 : new_default); \
    }
    FEATURE_LIST(MERGE_FEATURE)
#undef MERGE_FEATURE

    write_feature_state();
    DeleteFileW(prev_path.c_str());
    return true;
}

std::wstring game_base_directory() {
    cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
    if (basedir && basedir->string && basedir->string[0]) {
        std::wstring base = widen(basedir->string);
        while (base.size() > 1 && (base.back() == L'\\' || base.back() == L'/'))
            base.pop_back();
        if (!base.empty()) return base;
    }
    const std::wstring module_dir = module_directory();
    if (!module_dir.empty()) return module_dir;

    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, path, ARRAYSIZE(path))) {
        std::wstring exe(path);
        const size_t slash = exe.find_last_of(L"\\/");
        if (slash != std::wstring::npos) return exe.substr(0, slash);
    }
    DWORD length = GetCurrentDirectoryW(ARRAYSIZE(path), path);
    if (length && length < ARRAYSIZE(path)) return std::wstring(path, length);
    return L"";
}

bool parse_feature_stream(FILE* file) {
    bool found = false;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), file)) {
        std::string line = trim(buffer);
        const char* prefix = "set _sofbuddy_feature_";
        if (line.compare(0, std::strlen(prefix), prefix) != 0) continue;

        const size_t value_start = line.find_first_of(" \t", std::strlen(prefix));
        if (value_start == std::string::npos) continue;
        const std::string name = line.substr(std::strlen(prefix),
                                             value_start - std::strlen(prefix));
        std::string value = trim(line.substr(value_start));
        if (value.size() >= 2 && value.front() == '"') {
            const size_t end = value.find('"', 1);
            if (end == std::string::npos) continue;
            value = value.substr(1, end - 1);
        } else {
            const size_t end = value.find_first_of(" \t");
            if (end != std::string::npos) value.resize(end);
        }

        char* end = nullptr;
        const long selection = std::strtol(value.c_str(), &end, 10);
        const std::string trailing = end ? trim(end) : "";
        if (!end || !trailing.empty() || selection < 0 || selection > 1)
            continue;
        const Feature feature = feature_from_name(name.c_str());
        if (feature != Feature::Count && kCompiledFeatures[static_cast<size_t>(feature)]) {
            set_enabled(feature, selection != 0);
            found = true;
        }
    }
    return found;
}

bool load_feature_candidates(const std::vector<std::wstring>& candidates) {
    for (const std::wstring& candidate : candidates) {
        FILE* file = _wfopen(candidate.c_str(), L"rb");
        if (!file) continue;
        parse_feature_stream(file);
        std::fclose(file);
        return true;
    }
    return false;
}

bool load_persisted_features() {
    if (load_feature_candidates(config_candidates(L"sofbuddy_features.cfg")))
        return true;
    return load_feature_candidates(config_candidates(L"sofbuddy.cfg"));
}

void write_feature_state() {
    const std::wstring base = game_base_directory();
    if (base.empty()) return;
    const std::wstring dir = base + L"\\base";
    CreateDirectoryW(dir.c_str(), nullptr);
    const std::wstring path = dir + L"\\sofbuddy_features.cfg";
    const std::wstring temp_path = path + L".tmp";
    FILE* file = _wfopen(temp_path.c_str(), L"wb");
    if (!file) {
        PrintOut(PRINT_BAD, "Runtime features: unable to write sofbuddy_features.cfg\n");
        return;
    }
    std::fprintf(file, "# Generated feature toggles. Restart SoF after changing them.\n\n");
#define WRITE_FEATURE_STATE(macro, name, label) \
    std::fprintf(file, "set _sofbuddy_feature_%s \"%d\"\n", name, \
                 Enabled(Feature::RUNTIME_##macro) ? 1 : 0);
    FEATURE_LIST(WRITE_FEATURE_STATE)
#undef WRITE_FEATURE_STATE
    std::fclose(file);
    if (!MoveFileExW(temp_path.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp_path.c_str());
        PrintOut(PRINT_BAD, "Runtime features: unable to replace sofbuddy_features.cfg\n");
    }
}

void write_config() {
    const std::wstring path = config_path();
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos && slash > 0)
        CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);

    const std::wstring temp_path = path + L".tmp";
    FILE* file = _wfopen(temp_path.c_str(), L"wb");
    if (!file) {
        PrintOut(PRINT_BAD, "Runtime features: unable to write features.cfg\n");
        return;
    }
    std::fprintf(file, "# Generated from _sofbuddy_feature_* selections.\n");
    std::fprintf(file, "# Restart SoF after changing feature selections.\n\n");
#define WRITE_FEATURE(macro, name, label) \
    if (kCompiledFeatures[static_cast<size_t>(Feature::RUNTIME_##macro)]) \
        std::fprintf(file, "%s%s\n", Enabled(Feature::RUNTIME_##macro) ? "" : "// ", name);
    FEATURE_LIST(WRITE_FEATURE)
#undef WRITE_FEATURE
    std::fclose(file);
    if (!MoveFileExW(temp_path.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp_path.c_str());
        PrintOut(PRINT_BAD, "Runtime features: unable to replace features.cfg\n");
    }
    write_feature_state();
}

void set_override(std::string name, bool enabled) {
    name = trim(name);
    const size_t comment = name.find('#');
    if (comment != std::string::npos) name = trim(name.substr(0, comment));
    const Feature feature = feature_from_name(name.c_str());
    if (feature != Feature::Count) set_enabled(feature, enabled);
}

} // namespace

void Load() {
    if (g_loaded) return;
    g_loaded = true;

    for (size_t i = 0; i < kScaledUiBaseIndex; ++i) {
        set_enabled(static_cast<Feature>(i), kDefaultFeatures[i]);
    }

    if (!merge_previous_feature_defaults()) {
        const bool archive_present = candidate_exists(config_candidates(L"sofbuddy_features.cfg"));
        const bool legacy_present =
            candidate_has_feature_lines(config_candidates(L"sofbuddy.cfg"));
        const bool reset_to_defaults =
            !archive_present && !legacy_present && config_is_runtime_generated();

        bool config_present = false;
        if (!reset_to_defaults) {
            FILE* file = _wfopen(config_path().c_str(), L"rb");
            if (file) {
                config_present = true;
                char buffer[512];
                while (std::fgets(buffer, sizeof(buffer), file)) {
                    std::string line = trim(buffer);
                    if (line.empty()) continue;
                    if (line.compare(0, 2, "//") == 0) {
                        set_override(line.substr(2), false);
                    } else if (line[0] == '#') {
                        set_override(line.substr(1), false);
                    } else {
                        set_override(line, true);
                    }
                }
                std::fclose(file);
            }
        }

        const bool persisted = load_persisted_features();
        if (reset_to_defaults || !config_present)
            write_config();
        else
            write_feature_state();
    }

    const std::uint32_t scaledFeatures =
        (std::uint32_t(1) << static_cast<size_t>(Feature::RUNTIME_FEATURE_SCALED_CON)) |
        (std::uint32_t(1) << static_cast<size_t>(Feature::RUNTIME_FEATURE_SCALED_HUD)) |
        (std::uint32_t(1) << static_cast<size_t>(Feature::RUNTIME_FEATURE_SCALED_MENU));
    if (g_enabled_mask & scaledFeatures) {
        g_enabled_mask |= std::uint32_t(1) << kScaledUiBaseIndex;
    }
}

bool Compiled(Feature feature) {
    const size_t index = static_cast<size_t>(feature);
    return index < kFeatureCount && kCompiledFeatures[index];
}

Feature FromCvarName(const char* name) {
    if (!name || std::strncmp(name, kFeatureCvarPrefix, std::strlen(kFeatureCvarPrefix)) != 0)
        return Feature::Count;
    return feature_from_name(name + std::strlen(kFeatureCvarPrefix));
}

int SelectionValue(Feature feature) {
    return !Compiled(feature) ? 2 : (Enabled(feature) ? 1 : 0);
}

} // namespace RuntimeFeatures
