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
// True once the rect/text draw hooks needed by `<text talign center>` are installed.
bool scaled_menu_text_align_available(void);
// Re-read the engine's live vid size and re-derive menuScale before menu layout runs.
void scaled_menu_sync_scale_from_engine(void);
float scaled_menu_snapped_text_scale(realFontEnum_t font);
// Tooltip background fitting. tip_c::Render draws its bevel box from the font line
// height, but scaled glyph runs grow from the first glyph's top, so the visual text
// box shifts as the menu scale grows. This records the screen-space glyph vertices of
// the tip currently being rendered so the Draw_BeveledBox IAT hook can hug them.
void scaled_menu_tip_bounds_vertex(float x, float y);
void scaledMenu_EarlyStartup(void);
// In the injected 16:9 chrome, anchor a global page's backdrop at the menu origin so
// the background spans the full screen instead of starting at the ss_content X offset.
void scaled_menu_backdrop_clip_anchor(void* stm, const void* menu_c, int* x, int* y,
                                      int* w, int* h);
#if FEATURE_SCALED_MENU && FEATURE_INTRO_SCALER
void scaled_menu_backdrop_clip_extend(int* x, int* w, const void* menu_c);
#endif

#endif // UI_MENU

#endif // FEATURE_SCALED_MENU
