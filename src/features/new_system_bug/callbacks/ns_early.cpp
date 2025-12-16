#include "feature_config.h"

#if FEATURE_NEW_SYSTEM_BUG

#include "util.h"
#include "sof_compat.h"
#include <windows.h>
#include "../shared.h"

void new_system_bug_EarlyStartup(void)
{
	orig_LoadLibraryA = (HMODULE (__stdcall *)(LPCSTR))*(unsigned int*)rvaToAbsExe((void*)0x00111178);
	orig_Cmd_ExecuteString = (void(*)(const char*))rvaToAbsExe((void*)0x194F0);
	SOFBUDDY_ASSERT(orig_LoadLibraryA != nullptr);
	SOFBUDDY_ASSERT(orig_Cmd_ExecuteString != nullptr);
	
	PrintOut(PRINT_LOG, "New System Bug Fix: Early startup - applying defaults\n");

	//Custom Detour
	//Vid_LoadRefresh - @call    ds:LoadLibraryA
	WriteE8Call(rvaToAbsExe((void*)0x00066E75), (void*)&new_sys_bug_LoadLibraryRef);
	WriteByte(rvaToAbsExe((void*)0x00066E7A), 0x90);
}

#endif // FEATURE_NEW_SYSTEM_BUG

