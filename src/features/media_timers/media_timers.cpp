#include "sof_buddy.h"

//for _controlfp
#include "customfloat.h"

/* <iostream> intentionally unused; keep removed to silence linter */

#include "sof_compat.h"
#include "features.h"


#include "util.h"
#include "core_hooks.h"
#include "features/media_timers/hooks.h"


#include "../../DetourXS/detourxs.h"
#define DETOUR_TRACKER_AUTOWRAP
#include "detour_tracker.h"

#include <cmath>  // Include cmath for std::ceil

#include <cassert>


/*
	Feature: Media timers (QPC-based) and main loop pacing

	What it does:
	- Replaces legacy timeGetTime()-based timing with QueryPerformanceCounter for smoother frame pacing.
	- Implements a custom WinMain loop pacing (sleep + busyspin) respecting cl_maxfps and SP frame cadence.
	- Integrates with SoFPlus timers when present, updating its timestamp variables to keep both in sync.

	Init/race notes (moved from sof_buddy.cpp):
	- SoFPlus is initialised in NET_Init() (WSAStartup) during Qcommon_Init(), and its addons run via Cbuf_AddText.
	- Our early timer hook runs before addon code, but the first sp_sc_timer executes later during Cbuf_Execute in the frame.
	- To avoid timing resets (e.g. curtime reset to 0) we hook Sys_Milliseconds very early and then patch the main loop.

	Detours and memory edits used by this feature:
	- mediaTimers_early():
	  * DetourCreate(0x20055930 -> my_Sys_Milliseconds) // early Sys_Milliseconds hook
	- mediaTimers_apply_afterCmdline():
	  * WriteE8Call(0x20066412 -> winmain_loop)         // patch WinMain loop entry
	  * WriteE9Jmp(0x20066417 -> 0x2006643C)            // skip original loop body, resume after
	  * WriteE8Call(0x20065D5E -> my_Sys_Milliseconds); WriteByte(0x20065D63, 0x90) // Sys_SendKeyEvents uses our clock
	  * SoFPlus integration: sets sp_Sys_Mil, sp_* timestamp pointers and resets (*sp_current_timestamp etc.)
*/
void high_priority_change(cvar_t * cvar);
void sleep_change(cvar_t * cvar);
bool sleep_mode = true;
int extratime_resume = 0;

void sleep_jitter_change(cvar_t *cvar);
bool sleep_jitter = false; 

//how many msec ticks to busyloop for each frame
void sleep_busyticks_change(cvar_t * cvar);
int sleep_busyticks = 2;


void cl_maxfps_change(cvar_t *cvar);


cvar_t * cl_maxfps = NULL;

int (*sp_Sys_Mil) (void) = NULL;
int (*Sys_Mil) (void) = NULL;
int (*Sys_Mil_AfterHookThunk) (void) = NULL;

int winmain_loop(void);
int my_Sys_Milliseconds(void);

LARGE_INTEGER base = {0};
LARGE_INTEGER freq = {0};

int qpcTimeStampNano(void);

int oldtime = 0;
int frametime = 100;
float previous_cl_maxfps = 30.0f;

int *sp_whileLoopCount = NULL;
int *sp_lastFullClientFrame = NULL;
int *sp_current_timestamp = NULL;
int *sp_current_timestamp2 = NULL;


/*
	If we modify oldtime, these variables might already had got a value.
	So we set them to 0.
	_sp_cl_frame_delay:
		Minimum delay between frames (in milliseconds) (default = 7)
	== max fps limitter.
	if the length of current frame is less than 7, wait longer.
*/
inline void resetTimers(int val)
{
	*sp_current_timestamp = val;
	*sp_current_timestamp2 = val;
	*sp_lastFullClientFrame = val;
}

void (*spcl_Timers)(void) = NULL;
void (*spcl_FreeScript)(void) = NULL;

/*
	This is DllMain direct.
	This is early.
	I am wondering if the curtime used inside WSA_Startup() is not using our timers.
	But it should not be possible.
*/
void mediaTimers_early(void)
{
	#if 0
	int (*orig_sys_mil) (void) = 0x20055930;
	//sofplus only hooks into the sys_milli called by WinMain
	int before_curtime = orig_sys_mil();
	#endif
    Sys_Mil_AfterHookThunk = reinterpret_cast<int(*)(void)>(DetourCreateRF(reinterpret_cast<void*>(0x20055930),(void*)&core_Sys_Milliseconds,DETOUR_TYPE_JMP,5,"Media Timers","Media Timers"));

	#if 0
	//Correct the timer value used in sofplus timers.
	if (o_sofplus) {
		int * lastSofplusTimerTick = o_sofplus+0x52630;
		int deltaTime = before_curtime - *lastSofplusTimerTick;
		int new_curtime = my_Sys_Milliseconds();
		if ( deltaTime > 0 ) {
			*lastSofplusTimerTick = new_curtime - deltaTime;
		} else {
			*lastSofplusTimerTick = new_curtime - 1;
		}
	}
	#endif
}
#ifdef FEATURE_MEDIA_TIMERS

void mediaTimers_apply_afterCmdline(void)
{
	//CVAR_ARCHIVE - save to config.cfg
	//CVAR_NOSET - write-only.
	//===Cvars====
    cl_maxfps = orig_Cvar_Get("cl_maxfps","30",0,&cl_maxfps_change);
	
	create_mediatimers_cvars();


	//===Memory Edits===

	//Detour sys_milliseconds for any other code that calls sys_milliseconds uses media timer instead of timegettime.
	//gameloop calls -> sofplus_sys_milli -> our_sys_milli -> orig_sys_milli
	//important we call this super early to catch any bad curtime being set by sys_millisecond timegettime implem.
	// mediaTimers_early();


	// main() loop reimplemented.

	//Remember that this is where ctrl hooks the Sys_milliseconds()
	//Patch call to sys_millisecond -

    WriteE8Call(reinterpret_cast<void*>(0x20066412), reinterpret_cast<void*>(&winmain_loop));

	//continue the outer loop once sys_millisecond returns, nothing else.
    WriteE9Jmp(reinterpret_cast<void*>(0x20066417),reinterpret_cast<void*>(0x2006643C));


	//use Sys_Milliseconds() instead of timeGetTime() for: 
	//	sys_win.c
	//		Sys_SendKeyEvent()
	//			sys_frame_time = timeGetTime();
	//Wasn't originally an 0xE8 call, was 0xFF 0x15. Thus extra byte nopped.
    WriteE8Call(reinterpret_cast<void*>(0x20065D5E), reinterpret_cast<void*>(&my_Sys_Milliseconds)); // 5e 5f 60 61 62 63
    WriteByte(reinterpret_cast<void*>(0x20065D63),0x90);


	//==SoF Plus integration==
	if ( o_sofplus ) {
		
        spcl_FreeScript = reinterpret_cast<void(*)(void)>(reinterpret_cast<char*>(o_sofplus) + 0x9D20);
        spcl_Timers = reinterpret_cast<void(*)(void)>(reinterpret_cast<char*>(o_sofplus) + 0x10190);

        
        sp_Sys_Mil = reinterpret_cast<int(*)(void)>(reinterpret_cast<char*>(o_sofplus) + 0xFA60); //sofplus sys_mil
		Sys_Mil = &my_Sys_Milliseconds;

        sp_whileLoopCount = reinterpret_cast<int*>(reinterpret_cast<char*>(o_sofplus) + 0x33188);
        *sp_whileLoopCount = 0;

		//If framecount increased (CL_Frame ran) sets 

		// sp_lastFullClientFrame = sp_current_timestamp
		// inside CL_Frame Hook.
        sp_lastFullClientFrame = reinterpret_cast<int*>(reinterpret_cast<char*>(o_sofplus) + 0x331F0);
        *sp_lastFullClientFrame = 0;

		//Both set by Sys_Milli inside Main()

		//This one only used by cmp to call timers within the sys_mill hook
        sp_current_timestamp2 = reinterpret_cast<int*>(reinterpret_cast<char*>(o_sofplus) + 0x33220);
        *sp_current_timestamp2 = 0;

		//Used to set sp_lastFullClientFrame and SCR_DrawInterface fps
        sp_current_timestamp = reinterpret_cast<int*>(reinterpret_cast<char*>(o_sofplus) + 0x331F4);
        *sp_current_timestamp = 0;

	
	
	}
	else {
        // Sys_Mil = 0x20055930; //original
        Sys_Mil = &my_Sys_Milliseconds;
    }

	#if 0
	//This was an idea to set 1 core affinity to make QueryPerformanceCounter behave nicer.
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
	
}
#endif

/*
	--IMPORTANT--
	When using Cvar_Get in _change callbacks, supply NULL as callback, to prevent infinite recursion.

	Cvar_Get(callback):
	     if callback:
	         if modified:
	              call callback()

	Cvar_Set2():
	     if value different than current:
	         if cvar->callback:
	             call callback()

	modified stays set if you never reset to false ;P (but its fine)(no need use modified system if using callbacks)

*/
void high_priority_change(cvar_t * cvar) {
	if ( cvar->value ) {
		PrintOut(PRINT_GOOD,"cpu priority is now HIGH\n");
		HANDLE hProcess = GetCurrentProcess();
		// Set the priority class to HIGH_PRIORITY_CLASS
		BOOL success = SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS);
	} else {
		PrintOut(PRINT_GOOD,"cpu priority is now NORMAL\n");
		HANDLE hProcess = GetCurrentProcess();
		// Set the priority class to HIGH_PRIORITY_CLASS
		BOOL success = SetPriorityClass(hProcess, NORMAL_PRIORITY_CLASS);
	}

}


void sleep_change(cvar_t * cvar) {
	if ( cvar->value ) {
		sleep_mode = true;
		extratime_resume=0;
		PrintOut(PRINT_GOOD,"cpu sleep is ENABLED\n");
	} else {
		sleep_mode = false;
		extratime_resume=0;
		PrintOut(PRINT_GOOD,"cpu sleep is DISABLED\n");
	}
	
}


void sleep_jitter_change(cvar_t *cvar)
{
	if ( cvar->value ) {
		sleep_jitter = true;
		PrintOut(PRINT_GOOD,"sleep_jitter is ENABLED\n");
	}
	else {
		sleep_jitter = false;
		PrintOut(PRINT_GOOD,"sleep_jitter is DISABLED\n");
	}
}
/*

*/
void sleep_busyticks_change(cvar_t * cvar)
{
	// PrintOut(PRINT_GOOD,"sleep_exclude_change changed\n");
	sleep_busyticks = cvar->value;

	PrintOut(PRINT_GOOD,"sleep_busyticks is now : %i\n",sleep_busyticks);
}

void cl_maxfps_change(cvar_t *cvar)
{
	// PrintOut(PRINT_GOOD,"cl_maxfps changed %f\n",cvar->value);

	//update _sofbuddy_sleep_gamma
	// orig_Cvar_Set2("_sofbuddy_sleep_gamma",cvar->string,false);
	previous_cl_maxfps = cvar->value;
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

On Linux/wine
Current Timer Resolution: 156250 ns
Maximum Timer Resolution: 10000 ns
Minimum Timer Resolution: 10000 ns

0.15625ms
*/
typedef NTSTATUS(WINAPI* pNtQueryTimerResolution)(PULONG, PULONG, PULONG);


#if 1

int winmain_loop(void)
{
    static int * const cls_state = reinterpret_cast<int*>(0x201C1F00);
	int newtime = 0, timedelta = 0;
    static int * const extratime = reinterpret_cast<int*>(0x201E7578);

	bool didSleep = false;
	bool isBeingBorrowed = false;

	int last_extratime_resume = 0;
	//detect fresh cl_frame()

	if ( *extratime == 0 ) {
		if ( extratime_resume > 0 ) {
			//We have to confirm that this new frame_window renders 2 frames on time
			//Else we correct our lie.
			isBeingBorrowed = true;

			//apply our lies to steal time from this new frame_window
			*extratime = extratime_resume;

			last_extratime_resume = extratime_resume;
			extratime_resume = 0;
		}
	}


	int targetMsec = *cls_state == 7 ? 100 : std::ceil(1000/cl_maxfps->value);

	/*
		. . . . . . . | draw . . . .  | draw . . .
		. . . . . . . | . . . . . . . | . . . . . . .

		we are at one of these dots.
		extratime is the dot we are at.
	*/
	if ( sleep_mode ) {
		/*
			On the first tick after frame is rendered.
			timedelta will be the time it took to render the frame.
			So no sleep will occur then.

			//If QCommon_Frame() (which does the main work, including rendering) consistently takes 
			//1ms or more, then timedelta will always be 1 or greater.
		*/
		//measure time in case:
		//(maybe QCommon_Frame() took 1ms already.. no need to sleep?)
		newtime = Sys_Mil();
		timedelta = newtime - oldtime;
		// orig_Com_Printf("timedelta1 = %i\n",timedelta);
		//1ms has not passed. 
		if ( timedelta == 0 ) {
			/*
				BURN TIME
			*/
			// orig_Com_Printf("FreeSleepTime = %i\n",(targetMsec - *extratime));
			if ( (targetMsec - *extratime)>sleep_busyticks && !didSleep ) {
				//ensures 1ms passes.
				//burn with Sleep()

				//any more iterations of this loop will USE busysleep
				didSleep = true;
				// orig_Com_Printf("We slept...\n");
				Sleep(1);
				newtime = Sys_Mil();

				//Sleep _STILL_ didn't produce >1ms change...
				timedelta = newtime - oldtime;
				if ( timedelta == 0 ) goto busyloop;

			} else {
				//ensures 1ms passes.
				busyloop:
				//burn with busyloop
				do
				{	
					__asm__ volatile("pause");
					newtime = Sys_Mil();
					timedelta = newtime - oldtime;
				}
				while (timedelta < 1);
				
			} 

		} //if no timedelta passed

		//1MS has passed...

		//Not really needed feature, but its there just in-case mb for slow laptops?
		if ( sleep_jitter ) {
			/*
				This 'frame' cannot fit into the 'frame_window',
				borrow from next 'frame_window'
			*/
			int remainingTime = targetMsec - (*extratime + timedelta);

			// (extratime+timedelta) > targetMsec
			if ( remainingTime < 0 ) {
				if ( !isBeingBorrowed ) {
					//Never borrow twice in a row
					int debt = 0 - remainingTime;
					if ( !extratime_resume ) {
						extratime_resume = debt;
					}
				} else {
					//We are being borrowed from by the previous frame_window
					//Yet we can't render our frame because of them.
					//Instead of cause a chain reaction, we tell the truth.
					timedelta += last_extratime_resume;
				}
			}

			//are we trying to borrow steal time from next frames' frame_window 
			//(force frames to render closer in time than average msec)
			if (extratime_resume) {
				//ensure we lie to server that this frame was on time.
				timedelta = targetMsec - *extratime;
				//Bad consequence of lieing : It really took 8 ms, we said 7ms
				//We expect it fits into next frame.
				//But if it doens't fit into next frame either, then that frame has to lie too
				//And so chains of lies == the frequency is false.
				//It is only _NOT_ a lie, if the next frame really fits 2 frames into 1 frame_window.
			}
		} //sleep_jitter feature

	} else {
		/*
			====SLEEP OFF====
		*/
		do
		{	
			__asm__ volatile("pause");
			newtime = Sys_Mil();
			timedelta = newtime - oldtime;
		}
		while (timedelta < 1);
		//Wait until 1ms has passed.

		
	} //if not sleep mode
	

	// This is intentionally above qCommonFrame, so that oldtime can be re-adjusted by future qpc_timer calls()
	// Without being overidden _AFTER_ qCommonFrame.
	oldtime = newtime;

	_controlfp( _PC_24, _MCW_PC );

	if ( o_sofplus ) {
		//We call this one because it handles everything, timers,free,_sp_cl_info_state,_sp_cl_info_server
        *reinterpret_cast<int*>(reinterpret_cast<char*>(o_sofplus) + 0x331AC) = 0x00;
        *reinterpret_cast<int*>(reinterpret_cast<char*>(o_sofplus) + 0x331BC) = 0x00;
		sp_Sys_Mil();
	}

	//higher msec value (less frequent) = lower frequency to server, more distance moved
	//lower msec value (more frequent) = faster frequency to server, less distance mvoed
	//when frequency and msec value do not match = change speed
	
	//Time is applied to extratime here: 
	orig_Qcommon_Frame (timedelta);

	//does not matter what we return here because we have jmp instruction immidiately after.
	return 0;
}


#else
// This is called in place of Sys_Millisecond.
int winmain_loop(void)
{

	int time,newtime;
	do {
		if ( _sofbuddy_sleep->value ) 
		{
			// orig_Com_Printf("Sleeping...\n");
			Sleep(1);
		}
		newtime = Sys_Mil();
		time = newtime - oldtime;
	} while (time < 1);
	
	_controlfp( _PC_24, _MCW_PC );
	#if 1
	if ( o_sofplus ) {
		//we will handle the sleep, instead.
		// setCvarInt(_sp_cl_cpu_cool,0);
		// setCvarInt(_sp_cl_frame_delay,0);
		*(int*)(o_sofplus + 0x331AC) = 0x00;
		*(int*)(o_sofplus + 0x331BC) = 0x00;
		sp_Sys_Mil();
	}	
	#endif

	oldtime = newtime;
	orig_Qcommon_Frame (time);

	//fake return because we are implementing everything
	return 0;
}
#endif

/*
	ULONG CurrentResolution, MaximumResolution, MinimumResolution;

	HMODULE hNtDll = LoadLibrary("ntdll.dll");
	pNtQueryTimerResolution NtQueryTimerResolution =
            (pNtQueryTimerResolution)GetProcAddress(hNtDll, "NtQueryTimerResolution");
    // Query the timer resolution
    NTSTATUS status = NtQueryTimerResolution(&CurrentResolution, &MaximumResolution, &MinimumResolution);

    if (status == 0) {
        std::wcout << L"Current Timer Resolution: " << CurrentResolution << L" ns\n";
        std::wcout << L"Maximum Timer Resolution: " << MaximumResolution << L" ns\n";
        std::wcout << L"Minimum Timer Resolution: " << MinimumResolution << L" ns\n";
    } else {
        std::wcout << L"Failed to query timer resolution, NTSTATUS: " << status << L"\n";
    }
*/


/*
freq = 10,000,000, so resolution = 1/10,000,000 = 0.0000001 = 0.1 microseconds = 100 nanoseconds.
1000 ns = 1us
1000 us = 1ms
1000 ms = 1s
_sp_cl_cpu_cool 1 = 0.1 ms = 100 us
*/
inline long long qpc_timers(bool force)
{
	
	LARGE_INTEGER cur = {0};

	if (!freq.QuadPart || force)
    {
		if (!QueryPerformanceFrequency(&freq))
		{
    orig_Com_Printf("QueryPerformanceFrequency failed\n");
			ExitProcess(1);
		}
	}
	//obtain a new base.
	if (!base.QuadPart || force)
	{
		if (!QueryPerformanceCounter(&base))
		{
    orig_Com_Printf("QueryPerformanceCounter failed\n");
			ExitProcess(1);
		}
	}
	
	QueryPerformanceCounter(&cur);
	return (long long)(cur.QuadPart - base.QuadPart);
}


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
		// printf("while diff < 0\n");
   		ticks_elapsed = qpc_timers(true);
		wasNeg = true;
	}

    //100ns tick * 1,000,000 = ns->us->ms 100ms
    // ticks_elapsed/freq = seconds. seconds * 1,000,000 = microseconds.
	int ret = (int)((ticks_elapsed * 1000LL) / freq.QuadPart);
	// printf("now: %i :: old %i\n",ret,prev_val);
	if ( ret < prev_val ) {
		// printf("if ret < prev_val\n");

		// ticks_elapsed = qpc_timers(true);
		// ret = ticks_elapsed * 1000 / freq.QuadPart;
		wasNeg = true;
	}

	// assert( !wasNeg );

	/*
		Not sure how often this occurs
	*/
	if ( wasNeg ) {
		// Have to adjust the timers sadly.
		// Or change the location of 'exec sofplus.cfg' slightly

		// printf("wasNeg\n");
		// orig_Com_Printf("wasNeg\n");

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

int qpcTimeStampNano(void)
{
    //freq = 10,000,000, so resolution = 1/10,000,000 = 0.0000001 = 0.1 microseconds = 100 nanoseconds.
    //1000 ns = 1us
    //1000 us = 1ms
    //1000 ms = 1s
    //_sp_cl_cpu_cool 1 = 0.1 ms = 100 us

	LARGE_INTEGER freq = {0};
    long long diff = qpc_timers(false);
    //Handle bug when thread switches core or bios/driver bug.
	while ( diff < 0 ) {
		diff = qpc_timers(true);
	}

    //100ns tick * 1,000,000 = ns->us->ms 100ms
    // diff/freq = seconds. seconds * 1,000,000 = microseconds.
	int ret = diff * 1'000'000'000 / freq.QuadPart;

	return ret;
}