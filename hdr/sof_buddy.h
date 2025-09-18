#ifndef sofbuddyh
#define sofbuddyh
#include <windows.h>
extern void* o_sofplus;
void afterWsockInit(void);

// Core lifecycle hooks (implemented in src/core.cpp)
void core_cvars_init(void);
void core_early(void);
void core_init(void); // legacy alias
void core_shutdown(void);

DWORD WINAPI sofbuddy_thread(LPVOID lpParam);



#endif