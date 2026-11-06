#pragma once

#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include <string>
#include <map>
#include <vector>
#include <cstdint>

extern std::map<std::string, std::map<std::string, std::vector<uint8_t>>> g_menu_internal_files;

void internal_menus_load_library(void);
void internal_menus_EarlyStartup(void);
void internal_menus_PostCvarInit(void);
void internal_menus_OnVidChanged(void);
void Cmd_SoFBuddy_Menu_f(void);
bool internal_menus_deathmatch_mode_active(void);
bool internal_menus_use_vanilla_loading_menu(void);
bool internal_menus_should_killmenu_before_loading(void);
void internal_menus_sync_loading_network_ui(void);
bool internal_menus_is_mp_loading_context(void);
bool internal_menus_should_lock_loading_input(void);
const char* internal_menus_loading_menu_name(void);
void loading_set_current(const char* map_name);
void loading_reset_current_map_unknown(void);
void loading_show_ui(void);
bool internal_menus_engine_shows_loading_plaque(void);
void internal_menus_note_loading_shown(void);
void internal_menus_push_menu_hooked(const char* menu, const char* parent, bool lock_input);
void internal_menus_scr_updatescreen_post(bool force);
void internal_menus_call_SCR_UpdateScreen(bool force);
const char* internal_menus_get_content_inset_rmf(void);
const char* internal_menus_get_content_inset_tall_rmf(void);
const char* internal_menus_get_tabs_row_prefix_rmf(void);
const char* internal_menus_get_tabs_row_prefix_narrow_rmf(void);
const char* internal_menus_get_tipfont_rmf(void);
const char* internal_menus_get_font_rmf(const char* tier);
const char* internal_menus_get_theme_tints_rmf(void);
const char* internal_menus_get_theme_list_labels_rmf(void);
const char* internal_menus_get_theme_list_match_rmf(void);
const char* internal_menus_get_theme_list_rmf(void);
void internal_menus_remember_menu_page(const char* menu_file);
void internal_menus_feature_prompt_init_cvars(void);
void internal_menus_feature_prompt_init_command(void);
void internal_menus_feature_prompt_note_menu(const char* menu_file);
void internal_menus_feature_prompt_note_closed(void);
void internal_menus_feature_prompt_tick(void);
bool internal_menus_feature_prompt_handle_cvar(const char* cvar_name);

#endif
