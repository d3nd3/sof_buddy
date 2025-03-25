#include "sof_buddy.h"

//for _controlfp
#include "customfloat.h"

#include <iostream>

#include "sof_compat.h"
#include "features.h"


#include "util.h"


#include "../DetourXS/detourxs.h"

#include <cmath>  // Include cmath for std::ceil

void high_priority_change(cvar_t * cvar);
void sleep_change(cvar_t * cvar);
bool sleep_mode = true;
int eatenFrame = 0;

void sleep_gamma_change(cvar_t *cvar);
int sleep_gamma = 4; //300fps

//how many msec ticks to busyloop for each frame
void sleep_busyticks_change(cvar_t * cvar);
int sleep_busyticks = 2;


void cl_maxfps_change(cvar_t *cvar);

cvar_t * test = NULL;
cvar_t * cl_maxfps = NULL;

int (*Sys_Mil) (void) = NULL;
int (*Sys_Mil_AfterHookThunk) (void) = NULL;

int winmain_loop(void);
int my_Sys_Milliseconds(void);

LARGE_INTEGER freq;
LARGE_INTEGER base;
int qpcTimeStampNano(void);

int oldtime = 0;
int frametime = 100;
float previous_cl_maxfps = 30.0f;

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

	/*
		There are all implemented to make _sp_cl_frame_delay work.
	*/
	
	//newTimeBeforeFrameDrawn
	//*(int*)((void*)o_sofplus+0x331F0) = val;

	// previous_newtime (thus used for staying in the busyloop)
	// slows down framerate
	// if (newtime - newTimeBeforeFrameDrawn )
	*(int*)((void*)o_sofplus+0x331F4) = val;
	// previous_newtime_clone
	*(int*)((void*)o_sofplus+0x33220) = val;
	
}

void (*spcl_Timers)(void) = NULL;

void (*spcl_FreeScript)(void) = NULL;
void mediaTimers_early(void)
{
	
}
#ifdef FEATURE_MEDIA_TIMERS
/*
	Also implements cl_maxfps in singleplayer
*/
void mediaTimers_apply(void)
{
	//CVAR_ARCHIVE - save to config.cfg
	//CVAR_NOSET - write-only.
	//===Cvars====
	cl_maxfps = orig_Cvar_Get("cl_maxfps","30",NULL,&cl_maxfps_change);
	test = orig_Cvar_Get("test","100",NULL,NULL);
	create_mediatimers_cvars();


	//===Memory Edits===
	freq = {0};
	base = {0};

	//Detour sys_milliseconds for any other code that calls sys_milliseconds uses media timer instead of timegettime.
	//gameloop calls -> sofplus_sys_milli -> our_sys_milli -> orig_sys_milli
	Sys_Mil_AfterHookThunk = DetourCreate(0x20055930,&my_Sys_Milliseconds,DETOUR_TYPE_JMP,5);


	// main() loop reimplemented.

	//Patch call to sys_millisecond
	WriteE8Call(0x20066412,&winmain_loop);

	//continue the outer loop once sys_millisecond returns, nothing else.
	WriteE9Jmp(0x20066417,0x2006643C);


	//use Sys_Milliseconds() instead of timeGetTime() for: 
	//	sys_win.c
	//		Sys_SendKeyEvent()
	//			sys_frame_time = timeGetTime();
	//Wasn't originally an 0xE8 call, was 0xFF 0x15. Thus extra byte nopped.
	WriteE8Call(0x20065D5E,&my_Sys_Milliseconds); // 5e 5f 60 61 62 63
	WriteByte(0x20065D63,0x90);


	//==SoF Plus integration==
	if ( o_sofplus ) {
		spcl_FreeScript = (void*)o_sofplus+0x9D20;
		spcl_Timers = (void*)o_sofplus+0x10190;
		resetTimers(0);
		*(int*)((void*)o_sofplus+0x331F0) = 0;
		//no longer need to use sofplus sys_mil
		// Sys_Mil = (void*)o_sofplus+0xFA60; //sofplus sys_mil
		Sys_Mil = &my_Sys_Milliseconds;
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
		eatenFrame=0;
	} else {
		sleep_mode = false;
		eatenFrame=0;
	}
	PrintOut(PRINT_GOOD,"sleep is now : %i\n",sleep_mode);
}

/*
	TODO: Get max ms they can render at and adjust.
	was = 5
	now using 1000/500 = 2
	claiming system can bench 500fps uncapped
*/
void sleep_gamma_change(cvar_t *cvar)
{
	// PrintOut(PRINT_GOOD,"sleep_gamma changed\n");
	// orig_Cvar_SetInternal(1);
	cl_maxfps = orig_Cvar_Get("cl_maxfps","30",NULL,NULL);
	int targetMsec = std::ceil(1000/cl_maxfps->value);

	//disable it
	if ( cvar->value == 0.0f ) {
		//targetMsec - 1000/200 ... assume system can run 200fps at peak.
		sleep_gamma = 0; //default
		PrintOut(PRINT_GOOD,"sleep_gamma is now : %i\n",sleep_gamma);
		return;
	}
	
	//frametimeMS - maxRendertimeMS = remaining after render time = spare time.
	// provide fps of max system
	//fast system means larger values (more space in next frame to occupy)
	//slow system means smaller values (less space in next frame to occupy)
	sleep_gamma =  targetMsec - std::round(1000/cvar->value);
	if (sleep_gamma >= targetMsec) {
		//impossible scenario = sleeping all frame ( highest value for least sleep = targetMsec - 1)
		//targetMsec - 1000/200 ... assume system can run 200fps at peak.
		sleep_gamma = targetMsec - 1; //Give them the fastest setup
	}
	//ceil ensures the slower of the ms  is selected when between int ranges.
	//eg. 1000/550 = 1.8, so use 2ms.

	PrintOut(PRINT_GOOD,"sleep_gamma is now : %i\n",sleep_gamma);
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

/*
GOAL:
the frames sacrifice equal distance from each other in time for equal frequency per second

so more stutter, but the stutter is not noticeable if you are on 60hz refresh eg.

msec sent to server is set:
ms = cls.frametime * 1000;

This variable is the actual msec when factoring in load + vsync etc.
Not the target/max msec.

cls.frametime is calculated in CL_Frame():
cls.frametime = extratime/1000.0;

Which makes sense, since its the extratime that triggered the frame to draw...
When vsync is active the renderFrame will block, causing high time/msec value pushed into CL_Frame()
Making extratime have a very large value.

SoF uses 1/cls.frametime to compute fps display
So if we modify extratime, we are messing with the fps display.
*/
int winmain_loop(void)
{
	static int * const cls_state = 0x201C1F00;
	

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
	//orig_Com_Printf("winmain_loop\n");

	int newtime = 0, time = 0;
	static int * extratime = 0x201E7578;
	// static bool firstRunThisFrame = true;
	bool didSleep = false;

	// If Qcommon detected new CL_Frame(), we override it to eat.
	if ( *extratime == 0 ) {
		//detect fresh cl_frame()
		// firstRunThisFrame = true;
		cl_maxfps->value = previous_cl_maxfps;
		*extratime = eatenFrame;
	}
	
	//Reset this - it has served its purpose by filling extratime above.
	eatenFrame = 0;

	/*
	if ( firstRunThisFrame ) {
		firstRunThisFrame = false;
		// if ( !didSleep ) PrintOut(PRINT_GOOD,"Can't Sleep this frame, starved: %i\n",remainingTime);
	}
	*/
	int targetMsec = *cls_state == 7 ? 100 : std::ceil(1000/cl_maxfps->value);

	if ( sleep_mode ) {

		
		while(1) {
			if (o_sofplus) {
				*(int*)((void*)o_sofplus+0x33188) = 0;
				spcl_FreeScript();
			}
			
			
			// printf("lol?\n");
			/*
				====SLEEP ON====
			*/
			//1000/cl_maxfps == float
			//sof engine uses float comparison
			//so we use ceil
			//1000/143-160 = 6.X
			//such that 6 < 6.X
			//so use ceil to get same behaviour in int
			//6 < 7
			

			/*
				Time measurements are taken before and after running Qcommon_Frame()
				The result of that measurement is here.

				----qCommon extratime: 5
				timestamp+measure: 1ms
				----qCommon extratime: 6
				timestamp+measure: 1ms
				----qCommon extratime: 7 drawFrame() reset
				timestamp+measure: 3ms
				----qCommon extratime: 3
				timestamp+measure: 1ms
				----qCommon extratime: 4
				timestamp+measure: 1ms
				----qCommon extratime: 5
			*/
			newtime = Sys_Mil();
			time = newtime - oldtime;

			//calculating last Frame's render time in advanced to predict if it will make extratime > targetMsec
			int remainingTime = targetMsec - (*extratime+time);
			// int remainingTime = frametime - (*extratime + time);

			//WE HAVE A PROBLEM--> vsync has lower fps than cl_maxfps
			//Also how can we ever approximate a system capability.
			//CAVEAT: THIS feature of eating into next frame has to be reserved for systems that target a fixed fps with VSYNC off.
			//TODO: Check vsync is off + FORCE off.
			//Also this could slow your movespeed down in-game if your actual fps dips too much below your maxfps

			//100 99 2
			//7 6 2
			//ah so negative remainingTime is the condition when the sum of the msec units
			//does not fit into the total. interesting problem.
			//*extratime 
			if ( remainingTime < 0 ) {
				// PrintOut(PRINT_GOOD,"CannotFit-> targetMsec:%i usedTime:%i lastChunk:%i\n",targetMsec,*extratime,time);
				int debt = 0 - remainingTime;
				//on a fast system u can draw the frame in 1 or 2 ms.
				//_should_ benchmark this to get theier system value.
				//when small debt, try to squeeze it into next frame.
				//allows 7-2 = 5ms of render time == 200fps capable system.
				// int sleep_gamma =  frametime - std::round(1000/_sofbuddy_sleep_gamma->value);
				if ( debt <= sleep_gamma && !eatenFrame ) {
					//OUR ATTEMPT TO COMPENSATE THE NEXT FRAME WINDOW...
					// measure how much it eats into next frame
					// ===================================================================
					// PrintOut(PRINT_GOOD,"Non-Sleep: eating into next frame %i\n",debt);
					
					// time = 
					eatenFrame = debt;
					//extratime gets set to this later in this function.
					// eatenFrame = time;

					//temporary force QCommon_Frame() to renderNewFrame
					//this has to be the actual frametime because it sets cls.frametime and msec in cmd.
					// *extratime = targetMsec;
					// *extratime = frametime;


					//whichever value extrattime has now, whcih is lower than usual.
					previous_cl_maxfps = cl_maxfps->value;
					cl_maxfps->value = 1000.0f;

				} else {
					time = newtime - oldtime;
				}
				//break to QCommon_Frame()
				break;
			}

			if ( time == 0 ) {
				//require a sleep of some form. because 0 ms passed.

				// PrintOut(PRINT_GOOD,"targetMsec =  %i\n",targetMsec);
				//AFTER Render CL_Frame() has been measured AND ONCE.. (because only want 1ms minimum sleep per iteration here.)

				//Impossible for remainingTime to be 0 - implies extratime == targetMsec - because last QCommon_Frame would had reset it otherwise.
				//2ms ticks of busyloop at end of every frame. (exclude 2,1)
				if ( /**extratime > eatenFrame &&*/ remainingTime>sleep_busyticks && !didSleep ) {
					// printf("Sleep...??\n");
					didSleep = true;
					//AfterFrameRendered...
					// unsigned long before = qpcTimeStampNano();
					Sleep(1);
					// 1058500 nanoseconds for 1ms sleep.
					// unsigned long after = qpcTimeStampNano();

					

					// unsigned long actualSlept = after - before;
					//if (actualSlept >= 2'000'000) PrintOut(PRINT_GOOD,"Long Sleep %u\n",actualSlept);
					// actualSlept as MS
					// int actualSleptMS = actualSlept / 1'000'000;
					/*
						too much quota/time was used up, so this frame is drawn late, but
						the next frame will still (fit into the frame window time hopefully, if it does 
						not, then you just notice frame drops, any long frame is by default allowed to take as
						long as it wants. its execClientFrame() when timePassed >= 7ms , so a minimum guarantee only.)

						What i'm saying is that, frames are independent, they dont know if the previous one was laggy or not
						There is no compensation for a long average, they independent units.
					*/

					//sleep was performed, so update.
					newtime = Sys_Mil();
					//Sleep didn't produce >1ms change...
					int time_and_sleep = newtime - oldtime;
					if ( time_and_sleep == 0 ) 
					{
						//is this correct?
						//See below function to see main loop structure.
						//We want to have time_and_sleep > 1 before iterating the loop again.
						//I made it such that if this is the case, it won't try sleep again, because it must be close to 1ms, just
						//busy sleep to further it.
						continue;
					}
					int overlapMs = time_and_sleep - remainingTime;
					//time_and_sleep > remainingTime
					// int sleep_gamma =  frametime - std::round(1000/_sofbuddy_sleep_gamma->value);
					//allows 7-2 = 5ms of render time == 200fps capable system.
					if ( overlapMs > 0 && overlapMs <= sleep_gamma && !eatenFrame ) {
						//OUR ATTEMPT TO COMPENSATE THE NEXT FRAME WINDOW...
						// measure how much it eats into next frame
						
						// time = actualSleptMS;
						// if ( time > 0) time = time -1;
						// if ( time < targetMsec ) time +=1;
						// ===================================================================
						// PrintOut(PRINT_GOOD,"Eating into next frame %i\n",time_and_sleep);
						

						// time = overlapMs;
						time = time_and_sleep;
						//extratime gets set to this later in this function.
						eatenFrame = overlapMs;

						//temporary force QCommon_Frame() to renderNewFrame
						// *extratime = targetMsec;
						// *extratime = frametime;


						previous_cl_maxfps = cl_maxfps->value;
						cl_maxfps->value = 1000.0f;
						
					} else {
						time = time_and_sleep;
					}
					//break to QCommon_Frame()
					break;
				} else {
					// printf("busy-sleep...??\n");
					/*
						remainingTime < 2 :: 1 or 0
						Do busywait at end to reduce chance of sleep overlap.
					*/
					
					//Do busyloop if remainingTime too small
					do
					{	

						__asm__ volatile("pause");
						newtime = Sys_Mil();
						if (o_sofplus) {
							*(int*)((void*)o_sofplus+0x33188) = 0;
							spcl_FreeScript();
						}
						time = newtime - oldtime;
					}
					while (time < 1);
					//Wait until 1ms has passed.

					//break to QCommon_Frame()
					break;
				} //debt-sleep-busy triple if

			} //if no time passed

			//time passed during compute, was not in excess.
			break;
		}//while(1)
		
	} else {
		/*
			====SLEEP OFF====
		*/
		/*
		if ( firstRunThisFrame ) {
			firstRunThisFrame = false;
			PrintOut(PRINT_GOOD,"Can't Sleep this frame, starved: %i\n",remainingTime);
		}
		*/
		//Do busyloop if remainingTime too small
		do
		{	
			__asm__ volatile("pause");
			newtime = Sys_Mil();
			if (o_sofplus) {
				*(int*)((void*)o_sofplus+0x33188) = 0;
				spcl_FreeScript();
			}
			time = newtime - oldtime;

			/*
			//RenderFrameMsec = 1 or 2 ms on my system.
			//That leaves 5 msec for sleep
			if ( *extratime == 0 ) {
				orig_Com_Printf("RenderFrameMsec = %i\n",time);
			}
			*/
		}
		while (time < 1);
		//Wait until 1ms has passed.
	}
	

/*
	//Do something once per Frame.
	if ( firstRunThisFrame ) {
		firstRunThisFrame = false;
		if ( *extratime > eatenFrame ) {
			eatenFrame = 0;
			
			//debug iteration after frame drawn.
			if (remainingTime <= 2) {
				PrintOut(PRINT_GOOD,"Can't Sleep this frame, starved: %i\n",remainingTime);
			}
			
		}
	}
	
*/

	// This is intentionally above qCommonFrame, so that oldtime can be changed during any code in Qcommon from Sys_SendKeyEvent() eg.

	oldtime = newtime;
	if ( o_sofplus ) {
		spcl_Timers();
		resetTimers(newtime);
	}

	_controlfp( _PC_24, _MCW_PC );

	//higher msec value (slowerframe) = larger distance moved
	//higher frame = faster frequency to server
	//when frequency and msec value do not match = change speed

	//if we are pushing for 7ms when we are 6ms frequency, we do speed boost unintended.
	
	if ( !eatenFrame && *extratime+time >= targetMsec) {
		//frame about to be rendered.
		frametime = *extratime+time;
		// *extratime = test->value;
	} else if (eatenFrame) {
		frametime = targetMsec;
		// *extratime = test->value;
	}
	
	//Time is applied to extratime here: 
	orig_Qcommon_Frame (time);


	//does not matter what we return here.
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

/*
	
The difference between an older and a newer value of the QueryPerformanceCounter can be negative.
(This can happen when the thread moves from one CPU core to an other.)

Consider explicitly binding the thread to a specific processor using SetThreadAffinityMask to avoid counter discrepancies caused by core switching.
*/
// SofPlus -> Us -> orig
int my_Sys_Milliseconds(void)
{
	/*
	int a = timeGetTime();
	//set curtime
    *(int*)0x20390D38 = a;
	return a;
	*/
	//orig_Com_Printf("mediaTimersWait\n");

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
   		// orig_Com_Printf("QueryPerformanceCounter is negative\n");
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
   		// orig_Com_Printf("QueryPerformanceCounter is wrapped\n");
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
    	// _sp_cl_frame_delay support
    	resetTimers(ret);
    	*(int*)((void*)o_sofplus+0x331F0) = ret;
    }

    //set curtime
    *(int*)0x20390D38 = ret;

    return ret;
}

int qpcTimeStampNano(void)
{
	//orig_Com_Printf("mediaTimersWait\n");
    

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
   	}

    //100ns tick * 1,000,000 = ns->us->ms 100ms
    // diff/freq = seconds. seconds * 1,000,000 = microseconds.
    int ret = diff * 1'000'000'000 / freq.QuadPart;

    return ret;
}