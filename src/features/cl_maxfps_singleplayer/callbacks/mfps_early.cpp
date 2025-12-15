#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include <windows.h>
#include "detours.h"
#include "util.h"
#include "sof_buddy.h"
#include "../shared.h"

void cl_maxfps_EarlyStartup(void)
{
    PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: Applying early patches...\n");
    
    //Nop the CL_Frame() function to prevent frame limiting in singleplayer
    WriteByte(rvaToAbsExe((void*)0x0000D973), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0000D974), 0x90);

    PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: sp_Sys_Mil hook registered automatically\n");
    if (o_sofplus && orig_sp_Sys_Mil == NULL) {
        void* spcl_Sys_Mil = rvaToAbsSoFPlus((void*)0xFA60);
        if (DetourSystem::Instance().ApplyDetourAtAddress(spcl_Sys_Mil, (void*)hksp_Sys_Mil, (void**)&orig_sp_Sys_Mil, "sp_Sys_Mil", 0)) {
            PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: Detour applied: sp_Sys_Mil -> %p\n", orig_sp_Sys_Mil);
        } else {
            PrintOut(PRINT_BAD, "cl_maxfps_singleplayer: Failed to detour sp_Sys_Mil at %p\n", spcl_Sys_Mil);
        }
    }
    
    PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: Early patches applied\n");
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER

