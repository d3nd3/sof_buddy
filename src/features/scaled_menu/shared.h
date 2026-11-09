#pragma once

#include "feature_config.h"

#if FEATURE_SCALED_MENU

#ifdef UI_MENU

#include "../scaled_ui_base/shared.h"
#include "menu_variant.h"

void scaled_menu_refresh_open_canvases(void);
void scaled_menu_reload_open_page(void);
void scaled_menu_on_vid_layout_changed(void);
bool scaled_menu_install_hooks(void);
bool scaled_menu_uniform_actual_active(void);
void scaled_menu_set_pushed_menu(const char* name);
float scaled_menu_screen_scale(void);
// Re-read the engine's live vid size and re-derive menuScale before menu layout runs.
void scaled_menu_sync_scale_from_engine(void);
float scaled_menu_snapped_text_scale(realFontEnum_t font);
void scaledMenu_EarlyStartup(void);
#if FEATURE_SCALED_MENU && FEATURE_INTRO_SCALER
void scaled_menu_backdrop_clip_extend(int* x, int* w, const void* menu_c);
#endif

#endif // UI_MENU

#endif // FEATURE_SCALED_MENU
