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
	
    // Keep Sys_SendKeyEvents on stock timeGetTime. sys_msg_time (msg.time) and
    // sys_frame_time must share one clock because CL_KeyState subtracts downtime
    // (from Key_Event's msg.time argument) from sys_frame_time.
	
	if (o_sofplus) {
		PrintOut(PRINT_LOG, "Media timers: Initializing SoFPlus integration...\n");
		
		sp_Sys_Mil = (int(*)(void))((char*)o_sofplus + 0xFA60);

		// SoFPlus's Sys_Millisec_Hook calls the original Sys_Milliseconds directly.
		// Redirect that call to our QPC clock so curtime has one writer/timebase.
		// Hook prologue: 83 EC 0C  B8 <imm32>  (sub esp,0Ch; mov eax, imm32)
		unsigned char* sp_hook = (unsigned char*)o_sofplus + 0xFA60;
		if (sp_hook[3] == 0xB8) {
			writeUnsignedIntegerAt(sp_hook + 4,
				(unsigned int)(uintptr_t)(void*)&my_Sys_Milliseconds);
		} else {
			PrintOut(PRINT_BAD, "Media timers: unexpected SoFPlus Sys_Millisec_Hook prologue\n");
		}
		
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
