#include <iostream>

#include "sof_buddy.h"
#include "features.h"
#include "sof_compat.h"
#include "./DetourXS/detourxs.h"


//__ioinit
void (*orig_FS_InitFilesystem)(void) = NULL;
void my_FS_InitFilesystem(void);
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

	Each library exports the full set of winsock functions.
	SoF.exe [Implicit]-> sof_buddy.dll [Explicit]->sof_plus.dll [Implicit]->native_winsock.dll

	Module Definition File (.def): A module definition file gives you fine-grained control over linking. 
	You can list the specific functions you want to import from a DLL:
	LIBRARY    MyProgram
	EXPORTS
	    MyFunction
	IMPORTS
	    WSOCK32.dll.bind
	    WSOCK32.dll.sendto
	    ; ... (other functions)

	Linker Options: Some linkers might have options to specify individual functions to import.
	__declspec(dllimport) is a storage-class specifier that tells the compiler that a function or object or data type is 
	defined in an external DLL.

	The function or object or data type is exported from a DLL with a corresponding __declspec(dllexport).

	spcl.dll selectively imports:
	  bind
	  sendto
	  recvfrom
	  WSAGetLastError
	  WSASetLastError
	  WSAStartup
	  inet_addr

	 I could had used implicit linking if I had known about it, so if I renamed the wsock.lib file it links to spcl.dll? weird?

	 Initialization Order: 
	   The operating system loader calls the DllMain functions of these DLLs in the order they are loaded.
	   However, this order is not always predictable and can vary depending on factors like the DLL's 
	   dependency graph and loading mechanisms.
*/
void afterWsockInit(void)
{
	/*
		This is called by our DllMain(), thus before SoF.exe CRTmain().
		Cvars etc not allowed here. 
	*/
#ifdef FEATURE_MEDIA_TIMERS
	//my_Sys_Milliseconds hook
	mediaTimers_early();
#endif
#ifdef FEATURE_FONT_SCALING
	scaledFont_early();
#endif
	refFixes_early();

	// We ensure that the sofplus init function is
	if ( o_sofplus ) {
		BOOL (*sofplusEntry)(void) = (int)o_sofplus + 0xF590;
		BOOL result = sofplusEntry();
	}

	//orig_Qcommon_Init = DetourCreate(orig_Qcommon_Init,&my_orig_Qcommon_Init,DETOUR_TYPE_JMP,5);
	orig_FS_InitFilesystem = DetourCreate(0x20026980, &my_FS_InitFilesystem,DETOUR_TYPE_JMP,6);
	orig_Cbuf_AddLateCommands = DetourCreate(0x20018740,&my_Cbuf_AddLateCommands,DETOUR_TYPE_JMP,5);
	
}

//Every cvar here would trigger its modified, because no cvars exist prior.
//But its loaded with its default value.
/*
	This is earlier than Cbuf_AddLateCommands(), just before exec default.cfg and exec config.cfg IN Qcommon_Init()

	Cvar flags are or'ed in, thus multiple calls to Cvar_Get stack the flags.
	Does the command with NULL, remove the command or create multiple?
	  NULL does not remove previous set modified callbacks.
	  With a new modified callback, it overrides the currently set one.
*/
void my_FS_InitFilesystem(void) {
	orig_FS_InitFilesystem();

	//Best place for cvars if you want config.cfg induced triggering of modified events.
	refFixes_apply();
	#ifdef FEATURE_FONT_SCALING
		scaledFont_apply();
	#endif

	//orig_Com_Printf("End FS_InitFilesystem\n");
}
/*
	A long standing bug, was related to the order of initializing cvars, which behaved different for a user without
	a config.cfg than one with one. If getting crashes, always remember this!.
*/
void my_orig_Qcommon_Init(int argc, char **argv)
{
	orig_Qcommon_Init(argc,argv);
}


/*
Safest Init Location for wanting to check command line values.
*/
//this is called inside Qcommon_Init()
qboolean my_Cbuf_AddLateCommands(void)
{
	qboolean ret = orig_Cbuf_AddLateCommands();
#ifdef FEATURE_MEDIA_TIMERS
	mediaTimers_apply();
#endif

	return ret;
}

cvar_t *(*orig_Cvar_Get)(const char * name, const char * value, int flags, cvarcommand_t command) = 0x20021AE0;
void ( *orig_Com_Printf)(char * msg, ...) = 0x2001C6E0;
void (*orig_Qcommon_Frame) (int msec) = 0x2001F720;

void (*orig_Qcommon_Init) (int argc, char **argv) = 0x2001F390;
qboolean (*orig_Cbuf_AddLateCommands)(void) = NULL;