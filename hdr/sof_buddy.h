#ifndef sofbuddyh
#define sofbuddyh
#include <windows.h>
extern void* o_sofplus;
extern bool sofplus_initialized;
void lifecycle_EarlyStartup(void);

DWORD WINAPI sofbuddy_thread(LPVOID lpParam);
#endif