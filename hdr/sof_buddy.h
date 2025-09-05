#ifndef sofbuddyh
#define sofbuddyh
#include <windows.h>
extern void* o_sofplus;
void afterWsockInit(void);

DWORD WINAPI sofbuddy_thread(LPVOID lpParam);



#endif