#include "feature_config.h"

#if FEATURE_MEDIA_TIMERS

#include "sof_buddy.h"
#include "util.h"
#include "sof_compat.h"
#include "../shared.h"

/*
	Media timers EarlyStartup lifecycle callback
	
	This runs during the early startup phase and handles basic initialization
	that doesn't require CVars or the filesystem to be available.
	
	The detour system automatically registers and applies the Sys_Milliseconds detour
	via the static AutoDetour object created when we include "generated_detours.h".
	The detour system now safely handles static initialization by deferring registrations
	until ProcessDeferredRegistrations() is called in lifecycle_EarlyStartup().
*/
void mediaTimers_EarlyStartup(void) {
	// Ensure Sleep(1) has 1ms granularity to avoid oversleep hitches.
	mediaTimers_Request1msTimerPeriod(sleep_mode);

    //overwrite 'call    Sys_Milliseconds' -> 'call winmain_loop'
	WriteE8Call(rvaToAbsExe((void*)0x00066412), (void*)&winmain_loop);
    //skip the rest of the loop, so we implement most ourself
	WriteE9Jmp(rvaToAbsExe((void*)0x00066417), rvaToAbsExe((void*)0x0006643C));
	
    //Sys_sendkeyevents @call    Sys_Milliseconds instead of TimeGetTime
	WriteE8Call(rvaToAbsExe((void*)0x00065D5E), (void*)&my_TimeGetTime);
	WriteByte(rvaToAbsExe((void*)0x00065D63), 0x90);

	
	if (o_sofplus) {
		PrintOut(PRINT_LOG, "Media timers: Initializing SoFPlus integration...\n");
		
		sp_Sys_Mil = (int(*)(void))((char*)o_sofplus + 0xFA60);
		
		spcl_FreeScript = (void(*)(void))((char*)o_sofplus + 0x9D20);
		spcl_Timers = (void(*)(void))((char*)o_sofplus + 0x10190);
		
		sp_whileLoopCount = (int*)((char*)o_sofplus + 0x33188);
		*sp_whileLoopCount = 0;
		
		sp_lastFullClientFrame = (int*)((char*)o_sofplus + 0x331F0);
		*sp_lastFullClientFrame = 0;
		
		sp_current_timestamp2 = (int*)((char*)o_sofplus + 0x33220);
		*sp_current_timestamp2 = 0;
		
		sp_current_timestamp = (int*)((char*)o_sofplus + 0x331F4);
		*sp_current_timestamp = 0;
		
	}
	
	PrintOut(PRINT_LOG, "Media timers: Early startup complete\n");
}

#endif // FEATURE_MEDIA_TIMERS
