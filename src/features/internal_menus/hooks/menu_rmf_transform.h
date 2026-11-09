#pragma once

#include "feature_config.h"

#if FEATURE_SCALED_MENU

#include <string>

// Rewrites an `_ss` path to `_ns` in place (fallback for loads that bypass the
// toke hooks) and records that the wide variant was requested.
bool scaled_menu_redirect_ss_path(char* path);

// Applies the Scaled Menu RMF text transforms to `_ns` menu content: the bghoul
// vertical ratio and, when the `_ss` variant was requested, the 16:9 chrome.
// Returns true when `content` changed and must be served from a new buffer.
bool scaled_menu_transform_rmf_content(const char* path, std::string& content);

#endif // FEATURE_SCALED_MENU
