#include "feature_config.h"

#if FEATURE_CONSOLE_PROTECTION

#include "util.h"
#include "sof_compat.h"
#include <windows.h>
#include <string.h>
#include "../shared.h"

void console_protection_EarlyStartup(void)
{
	PrintOut(PRINT_LOG, "Console Protection: Applying security patches...\n");

	oSys_GetClipboardData = (char*(*)(void))rvaToAbsExe((void*)0x00065E60);
	
	WriteE8Call(rvaToAbsExe((void*)0x0004BB63), (void*)&hkSys_GetClipboardData);
	WriteE9Jmp(rvaToAbsExe((void*)0x0004BB6C), rvaToAbsExe((void*)0x0004BBFE));

	WriteE9Jmp(rvaToAbsExe((void*)0x0002111D), (void*)&my_Con_Draw_Console);
	WriteE9Jmp(rvaToAbsExe((void*)0x00020C90), rvaToAbsExe((void*)0x00020D6C));

	PrintOut(PRINT_LOG, "Console Protection: Security patches applied successfully.\n");
}

#endif // FEATURE_CONSOLE_PROTECTION

