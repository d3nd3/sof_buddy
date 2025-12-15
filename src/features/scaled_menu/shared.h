#pragma once

#include "feature_config.h"

#if FEATURE_SCALED_MENU

#ifdef UI_MENU

#include "../scaled_ui_base/shared.h"

char *__thiscall my_rect_c_Parse(void* toke_c, int idx);
void my_Draw_GetPicSize(int *w, int *h, char *pic);

void scaledMenu_EarlyStartup(void);

#endif // UI_MENU

#endif // FEATURE_SCALED_MENU


