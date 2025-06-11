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
int *sp_oldtime = NULL;
int *sp_oldtime_internal = NULL;


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
	*sp_oldtime = val;
	*sp_oldtime_internal = val;
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
	Sys_Mil_AfterHookThunk = DetourCreate(0x20055930,&my_Sys_Milliseconds,DETOUR_TYPE_JMP,5);

	#if 0
	//Correct the timer value used in sofplus timers.
	if (o_sofplus) {
		int * lastSofplusTimerTick = (void*)o_sofplus+0x52630;
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
/*
	Also implements cl_maxfps in singleplayer

	QCommon_Init() is called before any loop stuff.

	SoFPlus Init (WSAStartup) is called by NET_Init() inside QCommon_Init() just before this function.

	Because the sofplus addons are ran by WSA_Startup()-. Cbuf_AddText("exec sofplus.cfg").
	The actual first call to sp_sc_timer occurs later when the console is executed.

	This means the next call to Sys_Milliseconds() after WSA_Startup() is setting the curtime back again.

	NOTE: Sys_Miliseconds is called inside Sys_Init() before NET_Init().
*/
void mediaTimers_apply_afterCmdline(void)
{
	//CVAR_ARCHIVE - save to config.cfg
	//CVAR_NOSET - write-only.
	//===Cvars====
	cl_maxfps = orig_Cvar_Get("cl_maxfps","30",NULL,&cl_maxfps_change);
	
	create_mediatimers_cvars();


	//===Memory Edits===

	//Detour sys_milliseconds for any other code that calls sys_milliseconds uses media timer instead of timegettime.
	//gameloop calls -> sofplus_sys_milli -> our_sys_milli -> orig_sys_milli
	//important we call this super early to catch any bad curtime being set by sys_millisecond timegettime implem.
	// mediaTimers_early();


	// main() loop reimplemented.

	//Remember that this is where ctrl hooks the Sys_milliseconds()
	//Patch call to sys_millisecond -

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

		
		sp_Sys_Mil = (void*)o_sofplus+0xFA60; //sofplus sys_mil
		Sys_Mil = &my_Sys_Milliseconds;

		sp_whileLoopCount = (void*)o_sofplus+0x33188;
		*sp_whileLoopCount = 0;

		//If framecount increased (CL_Frame ran) sets 
		sp_lastFullClientFrame = (void*)o_sofplus+0x331F0;
		*sp_lastFullClientFrame = 0;

		sp_oldtime_internal = (void*)o_sofplus+0x33220;
		*sp_oldtime_internal = 0;

		sp_oldtime = (void*)o_sofplus+0x331F4;
		*sp_oldtime = 0;

		

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
	/*
	int nt=0, t=0,ot=0;
	do
	{
		nt = Sys_Mil ();
		t = nt - ot;
	} while (t < 1);
	//Time is applied to extratime here: 
	orig_Qcommon_Frame (t);

	ot = nt;
	//does not matter what we return here.
	return 0;
	*/

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
			// printf("new sys_mil()\n");
			newtime = Sys_Mil();
			#if 0
			if (o_sofplus) {
				//Call this to handle timers!
				sp_Sys_Mil();
			}
			#else
			if (o_sofplus) {
				*sp_whileLoopCount = 0;
				spcl_FreeScript();
			}
			#endif
			
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

					// printf("after sleep\n");
					//sleep was performed, so update.
				newtime = Sys_Mil();
					//Call this to execute any timers!
					// sp_Sys_Mil();

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
						// printf("busysleep\n");
					__asm__ volatile("pause");
					newtime = Sys_Mil();
						#if 0
						if (o_sofplus) {
							//Call this to handle timers!
							sp_Sys_Mil();
						}
						#else
					if (o_sofplus) {
						*sp_whileLoopCount = 0;
						spcl_FreeScript();
					}
						#endif
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
			// printf("busysleep 2\n");
			__asm__ volatile("pause");
			newtime = Sys_Mil();
			#if 0
			if (o_sofplus) {
				//Call this to handle timers!
				sp_Sys_Mil();
			}
			#else
			if (o_sofplus) {
				*sp_whileLoopCount = 0;
				spcl_FreeScript();
			}
			#endif
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
	#if 1
	if ( o_sofplus ) {
		//Try to get sofplus to fire/Cbuf_AddText the timers callbacks.
		//It uses curtime and updates
		//int * lastSofplusTimerTick = (void*)o_sofplus+0x52630;
		//to curtime at end. 

		//Maybe the difference is something small like:
		//the initial value of curtime in sofplus/q2 is not 0?
		//we can test it by adding 1 to curtime.

		//1 ms should have already passed, so the Timers should be fired.
		//Why are they not being fired?
		// int* curtext_size_before = *(int*)0x2023F830; 
		// spcl_Timers();
		// spcl_FreeScript();
		//We call this one because it handles everything, timers,free,_sp_cl_info_state,_sp_cl_info_server

		//TODDO : Ensure _sp_cl_cpu_cool is fully disabled here.
		sp_Sys_Mil();
		// resetTimers(newtime);
		// int* curtext_size_after = *(int*)0x2023F830;

		/*
		if (curtext_size_after != curtext_size_before) {
			printf("CBuf_AddText for Timer!\n");
			orig_Com_Printf("Cbuf_AddText for Timer!\n");	
		}
		*/
		
	}
	#endif

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
	
	// orig_Com_Printf("Before qcommon_Frame\n");
	//Time is applied to extratime here: 
	orig_Qcommon_Frame (time);
	// orig_Com_Printf("After qcommon_Frame\n");

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
freq = 10,000,000, so resolution = 1/10,000,000 = 0.0000001 = 0.1 microseconds = 100 nanoseconds.
1000 ns = 1us
1000 us = 1ms
1000 ms = 1s
_sp_cl_cpu_cool 1 = 0.1 ms = 100 us
*/
__attribute__((always_inline)) long long qpc_timers(bool force)
{
	
	LARGE_INTEGER cur = {0};

	if (!freq.QuadPart || force)
    {
		if (!QueryPerformanceFrequency(&freq))
		{
			printf("QueryPerformanceFrequency failed\n");
			orig_Com_Printf("QueryPerformanceFrequency failed\n");
			ExitProcess(1);
		}
	}
	//obtain a new base.
	if (!base.QuadPart || force)
	{
		if (!QueryPerformanceCounter(&base))
		{
			printf("QueryPerformanceCounter failed\n");
			orig_Com_Printf("QueryPerformanceCounter failed\n");
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