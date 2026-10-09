#pragma once

#include "feature_list.inc"

#include <cstdint>

namespace RuntimeFeatures {

enum class Feature : unsigned char {
#define FEATURE_ID(macro, name, label) RUNTIME_##macro,
    FEATURE_LIST(FEATURE_ID)
#undef FEATURE_ID
    RUNTIME_FEATURE_SCALED_UI_BASE,
    Count
};

extern std::uint32_t g_enabled_mask;

void Load();

inline bool Enabled(Feature feature) {
    const unsigned index = static_cast<unsigned>(feature);
    return index < static_cast<unsigned>(Feature::Count) &&
           (g_enabled_mask & (std::uint32_t(1) << index)) != 0;
}

}
