#include <iostream>

#include "sof_buddy.h"
#include "features.h"
#include "sof_compat.h"
#include "./DetourXS/detourxs.h"

void my_orig_Qcommon_Init(int argc, char **argv);
qboolean my_Cbuf_AddLateCommands(void);

/*
	DllMain of WSOCK32 library
	Implicit Linking (Static Linking to the Import Library)
	the application is linked with an import library (.lib) file during the build process.
	This import library contains information about the functions and data exported by the .dll.
	When the application starts, the Windows loader automatically loads the .dll specified by the import library.
	If the .dll is not found at runtime, the application will fail to start.

	Before the application’s main entry point (e.g., main() or WinMain()) is called, 
	the loader inspects the import table of the executable to determine the required .dll files
	The loader attempts to load each .dll specified in the import table. This includes resolving the addresses 
	of the functions and variables that the application imports from the .dlls.

	Once a .dll is loaded, the loader calls the DllMain function of the .dll (if it exists) with the 
	DLL_PROCESS_ATTACH notification. This allows the .dll to perform any necessary initialization.
*/
void afterWsockInit(void)
{
#ifdef FEATURE_MEDIA_TIMERS
	mediaTimers_early();
#endif
#ifdef FEATURE_FONT_SCALING
	scaledFont_early();
#endif
	refFixes_early();
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
#ifdef FEATURE_MEDIA_TIMERS
	mediaTimers_apply();
#endif
	refFixes_apply();
#ifdef FEATURE_FONT_SCALING
	scaledFont_apply();
#endif
	return ret;
}

cvar_t *(*orig_Cvar_Get)(const char * name, const char * value, int flags, cvarcommand_t command) = 0x20021AE0;
void ( *orig_Com_Printf)(char * msg, ...) = 0x2001C6E0;
void (*orig_Qcommon_Frame) (int msec) = 0x2001F720;

void (*orig_Qcommon_Init) (int argc, char **argv) = 0x2001F390;
qboolean (*orig_Cbuf_AddLateCommands)(void) = 0x20018740;