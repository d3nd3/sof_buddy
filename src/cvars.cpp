#include "sof_buddy.h"

#include "sof_compat.h"
#include "features.h"

/*
	==UPDATED NOTES==
	(IF MODIFIED==TRUE : Cvar_Get() will fire callback else not. (Unless its creating new cvar))
	Cvar_Get() - (Only sets modified=1 upon Creation).
     --2 PATHWAYS.--
     OnCreation : 
          //fires the callback unless NULL for callback in fn call.
          if ( cvar->callback ) fire callback()

     OnExist : 
          //fires the callback if callback arg AND modified == 1
         if (cvar->modified && cvar->callback)
              fire callback();

     This means Cvar_Get depends on modified being true, to fire its callback. ( if the cvar already exists)



Cvar_Set2() - force=0 force=1 force=>1
     Will call Cvar_Get() and return, if the var is not existing already. (calls Cvar_Get() in creation mode/pathway).
     won't set the var->value(float) if CVAR_INTERNAL

    //fires the callback if the cvar has a callback associated to it, and if the value is changed.
    if ( !strcmp(newval,oldval)) {
        if ( cvar->callback ) fire callback()
    }
     
     if force >1 , also force CVAR_INTERNAL's to be set. (Warning: don't use for latched cvars, memory leak).

    server CVAR_LATCH - just sets cvar->latched_string
    client CVAR_LATCH - (protected with CVAR_INTERNAL) (specific for game cvar). - bypasses modified and userinfo_modified (Also calls UpdateViolence here).

  ----------------------

  Cvar_Get() - Used for 'Creating Cvar'
  Cvar_Set2() - Used for 'Setting Cvar'
  
	Cvar_Get -> If the variable already exists, the value will not be set
	
	==Cvar_Get Quirks==
	--flags--
		Cvar flags are or'ed in, thus multiple calls to Cvar_Get stack the flags, but doesn't remove flags.
	--NULL callback argument--
		Does the command with NULL, remove the command or create multiple?
	  	NULL does not remove previous set modified callbacks.
	  	With a new modified callback, it overrides the currently set one.

	--Cvar already exists--
		If the cvar already exists when calling Cvar_Get(), the callback is still fired if the value has been modified since.
		I assume this needs the modified value to be reset to false, not sure how that happens, or if it does. 

	 If you want your cvar's onChange callback to be triggered by config.cfg (which sets cvars).
	 They have to be first created before config.cfg, thus it would have 2 calls to the callback, if the config.cfg contains a different value,
	 than the default we supply here.

	Modified callback is called _WHEN_:
		Upon Cvar Creation in Cvar_Get()
			If the Cvar already exists, it checks it has a modified=true AND a callback, then fires callback()
			If the Cvar didn't exist, it checks ONLY if it has a callback, then fires callback()

		Upon Cvar change in Cvar_Set2()
		  If the value being set is DIFFERENT than current value _AND_ has a callback, then fires callback()


	IMPORTANT:
	  A cvar's modified function cannot use the global pointer because it hasnt' returned yet...

	For cvar creation which requires to change other config.cfg cvars
	By definition cvars which change other cvars cannot both have CVAR_ARCHIVE flags
	Because then the order of them would matter inside the config.cfg (Race Condition).

	SO: EVERY CVAR_ARCHIVE CVAR MUST NOT SHARE IMPLEMENTATION VARIABLE MAPPINGS, ELSE THEY CONFLICT WITH THEMSELVES,
	THEY'RE ESSENTIALLY ALIASES FOR EACH OTHER, WHICH IS INVALID.


	==Understanding cvar Race conditions==
		Qcommon_Init()

			//We do it twice incase a basedir or cddir etc are set
			//To change behaviour of exec config.cfg etc...

			//only process commandline "+set" commands
			Cbuf_AddEarlyCommands (false);
			Cbuf_Execute ();

			//Process some launch options: -user -cddir -basedir -game
			FS_InitFileSystem():

			Cbuf_AddText ("exec default.cfg\n");
			Cbuf_AddText ("exec config.cfg\n");

			//Repeat the previous +set commands to override anything set by the config.cfg or default.cfg
			//only process commandline "+set" commands
			Cbuf_AddEarlyCommands (true);
			Cbuf_Execute ();

			//Exec autoexec.cfg
		  CL_Init()
		    FS_ExecAutoexec()

			//Add all of the other commands with +CMD which are not +set and not -user etc (launch options)
		  Cbuf_AddLateCommands()


		TLDR:
			default.cfg
			config.cfg
			launch_+set's
			autoexec.cfg
			launch_+cmd's

		
*/


/*
_sofbuddy_classic_timers
_sofbuddy_high_priority
_sofbuddy_sleep
_sofbuddy_sleep_gamma
_sofbuddy_sleep_busyticks
_sofbuddy_lightblend_src
_sofbuddy_lightblend_dst
_sofbuddy_minfilter_unmipped
_sofbuddy_magfilter_unmipped
_sofbuddy_minfilter_mipped
_sofbuddy_magfilter_mipped
_sofbuddy_minfilter_ui
_sofbuddy_magfilter_ui
_sofbuddy_whiteraven_lighting
_sofbuddy_font_scale;
_sofbuddy_console_size;

16.
*/

//sof_buddy.cpp
cvar_t * _sofbuddy_classic_timers = NULL;

//meda_timers.cpp
cvar_t * _sofbuddy_high_priority = NULL;
cvar_t * _sofbuddy_sleep = NULL;
cvar_t * _sofbuddy_sleep_gamma = NULL;
cvar_t * _sofbuddy_sleep_busyticks = NULL;
cvar_t * _sp_cl_cpu_cool = NULL;
cvar_t * _sp_cl_frame_delay = NULL;

//ref_fixes.cpp
cvar_t * _sofbuddy_lightblend_src = NULL;
cvar_t * _sofbuddy_lightblend_dst = NULL;

cvar_t * _sofbuddy_minfilter_unmipped = NULL;
cvar_t * _sofbuddy_magfilter_unmipped = NULL;
cvar_t * _sofbuddy_minfilter_mipped = NULL;
cvar_t * _sofbuddy_magfilter_mipped = NULL;
cvar_t * _sofbuddy_minfilter_ui = NULL;
cvar_t * _sofbuddy_magfilter_ui = NULL;

cvar_t * _sofbuddy_whiteraven_lighting = NULL;

//scaled_font.cpp
cvar_t * _sofbuddy_font_scale = NULL;
cvar_t * _sofbuddy_console_size = NULL;
cvar_t * _sofbuddy_hud_scale = NULL;

cvar_t * con_maxlines = NULL;
cvar_t * con_buffersize = NULL;

cvar_t * test = NULL;
cvar_t * test2 = NULL;


#ifdef FEATURE_MEDIA_TIMERS
/*
========MAIN========
*/
void create_sofbuddy_cvars(void) {
	//sof_buddy.cpp
	_sofbuddy_classic_timers = orig_Cvar_Get("_sofbuddy_classic_timers","0",CVAR_NOSET,NULL);
}
#endif

#ifdef FEATURE_FONT_SCALING
/*
========SCALED FONTS========
*/
extern void consolesize_change(cvar_t * cvar);
extern void fontscale_change(cvar_t * cvar);
extern void hudscale_change(cvar_t * cvar);

void create_scaled_fonts_cvars(void) {
	//scaled_font.cpp
	//0.35 is nice value.
	_sofbuddy_console_size = orig_Cvar_Get("_sofbuddy_console_size","0.5",CVAR_ARCHIVE,&consolesize_change);
	//fontscale_change references a cvar, so order matters.
	_sofbuddy_font_scale = orig_Cvar_Get("_sofbuddy_font_scale","1",CVAR_ARCHIVE,&fontscale_change);

	_sofbuddy_hud_scale = orig_Cvar_Get("_sofbuddy_hud_scale","1",CVAR_ARCHIVE,&hudscale_change);

	con_maxlines = orig_Cvar_Get("con_maxlines","512",CVAR_ARCHIVE,NULL);
	con_buffersize = orig_Cvar_Get("con_buffersize","32768",CVAR_ARCHIVE,NULL);

	test = orig_Cvar_Get("test","21.1",NULL,NULL);
	test2 = orig_Cvar_Get("test2","21.1",NULL,NULL);

	// test = orig_Cvar_Get("test","0",NULL,NULL);
}
#endif

#ifdef FEATURE_MEDIA_TIMERS
/*
========MEDA_TIMERS========
*/
extern void high_priority_change(cvar_t * cvar);
extern void sleep_busyticks_change(cvar_t * cvar);
extern void sleep_gamma_change(cvar_t *cvar);
extern void sleep_change(cvar_t *cvar);

void create_mediatimers_cvars(void) {
	//meda_timers.cpp
	_sofbuddy_high_priority = orig_Cvar_Get("_sofbuddy_high_priority","1",CVAR_ARCHIVE,&high_priority_change);

	//this feature is too experimental and causes a wonky frequency*msec pairing, breaking
	//the law of : frequency*msec = 1000
	//so if the system is bottlenecked it causes speeding players.
	//we leave at default 0 for now. TODO: FIX THIS - do more proper.
	_sofbuddy_sleep = orig_Cvar_Get("_sofbuddy_sleep","0",CVAR_ARCHIVE,&sleep_change);

	//frametimeMS - maxRendertimeMS = remaining after render time = spare time.
	// provide fps of max system
	//fast system means larger values (more space in next frame to occupy)
	//slow system means smaller values (less space in next frame to occupy)

	//frametimeMS-7 ... (takes 7ms to renderFrameCode)
	//frametimeMS-6 ... (takes 6ms to renderFrameCode)
	//frametimeMS-5 ... (takes 5ms to renderFrameCode)
	//frametimeMS-4 ... (takes 4ms to renderFrameCode)
	//frametimeMS-3 ... (takes 3ms to renderFrameCode)
	//frametimeMS-2 ... (takes 2ms to renderFrameCode)
	//frametimeMS-1 ... (takes 1ms to renderFrameCode)
	//5 = slow, 2 = fast
	//how many 'spare ticks' in each frame to use for overflow/eating
	//Most of the time this cvar does nothing - becuase system is not lagging
	//More appropriate on struggling systems.
	//This cvar requires you to specify your max uncapped fps to calculate spare time.
	//Put low value eg. 140 == 7 == Never eat
	_sofbuddy_sleep_gamma = orig_Cvar_Get("_sofbuddy_sleep_gamma","300", CVAR_ARCHIVE, &sleep_gamma_change);


	//0 = messy ending to each frame = stutter = but lowest cpu usage
	//1 = lowest cpu usage without stutter
	//2 = less stutter when system overloaded - cost of more cpu (try this if too much lag)
	_sofbuddy_sleep_busyticks = orig_Cvar_Get("_sofbuddy_sleep_busyticks","2", CVAR_ARCHIVE, &sleep_busyticks_change);

	_sp_cl_cpu_cool = orig_Cvar_Get("_sp_cl_cpu_cool","0",NULL,NULL);
	_sp_cl_frame_delay = orig_Cvar_Get("_sp_cl_frame_delay","0",NULL,NULL);
}
#endif

#ifdef FEATURE_ALT_LIGHTING
/*
========REF_FIXES========
*/
extern void lightblend_change(cvar_t * cvar);
extern void minfilter_change(cvar_t * cvar);
extern void magfilter_change(cvar_t * cvar);
extern void whiteraven_change(cvar_t* cvar);

void create_reffixes_cvars(void) {
	//ref_fixes.cpp
	_sofbuddy_lightblend_src = orig_Cvar_Get("_sofbuddy_lightblend_src","GL_ZERO",CVAR_ARCHIVE,&lightblend_change);
	_sofbuddy_lightblend_dst = orig_Cvar_Get("_sofbuddy_lightblend_dst","GL_SRC_COLOR",CVAR_ARCHIVE,&lightblend_change);

	//sky detailtextures (unmipped)
	_sofbuddy_minfilter_unmipped = orig_Cvar_Get("_sofbuddy_minfilter_unmipped","GL_LINEAR",CVAR_ARCHIVE,&minfilter_change);
	_sofbuddy_magfilter_unmipped = orig_Cvar_Get("_sofbuddy_magfilter_unmipped","GL_LINEAR",CVAR_ARCHIVE,&magfilter_change);

	//mipped textures
	//Important ones - obtain a value from both mipmaps and take the weighted average between the 2 values - sample each mipmap by using 4 neighbours of each.
	_sofbuddy_minfilter_mipped = orig_Cvar_Get("_sofbuddy_minfilter_mipped","GL_LINEAR_MIPMAP_LINEAR",CVAR_ARCHIVE,&minfilter_change);
	_sofbuddy_magfilter_mipped = orig_Cvar_Get("_sofbuddy_magfilter_mipped","GL_LINEAR",CVAR_ARCHIVE,&magfilter_change);

	//ui - left magfilter_ui at GL_NEAREST for now because fonts look really bad with LINEAR, even tho others might be better at 4k
	_sofbuddy_minfilter_ui = orig_Cvar_Get("_sofbuddy_minfilter_ui","GL_NEAREST",CVAR_ARCHIVE,&minfilter_change);
	_sofbuddy_magfilter_ui = orig_Cvar_Get("_sofbuddy_magfilter_ui","GL_NEAREST",CVAR_ARCHIVE,&magfilter_change);

	_sofbuddy_whiteraven_lighting = orig_Cvar_Get("_sofbuddy_whiteraven_lighting","0",CVAR_ARCHIVE,NULL);
}


#endif