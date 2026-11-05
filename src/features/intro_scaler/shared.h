#pragma once

#include "feature_config.h"

#if FEATURE_INTRO_SCALER

#ifdef UI_MENU

bool intro_scaler_install_hooks(void);
void introScaler_EarlyStartup(void);
void intro_scaler_on_startup_menu_pushed(bool disclaimer, bool intro);
bool intro_scaler_intro_cinematic_active(void);
bool intro_scaler_disclaimer_menu_text_active(void);
float intro_scaler_page_width_scale(void);
void intro_scaler_letterbox_apply_in_stm_set_clip(void* menu);
void intro_scaler_set_clip_extend(void (*fn)(int* x, int* w, const void* menu_c));

#endif // UI_MENU

#endif // FEATURE_INTRO_SCALER
