/*
	New System Detection default bug
	
	Fix bug on proton (Proton 3.7.8, GloriousEggProton):
	- If vid_card or cpu_memory_using become 'modified'
	- Causes a cascade of low performance cvars to kick in
	- Files: drivers/alldefs.cfg, geforce.cfg, cpu4.cfg, memory1.cfg
	- These files have very bad performance values in them
	
	They can become modified if new hardware values differ from config.cfg
	The exact moment that hardware-changed new setup files are exec'ed
	(cpu_ vid_card different to config.cfg)
	
	This feature overrides those bad defaults with optimal settings.
*/

#include "feature_config.h"

#if FEATURE_NEW_SYSTEM_BUG
#include "util.h"
#include "sof_compat.h"
#include <windows.h>
#include "shared.h"

// Forward declarations
static void new_system_bug_InitDefaults(void);
HMODULE __stdcall new_sys_bug_LoadLibraryRef(LPCSTR lpLibFileName);

// Function pointer initialization
HMODULE (__stdcall *orig_LoadLibraryA)(LPCSTR lpLibFileName) = nullptr;



/*
	RefDllLoaded lifecycle callback
	
	This is called after LoadLibrary("ref_gl.dll")
	Apply optimal default settings to override bad hardware detection values
*/
static void new_system_bug_InitDefaults(void)
{
	PrintOut(PRINT_LOG, "New System Bug Fix: Applying optimal defaults...\n");
	
	// Override with highest quality settings
	orig_Cmd_ExecuteString("exec drivers/highest.cfg\n");
	
	// Fix 1024 high value of fx_maxdebrisonscreen, hurts CPU performance
	orig_Cmd_ExecuteString("set fx_maxdebrisonscreen 128\n");
	
	// Fix compression as default for some GPUs
	orig_Cmd_ExecuteString("set r_isf GL_SOLID_FORMAT\n");
	orig_Cmd_ExecuteString("set r_iaf GL_ALPHA_FORMAT\n");

	PrintOut(PRINT_GOOD, "New System Bug Fix: Optimal defaults applied\n");
}

/*
	Direct LoadLibrary replacement.
	
	RefInMemory is a LoadLibrary() in-place Detour, for ref_gl.dll

	Allows to modify ref_gl.dll at before R_Init() returns.
 */
HMODULE __stdcall new_sys_bug_LoadLibraryRef(LPCSTR lpLibFileName)
{
	HMODULE ret = orig_LoadLibraryA(lpLibFileName);
	if (ret) {
		/*
			Get the exact moment that hardware-changed new setup files are exec'ed (cpu_ vid_card different to config.cfg)
		*/
		WriteE8Call(rvaToAbsRef((void*)0x0000FA26), (void*)&new_system_bug_InitDefaults);
		WriteByte(rvaToAbsRef((void*)0x0000FA2B), 0x90);	
	}
	
	return ret;
}

#endif // FEATURE_NEW_SYSTEM_BUG