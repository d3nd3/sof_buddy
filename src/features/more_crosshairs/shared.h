#pragma once

#include <cstdint>
#include "feature_config.h"

#if FEATURE_MORE_CROSSHAIRS

void more_crosshairs_PostCvarInit(void);
void more_crosshairs_pump_deferred_menu_refresh(void);
void more_crosshairs_apply_pick_index(int index);
void more_crosshairs_set_active_buddy_page(const char* menu_file);
void more_crosshairs_refresh_menu_preview(void);
void more_crosshairs_sync_engine_pic_path(void);
void more_crosshairs_on_getpicsize(char* pic);
bool more_crosshairs_is_menu_preview_path(const char* pic);
// While on F12 Gameplay: map cached menu preview pic names to the current list pick.
const char* more_crosshairs_menu_preview_pic_name(const char* imgname);
void more_crosshairs_before_draw_crosshair(void);
void more_crosshairs_apply_stock_tint_gl(void);
const char* more_crosshairs_get_list_rmf(void);
const char* more_crosshairs_get_preview_rmf(void);
void more_crosshairs_gl_findimage_pre(char*& filename, int& imagetype, char& mimap, char& allowPicmip);
void* more_crosshairs_gl_findimage_fixup(void* result, char* filename, int imagetype, char mimap,
                                         char allowPicmip);
int more_crosshairs_menu_pick_count(void);
bool more_crosshairs_menu_pick_at(int index, int* theme, int* slot);
// theme >= kCrosshairThemeCount: load from a base/gamedir crosshair .pak (not bundled catalog).
bool more_crosshairs_theme_external_pak(int theme, const char** pak_path, const int8_t** slot_map);
#endif
