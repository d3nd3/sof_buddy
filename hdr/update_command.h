#pragma once

void sofbuddy_update_init(void);
void sofbuddy_update_maybe_check_startup(void);
bool sofbuddy_update_consume_startup_prompt_request(void);
void Cmd_SoFBuddy_Update_f(void);
void Cmd_SoFBuddy_UpdateInstall_f(void);
void Cmd_SoFBuddy_OpenUrl_f(void);
