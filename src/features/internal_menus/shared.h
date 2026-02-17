#pragma once

#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include <string>
#include <map>
#include <vector>
#include <cstdint>

extern std::map<std::string, std::map<std::string, std::vector<uint8_t>>> g_menu_internal_files;

constexpr int kInternalMenusLoadingHistoryRows = 5;
constexpr int kInternalMenusLoadingStatusRows = 8;
constexpr int kInternalMenusLoadingFileRows = 8;

void internal_menus_load_library(void);
void internal_menus_EarlyStartup(void);
void internal_menus_PostCvarInit(void);
void internal_menus_OnVidChanged(void);
void Cmd_SoFBuddy_Menu_f(void);
bool internal_menus_should_lock_loading_input(void);
void loading_push_history(const char* map_name);
void loading_push_status(const char* msg);
void loading_set_current(const char* map_name);
void loading_set_files(const char* const* filenames, int count);

#endif
