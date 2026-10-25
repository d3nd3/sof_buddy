#pragma once

void sofbuddy_update_init(void);
void sofbuddy_update_maybe_check_startup(void);
bool sofbuddy_update_consume_startup_prompt_request(void);
void sofbuddy_update_show_prompt_menu(void);
void sofbuddy_update_reload_update_menus(void);
const char* sofbuddy_update_prompt_notes_rmf(void);
void Cmd_SoFBuddy_Update_f(void);
void Cmd_SoFBuddy_UpdateInstall_f(void);
void Cmd_SoFBuddy_OpenUrl_f(void);
