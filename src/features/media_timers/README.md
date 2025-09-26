# Media Timers

- Hook: Sys_Milliseconds
- Reason: Provides consistent timing for media/video playback; exposes cvars for tuning.
- Files:
  - hooks.cpp: (planned) per-feature hook targets and behaviors.



## features mediaTimers_apply_afterCmdline

	Also implements cl_maxfps in singleplayer

	QCommon_Init() is called before any loop stuff.

	SoFPlus Init (WSAStartup) is called by NET_Init() inside QCommon_Init() just before this function.

	Because the sofplus addons are ran by WSA_Startup()-. Cbuf_AddText("exec sofplus.cfg").
	The actual first call to sp_sc_timer occurs later when the console is executed.

	This means the next call to Sys_Milliseconds() after WSA_Startup() is setting the curtime back again.

	NOTE: Sys_Miliseconds is called inside Sys_Init() before NET_Init().

## winmain_loop
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


## loop edited
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


## Sys_Milliseconds()
The difference between an older and a newer value of the QueryPerformanceCounter can be negative.
(This can happen when the thread moves from one CPU core to an other.)

Consider explicitly binding the thread to a specific processor using SetThreadAffinityMask to avoid counter discrepancies caused by core switching.


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
