#include "feature_config.h"

#if FEATURE_MEDIA_TIMERS

#include "sof_buddy.h"
#include "util.h"
#include "sof_compat.h"
#include "shared.h"

#include "customfloat.h"

#include <cmath>
#include <cassert>

bool sleep_mode = true;
int extratime_resume = 0;

bool sleep_jitter = false; 

//how many msec ticks to busyloop for each frame
int sleep_busyticks = 2;

cvar_t * cl_maxfps = NULL;
extern cvar_t * _sofbuddy_sleep;

int (*sp_Sys_Mil) (void) = NULL;

#include "generated_detours.h"

LARGE_INTEGER base = {0};
LARGE_INTEGER freq = {0};

extern void* o_sofplus;
extern void (*orig_Qcommon_Frame) (int msec);

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

void (*spcl_Timers)(void) = NULL;
void (*spcl_FreeScript)(void) = NULL;


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
	SOFBUDDY_ASSERT(cvar != nullptr);
	
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
	SOFBUDDY_ASSERT(cvar != nullptr);
	
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
	SOFBUDDY_ASSERT(cvar != nullptr);
	
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
	SOFBUDDY_ASSERT(cvar != nullptr);
	SOFBUDDY_ASSERT(cvar->value >= 0);
	
	// PrintOut(PRINT_GOOD,"sleep_exclude_change changed\n");
	sleep_busyticks = cvar->value;

	PrintOut(PRINT_GOOD,"sleep_busyticks is now : %i\n",sleep_busyticks);
}

void cl_maxfps_change(cvar_t *cvar)
{
	SOFBUDDY_ASSERT(cvar != nullptr);
	SOFBUDDY_ASSERT(cvar->value > 0.0f);
	
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
/*
GOAL:
the frames sacrifice equal distance from each other in time for equal frequency per second
so more stutter, but the stutter is not noticeable if you are on 60hz refresh eg.

msec sent to server is set:
ms = cls.frametime * 1000;

This variable is the actual msec when factoring in load + vsync etc.
Not the target/max msec. (because of extratime - measured)

cls.frametime is calculated in CL_Frame():
cls.frametime = extratime/1000.0;

Which makes sense, since its the extratime that triggered the frame to draw...
When vsync is active the renderFrame will block, causing high time/msec value pushed into CL_Frame()
Making extratime have a very large value.

SoF uses 1/cls.frametime to compute fps display
So if we modify extratime, we are messing with the fps display _AND_ that affects user speed, so its potentially
unsafe.
*/
int winmain_loop(void)
{
	static int * const cls_state = (int*)0x201C1F00;
	int newtime = 0, timedelta = 0;
	static int * const extratime = (int*)0x201E7578;

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


	SOFBUDDY_ASSERT(cls_state != nullptr);
	SOFBUDDY_ASSERT(extratime != nullptr);
	
	int targetMsec = *cls_state == 7 ? 100 : (cl_maxfps ? std::ceil(1000/cl_maxfps->value) : 33);
	SOFBUDDY_ASSERT(targetMsec > 0);

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
		newtime = my_Sys_Milliseconds();
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
				newtime = my_Sys_Milliseconds();
				timedelta = newtime - oldtime;
				if ( timedelta == 0 ) goto busyloop;

			} else {
				//ensures 1ms passes.
				busyloop:
				//burn with busyloop
				do
				{	
					__asm__ volatile("pause");
					newtime = my_Sys_Milliseconds();
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
			newtime = my_Sys_Milliseconds();
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
		SOFBUDDY_ASSERT(sp_Sys_Mil != nullptr);
		*(int*)((char*)o_sofplus + 0x331AC) = 0x00;
		*(int*)((char*)o_sofplus + 0x331BC) = 0x00;
		sp_Sys_Mil();
	}

	//higher msec value (less frequent) = lower frequency to server, more distance moved
	//lower msec value (more frequent) = faster frequency to server, less distance mvoed
	//when frequency and msec value do not match = change speed
	
	//Time is applied to extratime here: 
	SOFBUDDY_ASSERT(orig_Qcommon_Frame != nullptr);
	SOFBUDDY_ASSERT(timedelta >= 0);
	orig_Qcommon_Frame (timedelta);

	//does not matter what we return here because we have jmp instruction immidiately after.
	return 0;
}
/*
========LOOP EDITED=========

	while (1)
	{
		<--------------------EXIT/RESUME
		// if at a full screen console, don't update unless needed
		if (Minimized || (dedicated && dedicated->value) )
		{
			Sleep (1);
		}

		while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage (&msg, NULL, 0, 0))
				Com_Quit ();
			sys_msg_time = msg.time;
			TranslateMessage (&msg);
   			DispatchMessage (&msg);
		}

		do
		{
			newtime = Sys_Milliseconds (); <----------------ENTRY
			time = newtime - oldtime;
		} while (time < 1);

		_controlfp( _PC_24, _MCW_PC );
		Qcommon_Frame (time);

		oldtime = newtime;
		<--------------------EXIT/RESUME
	}
*/

#else
// This is called in place of Sys_Millisecond.
int winmain_loop(void)
{

	int time,newtime;
	do {
		if ( _sofbuddy_sleep && _sofbuddy_sleep->value ) 
		{
			// orig_Com_Printf("Sleeping...\n");
			Sleep(1);
		}
		newtime = my_Sys_Milliseconds();
		time = newtime - oldtime;
	} while (time < 1);
	
	_controlfp( _PC_24, _MCW_PC );
	#if 1
	if ( o_sofplus ) {
		//we will handle the sleep, instead.
		// setCvarInt(_sp_cl_cpu_cool,0);
		// setCvarInt(_sp_cl_frame_delay,0);
		*(int*)((char*)o_sofplus + 0x331AC) = 0x00;
		*(int*)((char*)o_sofplus + 0x331BC) = 0x00;
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




int qpcTimeStampNano(void)
{
	extern long long qpc_timers(bool force);
    long long diff = qpc_timers(false);
    while ( diff < 0 ) {
		diff = qpc_timers(true);
	}
	if (!freq.QuadPart) {
		return 0;
	}
	SOFBUDDY_ASSERT(freq.QuadPart > 0);
	int ret = diff * 1'000'000'000 / freq.QuadPart;
	return ret;
}

#endif // FEATURE_MEDIA_TIMERS