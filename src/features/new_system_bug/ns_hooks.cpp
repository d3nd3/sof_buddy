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
	SOFBUDDY_ASSERT(detour_Cmd_ExecuteString::oCmd_ExecuteString != nullptr);
	
	PrintOut(PRINT_LOG, "New System Bug Fix: Applying optimal defaults...\n");

	// R_Init is still building its command buffer here. Append the profile and
	// overrides together so exec inserts highest.cfg before the safe values.
	using ref_cmd_execute_text_t = void (__cdecl *)(int, char*);
	ref_cmd_execute_text_t* cmd_execute_text =
		reinterpret_cast<ref_cmd_execute_text_t*>(rvaToAbsRef((void*)0x0008FDE4));
	if (cmd_execute_text && *cmd_execute_text) {
		char commands[] =
			"exec drivers/highest.cfg\n"
			"set fx_maxdebrisonscreen 128\n"
			"set r_isf GL_SOLID_FORMAT\n"
			"set r_iaf GL_ALPHA_FORMAT\n";
		(*cmd_execute_text)(2, commands); // EXEC_APPEND
	} else {
		PrintOut(PRINT_BAD, "New System Bug Fix: ref Cmd_ExecuteText unavailable\n");
	}

	PrintOut(PRINT_DEV, "New System Bug Fix: Optimal defaults applied\n");
}

/*
	Direct LoadLibrary replacement.
	
	RefInMemory is a LoadLibrary() in-place Detour, for ref_gl.dll

	Allows to modify ref_gl.dll at before R_Init() returns.
 */
HMODULE __stdcall new_sys_bug_LoadLibraryRef(LPCSTR lpLibFileName)
{
	SOFBUDDY_ASSERT(lpLibFileName != nullptr);
	SOFBUDDY_ASSERT(orig_LoadLibraryA != nullptr);
	
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