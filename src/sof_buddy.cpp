#include "sof_buddy.h"

#include <iostream>

#include "sof_compat.h"
#include "features.h"

#include "./DetourXS/detourxs.h"

#include "util.h"

//__ioinit
void (*orig_FS_InitFilesystem)(void) = NULL;
void my_FS_InitFilesystem(void);
void my_orig_Qcommon_Init(int argc, char **argv);
qboolean my_Cbuf_AddLateCommands(void);

cvar_t *(*orig_Cvar_Get)(const char * name, const char * value, int flags, cvarcommand_t command) = 0x20021AE0;
cvar_t *(*orig_Cvar_Set2) (char *var_name, char *value, qboolean force) = 0x20021D70;
void (*orig_Cvar_SetInternal)(bool active) = 0x200216C0;
void ( *orig_Com_Printf)(char * msg, ...) = NULL;
void (*orig_Qcommon_Frame) (int msec) = 0x2001F720;

void (*orig_Qcommon_Init) (int argc, char **argv) = 0x2001F390;
qboolean (*orig_Cbuf_AddLateCommands)(void) = NULL;


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

#ifdef FEATURE_FONT_SCALING
	scaledFont_early();
#endif
	PrintOut(PRINT_LOG,"Before refFixes\n");
	refFixes_early();
	PrintOut(PRINT_LOG,"After refFixes\n");
	// We ensure that the sofplus init function is
	if ( o_sofplus ) {
		PrintOut(PRINT_LOG,"Before sofplusEntry\n");
		BOOL (*sofplusEntry)(void) = (int)o_sofplus + 0xF590;
		BOOL result = sofplusEntry();
		PrintOut(PRINT_LOG,"After sofplusEntry\n");
	}

	//orig_Qcommon_Init = DetourCreate(orig_Qcommon_Init,&my_orig_Qcommon_Init,DETOUR_TYPE_JMP,5);
	
	orig_FS_InitFilesystem = DetourCreate(0x20026980, &my_FS_InitFilesystem,DETOUR_TYPE_JMP,6);
	orig_Cbuf_AddLateCommands = DetourCreate(0x20018740,&my_Cbuf_AddLateCommands,DETOUR_TYPE_JMP,5);


	//Patch CL_Frame() to allow client frame limitting whilst in non dedicated server
	WriteByte(0x2000D973,0x90);WriteByte(0x2000D974,0x90);
	
	PrintOut(PRINT_GOOD,"SoF Buddy fully initialised!\n");
}

/*
	Fix bug on proton:
		Proton 3.7.8
		GloriousEggProton 
	if vid_card or cpu_memory_using become 'modified'
	causes a cascade of low performance cvars to kick in. (drivers/alldefs.cfg,geforce.cfg,cpu4.cfg,memory1.cfg)
	which have very bad values in them.

	they can become modified if new hardware values differ from config.cfg

	The exact moment that hardware-changed new setup files are exec'ed (cpu_ vid_card different to config.cfg)
*/
void InitDefaults(void)
{
	orig_Cmd_ExecuteString("exec drivers/highest.cfg\n");
	// fix 1024 high value of fx_maxdebrisonscreen, hurts cpu.
	orig_Cmd_ExecuteString("set fx_maxdebrisonscreen 128\n");
	// fix compression as default for some gpu.
	orig_Cmd_ExecuteString("set r_isf GL_SOLID_FORMAT\n");
	orig_Cmd_ExecuteString("set r_iaf GL_ALPHA_FORMAT\n");

	PrintOut(PRINT_GOOD,"Fixed defaults\n");
}

void my_FS_InitFilesystem(void) {
	orig_FS_InitFilesystem();

	//Allow PrintOut to use Com_Printf now.
	orig_Com_Printf = 0x2001C6E0;

	

	//Best (Earliest)(before exec config.cfg) place for cvar creation, if you want config.cfg induced triggering of modified events.
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
Safest(Earliest) Init Location for wanting to check command line values.

Cbuf_AddEarlyCommands
  processes all `+set` commands first.

Later in frame...
Cbuf_AddLateCommands
  all other commands that start with + are processed.
  You can mark the end of the command with `-`.

"public 1" must be set inside dedicated.cfg because dedicated.cfg overrides it? If no dedicated.cfg in user, the dedicated.cfg from pak0.pak is used.
*/
//this is called inside Qcommon_Init()
qboolean my_Cbuf_AddLateCommands(void)
{
	qboolean ret = orig_Cbuf_AddLateCommands();
	//user can disable modern tick from launch option. +set _sofbuddy_classic 1
#ifdef FEATURE_MEDIA_TIMERS
	create_sofbuddy_cvars();
	if ( _sofbuddy_classic_timers && _sofbuddy_classic_timers->value == 0.0f ) 
		{
			PrintOut(PRINT_GOOD,"Using QPC timers!\n");
			mediaTimers_apply();
		}
#endif
	
	/*
		CL_Init() already called, which handled VID_Init, and R_Init()
	*/
	return ret;
}

