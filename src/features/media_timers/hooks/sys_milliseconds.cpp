#include "feature_config.h"

#if FEATURE_MEDIA_TIMERS

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../shared.h"
#include <windows.h>

using detour_Sys_Milliseconds::tSys_Milliseconds;

extern LARGE_INTEGER base;
extern LARGE_INTEGER freq;
extern int oldtime;

/*
freq = 10,000,000, so resolution = 1/10,000,000 = 0.0000001 = 0.1 microseconds = 100 nanoseconds.
1000 ns = 1us
1000 us = 1ms
1000 ms = 1s
_sp_cl_cpu_cool 1 = 0.1 ms = 100 us
*/
long long qpc_timers(bool force)
{
	LARGE_INTEGER cur = {0};

	if (!freq.QuadPart || force)
    {
		if (!QueryPerformanceFrequency(&freq))
		{
			printf("QueryPerformanceFrequency failed\n");
			PrintOut(PRINT_BAD, "QueryPerformanceFrequency failed\n");
			ExitProcess(1);
		}
	}
	//obtain a new base.
	if (!base.QuadPart || force)
	{
		if (!QueryPerformanceCounter(&base))
		{
			printf("QueryPerformanceCounter failed\n");
			PrintOut(PRINT_BAD, "QueryPerformanceCounter failed\n");
			ExitProcess(1);
		}
	}
	
	QueryPerformanceCounter(&cur);
	return (long long)(cur.QuadPart - base.QuadPart);
}

/*
	
The difference between an older and a newer value of the QueryPerformanceCounter can be negative.
(This can happen when the thread moves from one CPU core to an other.)

Consider explicitly binding the thread to a specific processor using SetThreadAffinityMask to avoid counter discrepancies caused by core switching.
*/
// SofPlus -> Us -> orig

/*
	This is actually called in Sys_Init() inside QCommon_Init()
	By some cpu_analyse type code.

	Next its called by: Netchan_Init()

	Its also called every frame inside CL_Frame() , (not in q2). 
	Seems to have no purpose too. (Ah unless its to set curtime (would make sense)).

	Cbuf_Execute() in CL_Frame() is meant to handle the sp_sc_timer calbacks.
	Cbuf_AddText() called by the spTimers() function.

	analyze_cpu() - 20021183 , 200211a2
	Netchan_Init() - 2004d256
	CL_InitLocal() - 2000cad7 (cls.realtime = Sys_Milliseconds())
	WinMain() - 20066373 (oldtime = Sys_Milliseconds ();)
	_US_
	M_PushMenu() - 200cb7fe, 200c7823 200c78a6 (main menu when game starts)
	M_PushMenu() innerFunc() - 200e6171
	IN_MenuMove() - 200cc079
	CL_Frame() - 2000d91a
	Sys_SendKeyEvents() - 20065d63
	_US_
	CL_Frame()
	Sys_SendKeyEvents()
	IN_MenuMove()
	_US_
	...

	CL_ReadPackets()
		CL_ParseServerMessage()
			CL_ParseServerData() is what prints' the mapname.

	(Net_Init())
	WSA_Startup() is initialising the sofplus with the addon inits.

	Because the cpu_analyze() calls to Sys_Milliseconds() eventually result in
	curtime = 0.
	The times which it got saved at no longer make any sense after it resets.

	We have to find out what causes it to reset to 0.

*/
int my_Sys_Milliseconds(void)
{
	#if 0
	void *return_address = __builtin_return_address(0);
    printf("my_Sys_Milliseconds called by : %p\n", return_address);
	#endif

	static int prev_val = 0;

    long long ticks_elapsed = qpc_timers(false);

	bool wasNeg = false;
    //Handle bug when thread switches core or bios/driver bug.
	while ( ticks_elapsed < 0 ) {
   		ticks_elapsed = qpc_timers(true);
		wasNeg = true;
	}

    //100ns tick * 1,000,000 = ns->us->ms 100ms
    // ticks_elapsed/freq = seconds. seconds * 1,000,000 = microseconds.
	if (!freq.QuadPart) {
		ticks_elapsed = qpc_timers(true);
	}
	if (!freq.QuadPart) {
		return 0;
	}
	int ret = (int)((ticks_elapsed * 1000LL) / freq.QuadPart);

	if ( ret < prev_val ) {
		wasNeg = true;
	}

	/*
		Not sure how often this occurs
	*/
	if ( wasNeg ) {
		// Have to adjust the timers sadly.
		// Or change the location of 'exec sofplus.cfg' slightly

		//TODO:
    	//This doesn't fix the timers, because the actual timestamp
    	//of each timer has to be changed. Its a simple iteration.
    	//but i won't do it unless I have to. You'd also want to update
    	//the lastTimerTick variable inside of the spTimer() function.
		oldtime = ret;
		resetTimers(ret);
	}

    //set curtime
	*(int*)0x20390D38 = ret;

	prev_val = ret;

	return ret;
}

//an override hook because we do not call original
int sys_milliseconds_override_callback(detour_Sys_Milliseconds::tSys_Milliseconds original) {
	int result = my_Sys_Milliseconds();
	return result;
}

#endif // FEATURE_MEDIA_TIMERS

