#pragma once

#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include <string>
#include <map>
#include <vector>
#include <cstdint>

extern std::map<std::string, std::map<std::string, std::vector<uint8_t>>> g_menu_internal_files;
extern cvar_t* _sofbuddy_menu_internal;
extern bool g_sofbuddy_menu_calling;
extern bool g_internal_menus_external_loading_push;

void internal_menus_load_library(void);
void internal_menus_EarlyStartup(void);
void internal_menus_PostCvarInit(void);
void Cmd_SoFBuddy_Menu_f(void);
void loading_push_history(const char* map_name);
void loading_push_status(const char* msg);
void loading_set_current(const char* map_name);
void loading_set_files(const char* const* filenames, int count);

#endif
