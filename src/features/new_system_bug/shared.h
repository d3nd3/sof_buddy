#pragma once

#include "feature_config.h"

#if FEATURE_NEW_SYSTEM_BUG

#include <windows.h>

extern HMODULE (__stdcall *orig_LoadLibraryA)(LPCSTR lpLibFileName);
extern void (*orig_Cmd_ExecuteString)(const char*);

HMODULE __stdcall new_sys_bug_LoadLibraryRef(LPCSTR lpLibFileName);
void new_system_bug_EarlyStartup(void);

#endif // FEATURE_NEW_SYSTEM_BUG


