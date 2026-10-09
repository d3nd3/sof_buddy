#include "runtime_features.h"

#include "feature_config.h"
#include "default_feature_list.inc"

#include <windows.h>

#include <cstdio>
#include <string>

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

Feature feature_from_name(const std::string& name) {
#define FEATURE_NAME(macro, feature_name, label) \
    if (name == feature_name) return Feature::RUNTIME_##macro;
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

std::wstring config_path() {
    HMODULE module = nullptr;
    const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (!GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(&Load), &module)) {
        return L"sof_buddy\\features.cfg";
    }

    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(module, path, ARRAYSIZE(path));
    if (!length || length >= ARRAYSIZE(path)) return L"sof_buddy\\features.cfg";

    std::wstring result(path, length);
    const size_t slash = result.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return L"sof_buddy\\features.cfg";
    return result.substr(0, slash) + L"\\sof_buddy\\features.cfg";
}

void set_override(std::string name, bool enabled) {
    name = trim(name);
    const size_t comment = name.find('#');
    if (comment != std::string::npos) name = trim(name.substr(0, comment));
    const Feature feature = feature_from_name(name);
    if (feature != Feature::Count) set_enabled(feature, enabled);
}

} // namespace

void Load() {
    if (g_loaded) return;
    g_loaded = true;

    for (size_t i = 0; i < kScaledUiBaseIndex; ++i) {
        set_enabled(static_cast<Feature>(i), kDefaultFeatures[i]);
    }

    FILE* file = _wfopen(config_path().c_str(), L"rb");
    if (file) {
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

    const std::uint32_t scaledFeatures =
        (std::uint32_t(1) << static_cast<size_t>(Feature::RUNTIME_FEATURE_SCALED_CON)) |
        (std::uint32_t(1) << static_cast<size_t>(Feature::RUNTIME_FEATURE_SCALED_HUD)) |
        (std::uint32_t(1) << static_cast<size_t>(Feature::RUNTIME_FEATURE_SCALED_MENU));
    if (g_enabled_mask & scaledFeatures) {
        g_enabled_mask |= std::uint32_t(1) << kScaledUiBaseIndex;
    }
}

} // namespace RuntimeFeatures
