//for _controlfp
#include <float.h>
#include <iostream>

#include "sof_compat.h"
#include "sof_buddy.h"

#include "util.h"
#include "../DetourXS/detourxs.h"

void high_priority_change(cvar_t * cvar);
cvar_t * _sofbuddy_high_priority = NULL;
cvar_t * _sofbuddy_classic = NULL;
cvar_t * test = NULL;

int (*Sys_Mil) (void) = NULL;
int (*Sys_Mil_AfterHookThunk) (void) = NULL;

int winmain_loop(void);
int my_Sys_Milliseconds(void);

int oldtime = 0;

void resetTimers(int val)
{

	if ( o_sofplus ) {
		*(int*)((void*)o_sofplus+0x331F0) = val;
		*(int*)((void*)o_sofplus+0x331F4) = val;
		*(int*)((void*)o_sofplus+0x33220) = val;
	}
}
void mediaTimers_early(void)
{
	//Detour sys_milliseconds for any other code that calls sys_milliseconds uses media timer instead of timegettime.
	//Calling sofplus or original function address , calls ours now.
	Sys_Mil_AfterHookThunk = DetourCreate(0x20055930,&my_Sys_Milliseconds,DETOUR_TYPE_JMP,5);
}
void mediaTimers_apply(void)
{
	//user can disable modern tick from launch option. +set _sofbuddy_classic 1
	_sofbuddy_classic = orig_Cvar_Get("_sofbuddy_classic","0",CVAR_NOSET,NULL);

	if ( _sofbuddy_classic->value ) return;
	#if 0
	HANDLE processHandle = GetCurrentProcess();

	// Specify the core you want to set affinity to (e.g., core 0)
	DWORD_PTR processAffinityMask = 1 << 0;

	// Set the affinity mask for the current process
	BOOL success = SetProcessAffinityMask(processHandle, processAffinityMask);
	if (success) {
	   std::cout << "Affinity set successfully." << std::endl;
	} else {
		MessageBox(NULL, "Failed to set affinity", "MessageBox Example", MB_OK);
	}
	#endif
	//std::cout << "mediaTimers_apply";
	//MessageBox(NULL, "mediaTimers_apply", "MessageBox Example", MB_OK);
	if ( o_sofplus ) {
		resetTimers(0);
		Sys_Mil = (void*)o_sofplus+0xFA60; //sofplus sys_mil
	}
	else
		Sys_Mil = 0x20055930; //original


	//CVAR_ARCHIVE
	//CVAR_NOSET
	//Cvars
	_sofbuddy_high_priority = orig_Cvar_Get("_sofbuddy_high_priority","0",CVAR_ARCHIVE,&high_priority_change);
	
	//test = orig_Cvar_Get("test","7",NULL,NULL);

	//loop Sys_Milliseconds() within WinMain() to call our sys_millisecond instead.
	//reconstruct the loop.
	WriteE8Call(0x20066412,&winmain_loop);

	//Attempt to skip the default code using jmp.
	//So that we handle the flow.
	WriteE9Jmp(0x20066417,0x2006643C);


	//use Sys_Milliseconds() instead of timeGetTime() for: 
	//	sys_win.c
	//		Sys_SendKeyEvent()
	//			sys_frame_time = timeGetTime();
	//Wasn't originally an 0xE8 call, was 0xFF 0x15. Thus extra byte nopped.
	WriteE8Call(0x20065D5E,&my_Sys_Milliseconds); // 5e 5f 60 61 62 63
	WriteByte(0x20065D63,0x90);
}

void high_priority_change(cvar_t * cvar) {
	if ( cvar->value ) {
		HANDLE hProcess = GetCurrentProcess();
		// Set the priority class to HIGH_PRIORITY_CLASS
		BOOL success = SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS);
	} else {
		HANDLE hProcess = GetCurrentProcess();
		// Set the priority class to HIGH_PRIORITY_CLASS
		BOOL success = SetPriorityClass(hProcess, NORMAL_PRIORITY_CLASS);
	}

}

void
Sys_Nanosleep(int nanosec)
{
	HANDLE timer;
	LARGE_INTEGER li;

	//100,000,000 = 10 sec
	//10,000,000 = 1 sec
	//10,000 = 1ms
	//10 = 1us
	//1 = 100 ns
	timer = CreateWaitableTimer(NULL, TRUE, NULL);

	// Windows has a max. resolution of 100ns.
	//Convert nanoseconds to 100ns unit.
	li.QuadPart = -nanosec / 100;

	SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE);

	WaitForSingleObject(timer, INFINITE);

	CloseHandle(timer);

}

/*
New issue, when changing vid_ref, this function is is still in call stack.

Not worth sleeping. Windows is too bad.
*/

int winmain_loop(void)
{
	//orig_Com_Printf("winmain_loop\n");
	int newtime, time;
	do
	{
		//If spcl.dll loaded, this should call spcl.dll version.
		newtime = Sys_Mil();
		
		time = newtime - oldtime;

		//There might be issues still with sofplus oldtimers when changing this cvar.
		//Might need to update them.

		//newtime has to be > oldtime for this to break
	}
	while (time < 1);

	//orig_Com_Printf("loop\n");
	oldtime = newtime;

	_controlfp( _PC_24, _MCW_PC );
	orig_Qcommon_Frame (time);

	return newtime;
}

/*
	
The difference between an older and a newer value of the QueryPerformanceCounter can be negative.
(This can happen when the thread moves from one CPU core to an other.)

*/
int my_Sys_Milliseconds(void)
{
	if ( _sofbuddy_classic && _sofbuddy_classic->value ) return Sys_Mil_AfterHookThunk();
	//orig_Com_Printf("mediaTimersWait\n");
    static LARGE_INTEGER freq = { 0 };
    static LARGE_INTEGER base = { 0 };

    //freq = 10,000,000, so resolution = 1/10,000,000 = 0.0000001 = 0.1 microseconds = 100 nanoseconds.
    //1000 ns = 1us
    //1000 us = 1ms
    //1000 ms = 1s
    //_sp_cl_cpu_cool 1 = 0.1 ms = 100 us
    //DO ONCE.
    if (!freq.QuadPart)
    {
        if (!QueryPerformanceFrequency(&freq))
        {
            orig_Com_Printf("QueryPerformanceFrequency failed\n");
        	ExitProcess(1);
        }
    }

    /*
	base : 353890310400
	base2 : 1702992128
    */
    //DO ONCE.
    if (!base.QuadPart)
    {
        if (!QueryPerformanceCounter(&base))
        {
        	orig_Com_Printf("QueryPerformanceCounter failed\n");
        	ExitProcess(1);
        }
    }

    /*
    cur : 353890311403
	cur2 : 1702993131
    */
    LARGE_INTEGER cur;
    QueryPerformanceCounter(&cur);

    //(cur - base) * 1000/freq

    long long diff = cur.QuadPart - base.QuadPart;

    bool wasNeg = false;
    //Handle bug when thread switches core or bios/driver bug.
   	while ( diff < 0 ) {
	    if (!QueryPerformanceFrequency(&freq))
	    {
	        orig_Com_Printf("QueryPerformanceFrequency failed\n");
	    	ExitProcess(1);
	    }
   		//obtain a new base.
   		if (!QueryPerformanceCounter(&base))
   		{
   			orig_Com_Printf("QueryPerformanceCounter failed\n");
   			ExitProcess(1);
   		}

   		LARGE_INTEGER cur;
    	QueryPerformanceCounter(&cur);

    	diff = cur.QuadPart - base.QuadPart;
    	wasNeg = true;
   	}

    //100ns tick * 1,000,000 = ns->us->ms 100ms
    // diff/freq = seconds. seconds * 1,000,000 = microseconds.
    int ret = diff * 1000 / freq.QuadPart;

   	while ( ret < oldtime ) {
	    if (!QueryPerformanceFrequency(&freq))
	    {
	        orig_Com_Printf("QueryPerformanceFrequency failed\n");
	    	ExitProcess(1);
	    }
   		//obtain a new base.
   		if (!QueryPerformanceCounter(&base))
   		{
   			orig_Com_Printf("QueryPerformanceCounter failed\n");
   			ExitProcess(1);
   		}

   		LARGE_INTEGER cur;
    	QueryPerformanceCounter(&cur);

    	diff = cur.QuadPart - base.QuadPart;
    	ret = diff * 1000 / freq.QuadPart;
    	wasNeg = true;
   	}
   	#if 0
    if ( test != NULL ) {
    	if ( ret-oldtime > test->value ) {
    		orig_Com_Printf("mediaTimer: %i\n",ret-oldtime);
    	}
    }
    #endif
    
    if ( wasNeg ) {
    	//MessageBox(NULL, "Timer out of sync", "Error", MB_ICONERROR | MB_OK);
    	//ret has recalibrated so oldtime has to change.
    	oldtime = ret;
    	// update oldtime.
    	resetTimers(ret);
    }

    //set curtime
    *(int*)0x20390D38 = ret;

    return ret;
}