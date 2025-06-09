#include "sof_buddy.h"

#include <iostream>

#include "sof_compat.h"
#include "features.h"

#include "./DetourXS/detourxs.h"

#include "util.h"

//__ioinit
void (*orig_FS_InitFilesystem)(void) = NULL;
void my_FS_InitFilesystem(void);
void my_orig_Qcommon_Init(int argc, char **argv);
qboolean my_Cbuf_AddLateCommands(void);

cvar_t *(*orig_Cvar_Get)(const char * name, const char * value, int flags, cvarcommand_t command) = 0x20021AE0;
cvar_t *(*orig_Cvar_Set2) (char *var_name, char *value, qboolean force) = 0x20021D70;
void (*orig_Cvar_SetInternal)(bool active) = 0x200216C0;
void ( *orig_Com_Printf)(char * msg, ...) = NULL;
void (*orig_Qcommon_Frame) (int msec) = 0x2001F720;

void (*orig_Qcommon_Init) (int argc, char **argv) = 0x2001F390;
qboolean (*orig_Cbuf_AddLateCommands)(void) = NULL;

void (*orig_CL_UpdateSimulationTimeInfo)(float extratime) = NULL;
void (*orig_CL_ReadPackets)(void) = NULL;

void * (*orig_Sys_GetGameApi)(void * imports) = NULL;
void (*orig_CinematicFreeze)(bool bEnable) = NULL;
void my_CinematicFreeze(bool bEnable);


char *(*orig_Sys_GetClipboardData)( void ) = 0x20065E60;
char *Sys_GetClipboardData( void );

void * my_Sys_GetGameApi(void * imports)
{

	void * pv_ret = 0;
	// calls LoadLibrary
	pv_ret = orig_Sys_GetGameApi(imports);

	//Fixing cl_maxfps limitter for singleplayer-bug cinematics
	DetourRemove(&orig_CinematicFreeze);
	orig_CinematicFreeze = DetourCreate(0x50075190,&my_CinematicFreeze,DETOUR_TYPE_JMP,7);

	return pv_ret;
}
void my_CinematicFreeze(bool bEnable)
{
	bool before = *(char*)0x5015D8D5 == 1 ? true : false;
	orig_CinematicFreeze(bEnable);
	bool after = *(char*)0x5015D8D5 == 1 ? true : false;

	if (before != after) {
		*(char*)0x201E7F5B = after ? 0x1 : 0x00;
	}
}
/*
	Testing if black nyc1 issue was caused by this function.
	Seems it is not.

	https://github.com/d3nd3/sof_buddy/issues/3
*/
void my_CL_UpdateSimulationTimeInfo(float extratime)
{

	static int sim_counter = 0;

	//SinglePlayer never sleeps
	if ( sim_counter % 30 == 0 ) {
		orig_CL_UpdateSimulationTimeInfo(extratime);
	}

	sim_counter += 1;
}

void my_CL_ReadPackets(void)
{
	static int sim_counter = 0;

	//SinglePlayer never sleeps
	if ( sim_counter % 30 == 0 ) {
		orig_CL_ReadPackets();
	}

	sim_counter += 1;
}

/*
	This fixes the buffer overflow crash caused by > 2048px wide resolutions whilst entering lines into console.
*/
#define MAXCMDLINE 256
void my_Con_Draw_Console(void)
{
		//jmp to here.

		int		i;
		char	*text;

		static int* edit_line = 0x20367EA4;
		static char* key_lines = 0x20365EA0;

		static int* key_linepos = 0x20365E9C;
		static int* cls_realtime = 0x201C1F0C;
		static int* con_linewidth = 0x2024AF98;
		static int* con_vislines = 0x2024AFA0;

		static int* cls_key_dest = 0x201C1F04;
		static int* cls_state = 0x201C1F00;

		static void (*ref_draw_char) (int, int, int, int) = *(int*)0x204035C8;
		
		//key_menu
		if (*cls_key_dest == 3)
			return;
		//key_console, ca_active
		if (*cls_key_dest != 1 && *cls_state == 8)
			return;		// don't draw anything (always draw if not active)

		
		text = key_lines + MAXCMDLINE * *edit_line;
		// orig_Com_Printf("Drawing %c\n",text[1]);

		//Blinking underscore animation
		text[*key_linepos] = 0x20;
		if ( (*cls_realtime>>8) & 1 ) {
			
			text[*key_linepos] = 0x5F;	
		}
		
		int maxchars = *con_linewidth > MAXCMDLINE ? MAXCMDLINE : *con_linewidth;
		
		//Fill spaces
		for (i=*key_linepos+1 ; i < maxchars; i++)
			text[i] = ' ';

		
		//con_linewidth can be larger than MAXCMDLINE.
		//key_linepos cannot be larger than 255

		
		//number of characters on line already larger or equal to that than can fit on screen
		//then we show max number of characters.
		//but can we access till the end of buffer always?

		/*
		if (*key_linepos >= *con_linewidth) {
			text += 1 + *key_linepos - *con_linewidth;
		}
		*/

		//Refer to image in google notes - text=start_pos_show_left
		int space = &text[MAXCMDLINE-1] - text;
		

		if ( space >= 0 ) {
			maxchars = space < *con_linewidth ? space : *con_linewidth;
			// orig_Com_Printf("Max Chars = %i %08X\n",maxchars,ref_draw_char);

			paletteRGBA_s palette;
			palette.c = 0xFFFFFFFF;
			/*
			//0x48 -> 0x5C[A],0x5D[B],0x5E[G],0x5F[R]
			// 0x0C -> 0x1C [R][G][B][A] -> [R][G][B][A]
			// 16 + 8 = 24
			// White font?
			char * palette = malloc(24);
			memset(palette, 0x00, 0x24);
			*(unsigned int*)(palette + 16) = 0xFFFFFFFF;
			*(unsigned int*)(palette + 20) = 0xFFFFFFFF;
			*/
			// orig_Com_Printf("Drawing %c\n",text[1]);

			//Draw_Char(int, int, int, paletteRGBA_c &)
			for (i=0 ; i<maxchars; i++) {
				// if ( i < (*con_vislines/8)-1 ) orig_Com_Printf("%i %i Drawing %c\n",maxchars,*con_vislines,text[i]);
				ref_draw_char((i+1)<<3, *con_vislines - 16, text[i],&palette);
			}
			// free(palette);

			// remove previous cursor
			text = key_lines + MAXCMDLINE * *edit_line;
			text[*key_linepos] = 0;
		}
}

/*
char *Sys_GetClipboardData( void ) {
	char *data = NULL;
	char *cliptext;

	if ( OpenClipboard( NULL ) != 0 ) {
		HANDLE hClipboardData;

		if ( ( hClipboardData = GetClipboardData( CF_TEXT ) ) != 0 ) {
			if ( ( cliptext = (char *) GlobalLock( hClipboardData ) ) != 0 ) {
				data = (char *) Z_Malloc( GlobalSize( hClipboardData ) + 1, TAG_CLIPBOARD, qfalse);
				strcpy( data, cliptext );
				GlobalUnlock( hClipboardData );
				
				strtok( data, "\n\r\b" );
			}
		}
		CloseClipboard();
	}
	return data;
}
*/

char *my_Sys_GetClipboardData( void )
{
	char *cbd;
	static void (*Z_Free)(void *pvAddress) = 0x200F9D32;
	static int* key_linepos = 0x20365E9C;
	static char* key_lines = 0x20365EA0;
	static int* edit_line = 0x20367EA4;

	if ( ( cbd = orig_Sys_GetClipboardData() ) != 0 )
	{
		int allow_to_add_len;
		strtok( cbd, "\n\r\b" );

		//excludes null char
		allow_to_add_len = strlen( cbd );

		//key_linepos points to the null character
		//but its length corresponds to the non-null characters.

		//excludes null char. (MAXCMDLINE-1) non-null characters allowed.
		if ( allow_to_add_len + *key_linepos >= MAXCMDLINE ) {

			//excluding null char.
			allow_to_add_len= (MAXCMDLINE-1) - *key_linepos;
		}
		//allow_to_add_len fits already.

		//key_linepos starts at 1 because of ] or > symbol.
		if ( allow_to_add_len > 0 )
		{
			//allow_to_add_len  is 255
			// Com_Printf("New allow_to_add_len is : %i\n",allow_to_add_len);

			//Because index 255 is the max index to hold null
			//We only store 255 characters. not 256.

			//ensure clipboard is null character ended?
			cbd[allow_to_add_len]=0; //this clamps cbd to size i+1 , i if exclude null.
			//append cbd to key_lines

			strcat( key_lines + *edit_line * MAXCMDLINE, cbd );

			//key_linepos should only be allowed to be set to 255 max.
			*key_linepos += allow_to_add_len;
		}
		Z_Free( cbd );
	}

	return cbd;
}

/*
	DllMain of WSOCK32 library
	Implicit Linking (Static Linking to the Import Library)
	the application is linked with an import library (.lib) file during the build process.
	This import library contains information about the functions and data exported by the .dll.
	When the application starts, the Windows loader automatically loads the .dll specified by the import library.
	If the .dll is not found at runtime, the application will fail to start.

	Before the application’s main entry point (e.g., main() or WinMain()) is called, 
	the loader inspects the import table of the executable to determine the required .dll files
	The loader attempts to load each .dll specified in the import table. This includes resolving the addresses 
	of the functions and variables that the application imports from the .dlls.

	Once a .dll is loaded, the loader calls the DllMain function of the .dll (if it exists) with the 
	DLL_PROCESS_ATTACH notification. This allows the .dll to perform any necessary initialization.

	Each library exports the full set of winsock functions.
	SoF.exe [Implicit]-> sof_buddy.dll [Explicit]->sof_plus.dll [Implicit]->native_winsock.dll

	Module Definition File (.def): A module definition file gives you fine-grained control over linking. 
	You can list the specific functions you want to import from a DLL:
	LIBRARY    MyProgram
	EXPORTS
	    MyFunction
	IMPORTS
	    WSOCK32.dll.bind
	    WSOCK32.dll.sendto
	    ; ... (other functions)

	Linker Options: Some linkers might have options to specify individual functions to import.
	__declspec(dllimport) is a storage-class specifier that tells the compiler that a function or object or data type is 
	defined in an external DLL.

	The function or object or data type is exported from a DLL with a corresponding __declspec(dllexport).

	spcl.dll selectively imports:
	  bind
	  sendto
	  recvfrom
	  WSAGetLastError
	  WSASetLastError
	  WSAStartup
	  inet_addr

	 I could had used implicit linking if I had known about it, so if I renamed the wsock.lib file it links to spcl.dll? weird?

	 Initialization Order: 
	   The operating system loader calls the DllMain functions of these DLLs in the order they are loaded.
	   However, this order is not always predictable and can vary depending on factors like the DLL's 
	   dependency graph and loading mechanisms.

	Even if your code has a single successful LoadLibrary call, multiple DllMain functions in other DLLs might have already executed.

	So by default, spcl DllMain is called first, when we call LoadLibrary.
	SoF.exe -> sof_buddy.dll -> spcl.dll

	Cbuf_AddText - appends to buffer
	Cbuf_Execute() called by CL_Init() and Qcommon_Init() and Qcommon_Frame()

	sp sp_sc_timers callback code is executed by Qcommon_Frame()

*/
void afterWsockInit(void)
{


	/*
		This is called by our DllMain(), thus before SoF.exe CRTmain().
		Cvars etc not allowed here. 
	*/

	/*
		cl_showfps 2 breaks cl_showfps 1 on native for some reason.
	*/


	// orig_CL_UpdateSimulationTimeInfo = DetourCreate(0x2000D6A0, &my_CL_UpdateSimulationTimeInfo,DETOUR_TYPE_JMP, 6);
	// orig_CL_ReadPackets = DetourCreate(0x2000C5C0, &my_CL_ReadPackets,DETOUR_TYPE_JMP,8);

#ifdef FEATURE_MEDIA_TIMERS
	mediaTimers_early();
#endif

#ifdef FEATURE_FONT_SCALING
	scaledFont_early();
#endif

	PrintOut(PRINT_LOG,"Before refFixes\n");
	refFixes_early();
	PrintOut(PRINT_LOG,"After refFixes\n");
	

	//orig_Qcommon_Init = DetourCreate(orig_Qcommon_Init,&my_orig_Qcommon_Init,DETOUR_TYPE_JMP,5);
	
	orig_FS_InitFilesystem = DetourCreate(0x20026980, &my_FS_InitFilesystem,DETOUR_TYPE_JMP,6);
	orig_Cbuf_AddLateCommands = DetourCreate(0x20018740,&my_Cbuf_AddLateCommands,DETOUR_TYPE_JMP,5);

	
	orig_Sys_GetGameApi = DetourCreate((void*)0x20065F20,(void*)&my_Sys_GetGameApi,DETOUR_TYPE_JMP,5);




//=================== B U G  F I X I N G =========================

	//PASTING INTO CONSOLE CRASH/OVERFLOW
	// orig_Sys_GetClipboardData = DetourCreate((void*)0x20065E60,(void*)&my_Sys_GetClipboardData,DETOUR_TYPE_JMP,10);
	WriteE8Call(0x2004BB63,&my_Sys_GetClipboardData);
	// WriteByte(0x2004BB6E,0x8C); //Skip to end, we re-implement this section.
	WriteE9Jmp(0x2004BB6C,0x2004BBFE);

	//Below causes this issue : Black Loading After Cinematic nyc1
	//https://github.com/d3nd3/sof_buddy/issues/3
	//Patch CL_Frame() to allow client frame limitting whilst in "single-player/non-dedicated" mode.
	//Ths bug was fixed with hooking void CinematicFreeze(bool bEnable) and setting cl.frame.cinematicFreeze instantly.
	WriteByte(0x2000D973,0x90);WriteByte(0x2000D974,0x90);

#if 0

//===================================================================================================
//===================================================================================================
//===================================================================================================
	//Fix a nasty overflow bug in larger resolutions related to Con_DrawInput()
	//20020D41 - 5 bytes here  no longer needed.
	/*
		LEN:5 mov     eax, con_linewidth
		LEN:3 HEX: 0x83 0xC4 0x10 ... add     esp, 10h
		LEN:1 HEX: 0x46 ... inc     esi
		LEN:3 HEX: 0x83 0xC3 0x08 ... add     ebx, 8
		LEN:2 HEX: 0x3B 0xF0 ... cmp     esi, eax        ; con.linewidth
		LEN:2 HEX: 0x7C 0xCE ... jl      short loc_20020D1F
	*/
	/*
		Lets NOP the entire area first, before we shift up. We needed the extra space to create a CMP to constant.
		5+3+1+3+2+2 = 16 bytes
	*/
	#if 1
	for (int i=0; i<16;i++) {
		WriteByte(0x20020D41+i,0x90);
	}
	// add esp, 10h
	WriteByte(0x20020D41,0x83);WriteByte(0x20020D42,0xC4);WriteByte(0x20020D43,0x10);
	//inc esi
	WriteByte(0x20020D44,0x46);
	//add ebx, 8
	WriteByte(0x20020D45,0x83);WriteByte(0x20020D46,0xC3);WriteByte(0x20020D47,0x08);
	//cmp esi, 255 - this is now 3 bytes - that is the reason for memory shift.
	WriteByte(0x20020D48,0x83);WriteByte(0x20020D49,0xFE);WriteByte(0x20020D4A,0xFF);
	//jle rel_offset
	WriteByte(0x20020D4B,0x7E);WriteByte(0x20020D4C,0xD2);
	#endif
	/*
		Address calculated from:

		Address of jle + 2:
		0x20020D4B + 2 = 0x20020D4D

		Offset:
		0x20020D1F - 0x20020D4D = -0x2E (hex)
		-0x2E in decimal = -46.

		Convert -46 to a signed 8-bit byte:
		-46 → 0xD2 (hex).
	*/
	//Repeat for other offset
	/*
		0x20020CF6
		5: 8B 0D 98 AF 24 20
		1: 40
		2: 3B C1
		2: 7C F1
	*/
	
	#if 1
	for (int i=0; i<10;i++) {
		WriteByte(0x20020CF6+i,0x90);
	}
	//inc eax
	WriteByte(0x20020CF6,0x40);
	//cmp eax, 255 - this is now 3 bytes - that is the reason for memory shift.
	WriteByte(0x20020CF7,0x83);WriteByte(0x20020CF8,0xF8);WriteByte(0x20020CF9,0xFF);
	//jle rel_offset
	WriteByte(0x20020CFA, 0x7E); WriteByte(0x20020CFB, 0xF6);
	//0xF6 Was calculated by deepseek, same as before example.
	#endif

//===================================================================================================
//===================================================================================================
//===================================================================================================
#else
	// WriteE8Call(0x2002111D,&my_Con_Draw_Console);
	WriteE9Jmp(0x2002111D,&my_Con_Draw_Console);
	WriteE9Jmp(0x20020C90, 0x20020D6C);
#endif


	/*
		So ye, there is a race condition.
		If we call this init function before our detour/patches.
		They get overriden it seems.
		WSA_Startup calls Cmd_AddCommand etc in sofplus.
		Specifically _sp_sc_on_change_CVARNAME feature.

		Ah I remember now, it was because I was fustrated that I could not modify
		any of the code that was in DllMain, because the code would execute instantly after
		LoadLibrary(), so I had no chance to edit the memory. I wanted full control, thats all.
		Kinda Naive reasoning.
	*/
	// Why do we need control of this?
	/*
	    He doesn't use detours, so they are fully compatible with detours,
	    just it calls his hook first, then mine, then mine calls orig.
	    his -> mine -> orig
		Perhaps to ensure the same order each run, just in-case? Since the order of DllMain is not guaranteed?
		I think this is not needed.
		SoF plus memory edits in DllMain
			2000F2B0 = CL_ParseServerMessage: svc_stufftext handle
			2000C216 = ConnectionlessPacket() "cmd" disabled fully, even in singleplayer.
			2000C5EA = Hook ConnectionlessPacket()
			2000C04C = Ip of ConnectionlessPacket shown in Developer Print instead of normal Print
			2001F8F6 = CL_Frame() hook
			20066D86 = VID_LoadRefresh() hook
			2006702A = R_Init hook
			20066E25 = R_Shutdown hook
			200160F5 = V_RenderView hook
			2000D857 = cl_showfps print hook
			2001610D = SCR_DrawInterface hook
			20016126 = SCR_ExecuteLayoutString hook
			2000C75D = LoadSoundModule hook

			20066413 = Sys_Millisec hook
			==Cmd_TokenizeString with macroExpand=True==
				2000C01B = CL_ConnectionlessPacket
				2001233F = CL_PredictWeapons
				200194FC = Cmd_ExecuteString
				20019549 = Cmd_ExecuteString
				2005EEC8 = SV_ConnectionlessPacket
				20063970 = SV_ExecuteClientMessage
	*/
	/*
	//The media_timers.cpp is calling 
		if ( o_sofplus ) {
			spcl_Timers();
			resetTimers(newtime);
		}
	And fully disables spcl's sys_millisecond hook.
	But this all occurs within QCommon_Init(). Very later than here.
	*/
	
	#if 0
	if ( o_sofplus ) {
		PrintOut(PRINT_LOG,"Before sofplusEntry\n");
		BOOL (*sofplusEntry)(void) = (int)o_sofplus + 0xF590;
		BOOL result = sofplusEntry();
		PrintOut(PRINT_LOG,"After sofplusEntry\n");
	}
	#endif

	PrintOut(PRINT_GOOD,"SoF Buddy fully initialised!\n");
}

/*
	Fix bug on proton:
		Proton 3.7.8
		GloriousEggProton 
	if vid_card or cpu_memory_using become 'modified'
	causes a cascade of low performance cvars to kick in. (drivers/alldefs.cfg,geforce.cfg,cpu4.cfg,memory1.cfg)
	which have very bad values in them.

	they can become modified if new hardware values differ from config.cfg

	The exact moment that hardware-changed new setup files are exec'ed (cpu_ vid_card different to config.cfg)
*/
void InitDefaults(void)
{
	/*
		This is called after loadLibrary("ref_gl.dll")
	*/
	orig_Cmd_ExecuteString("exec drivers/highest.cfg\n");
	// fix 1024 high value of fx_maxdebrisonscreen, hurts cpu.
	orig_Cmd_ExecuteString("set fx_maxdebrisonscreen 128\n");
	// fix compression as default for some gpu.
	orig_Cmd_ExecuteString("set r_isf GL_SOLID_FORMAT\n");
	orig_Cmd_ExecuteString("set r_iaf GL_ALPHA_FORMAT\n");

	PrintOut(PRINT_GOOD,"Fixed defaults\n");
}

/*
	Inside Qcommon_Init()
	After:
	Cbuf_AddEarlyCommands (false);
	Cbuf_Execute ();

	<HERE>
	default.cfg
	config.cfg
	launch_+set's
	autoexec.cfg
	launch_+cmd's

	So we are trying to create cvars with modified callback functitons _BEFORE_ config.cfg.
	Yet we can't use QCommon_Init hook because Cbuf_Init() is then not called.
*/
void my_FS_InitFilesystem(void) {
	orig_FS_InitFilesystem(); //processes cddir user etc setting dirs

	/*
		This is super early and no cvars from default.cfg or config.cfg or autoexec.cfg or +cmd  have been issued.
		So values can be overwritten easily later.

		HOWEVER:
		This is the best place to create cvars as it ensures that the callbacks are executed by every other cvar creator function hereafter.
		eg. config.cfg autoexec.cfg etc.

	*/

	//Allow PrintOut to use Com_Printf now.
	orig_Com_Printf = 0x2001C6E0;

	//Best (Earliest)(before exec config.cfg) place for cvar creation, if you want config.cfg induced triggering of modified events.
	//Idea = Cvar_Get() cvar creation with modified callback.
	//Since config.cfg represents the user's saved values. It is important they trigger the callback to set internal values.

	// FEATURE_HD_TEX FEATURE_ALT_LIGHTING etc...
	refFixes_cvars_init();

	#ifdef FEATURE_FONT_SCALING
		scaledFont_cvars_init();
	#endif

	

	//orig_Com_Printf("End FS_InitFilesystem\n");
}
/*
	A long standing bug, was related to the order of initializing cvars, which behaved different for a user without
	a config.cfg than one with one. If getting crashes, always remember this!.

	<HERE>
	default.cfg
	config.cfg
	launch_+set's
	autoexec.cfg
	launch_+cmd's
	<OR HERE>

	This is currently unused/un-needed.
*/
void my_orig_Qcommon_Init(int argc, char **argv)
{
	//Earliest Init here.

	orig_Qcommon_Init(argc,argv);

	//This is latest init? here?
}


/*
Earliest Init Location for where launch commands have fully been processed/executed.

Cbuf_AddEarlyCommands
  processes all `+set` commands first.

Later in frame...
Cbuf_AddLateCommands
  all other commands that start with + are processed.
  Ignores launch options eg. -user 

"public 1" must be set inside dedicated.cfg because dedicated.cfg overrides it? If no dedicated.cfg in user, the dedicated.cfg from pak0.pak is used.

	default.cfg
	config.cfg
	launch_+set's
	autoexec.cfg
	<HERE>
	launch_+cmd's
	<OR HERE>
	
	Maybe the purpose of this hook is so that we do stuff before the:
	Soldier of Fortune Iniitalised print line. Thats all.
*/
//this is called inside Qcommon_Init()
qboolean my_Cbuf_AddLateCommands(void)
{
	qboolean ret = orig_Cbuf_AddLateCommands();

	/*
		The use case for this should be only for handling command line cvars...
	*/

	//user can disable modern tick from launch option. +set _sofbuddy_classic 1
#ifdef FEATURE_MEDIA_TIMERS
	create_sofbuddy_cvars();
	if ( _sofbuddy_classic_timers && _sofbuddy_classic_timers->value == 0.0f ) 
		{
			PrintOut(PRINT_GOOD,"Using QPC timers!\n");
			mediaTimers_apply_afterCmdline();
		}
#endif
	
	/*
		CL_Init() already called, which handled VID_Init, and R_Init()
	*/
	return ret;
}

