#pragma once

#include "feature_config.h"

#if FEATURE_CONSOLE_PROTECTION

extern char *(*oSys_GetClipboardData)(void);
char* hkSys_GetClipboardData(void);
void my_Con_Draw_Console(void);

#endif

