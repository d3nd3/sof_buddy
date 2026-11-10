#pragma once

#include "feature_config.h"

#if FEATURE_SCALED_MENU

#ifdef UI_MENU

#include <cstddef>

// SoFPlus's `_ss` menu variants are deprecated. `scaled_menu_variant_redirect()`
// rewrites a trailing `_ss` to `_ns` and records that the wide variant was
// requested (the FS loader uses that to inject the 16:9 chrome). `_ns` requests
// clear the flag. Names without a variant suffix are returned unchanged.
const char* scaled_menu_variant_redirect(const char* name, char* buffer, size_t buffer_size);
void scaled_menu_variant_reset(void);
bool scaled_menu_wide_variant_active(void);
void scaled_menu_note_ss_source(void);
bool scaled_menu_ss_content_page_active(void);
void scaled_menu_note_ss_content_page(bool active);

#endif // UI_MENU

#endif // FEATURE_SCALED_MENU
