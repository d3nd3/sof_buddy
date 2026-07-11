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
void internal_menus_update_connect_flow(int cls_state);
bool internal_menus_mp_connect_flow_active(void);
bool internal_menus_savegame_load_active(void); // plaque scoped: sv.loadgame and cls!=7 (SV_Map vs CL_Changing)
void internal_menus_begin_loading_plaque_context(int cls_state);
void internal_menus_end_loading_plaque_context(void);
void internal_menus_on_loadgame_map_load(void);
// Pak vanilla loading.rmf for local loads; custom UI during MP connect or http_maps download.
bool internal_menus_use_vanilla_loading_menu(void);
bool internal_menus_should_killmenu_before_loading(void);
void internal_menus_sync_loading_network_ui(void);
bool internal_menus_is_mp_loading_context(void);
bool internal_menus_should_lock_loading_input(void);
const char* internal_menus_loading_menu_name(void);
void loading_set_current(const char* map_name);
void loading_reset_current_map_unknown(void);
void loading_show_ui(void);
void internal_menus_call_SCR_UpdateScreen(bool force);
const char* internal_menus_get_content_inset_rmf(void);
const char* internal_menus_get_content_inset_tall_rmf(void);
void internal_menus_remember_menu_page(const char* menu_file);

#endif
