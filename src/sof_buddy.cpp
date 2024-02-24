#include <iostream>

#include "sof_buddy.h"
#include "features.h"
#include "sof_compat.h"
#include "./DetourXS/detourxs.h"

void my_orig_Qcommon_Init(int argc, char **argv);
qboolean my_Cbuf_AddLateCommands(void);


DWORD WINAPI sofbuddy_thread(LPVOID lpParam) {
	Sleep(100);
    //afterSoFplusInit();

	Sleep(100);
    Sleep(1500);
    orig_Com_Printf("SoFree thread is running!");
    return 1;
}


void afterWsockInit(void)
{
	mediaTimers_early();

	if ( o_sofplus ) {
		BOOL (*sofplusEntry)(void) = (int)o_sofplus + 0xF590;
		BOOL result = sofplusEntry();
	}

	//orig_Qcommon_Init = DetourCreate(orig_Qcommon_Init,&my_orig_Qcommon_Init,DETOUR_TYPE_JMP,5);
	orig_Cbuf_AddLateCommands = DetourCreate(orig_Cbuf_AddLateCommands,&my_Cbuf_AddLateCommands,DETOUR_TYPE_JMP,5);
	
}

void my_orig_Qcommon_Init(int argc, char **argv)
{
	orig_Qcommon_Init(argc,argv);
}


/*
Safest Init Location
*/
//this is called inside Qcommon_Init()
qboolean my_Cbuf_AddLateCommands(void)
{
	qboolean ret = orig_Cbuf_AddLateCommands();
	
	mediaTimers_apply();
	refFixes_apply();

	return ret;
}

cvar_t *(*orig_Cvar_Get)(const char * name, const char * value, int flags, cvarcommand_t command) = 0x20021AE0;
void ( *orig_Com_Printf)(char * msg, ...) = 0x2001C6E0;
void (*orig_Qcommon_Frame) (int msec) = 0x2001F720;

void (*orig_Qcommon_Init) (int argc, char **argv) = 0x2001F390;
qboolean (*orig_Cbuf_AddLateCommands)(void) = 0x20018740;