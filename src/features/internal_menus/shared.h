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
bool internal_menus_should_lock_loading_input(void);
void loading_set_current(const char* map_name);
void loading_seed_current_from_engine_mapname(void);
void loading_show_ui(void);

#endif
