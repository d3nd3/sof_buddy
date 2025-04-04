#include "sof_buddy.h"

#include "sof_compat.h"
#include "features.h"

//Every cvar here would trigger its modified, because no cvars exist prior.
//But its loaded with its default value.
/*
  Cvar_Get() - Used for 'Creating Cvar'
  Cvar_Set2() - Used for 'Setting Cvar'
  
	Cvar_Get -> If the variable already exists, the value will not be set
	This is earlier than Cbuf_AddLateCommands(), just before exec default.cfg and exec config.cfg IN Qcommon_Init()

	Cvar flags are or'ed in, thus multiple calls to Cvar_Get stack the flags, but doesn't remove flags.
	Does the command with NULL, remove the command or create multiple?
	  NULL does not remove previous set modified callbacks.
	  With a new modified callback, it overrides the currently set one.

	 If you want your cvar's onChange callback to be triggered by config.cfg (which sets cvars).
	 They have to be first created here, thus it would have 2 calls to the callback, if the config.cfg contains a different value,
	 than the default we supply here.
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
void create_scaled_fonts_cvars(void) {
	//scaled_font.cpp
	//0.35 is nice value.
	_sofbuddy_console_size = orig_Cvar_Get("_sofbuddy_console_size","0.5",CVAR_ARCHIVE,&consolesize_change);
	//fontscale_change references a cvar, so order matters.
	_sofbuddy_font_scale = orig_Cvar_Get("_sofbuddy_font_scale","1",CVAR_ARCHIVE,&fontscale_change);
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
	_sofbuddy_sleep = orig_Cvar_Get("_sofbuddy_sleep","1",CVAR_ARCHIVE,&sleep_change);

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

	//ui
	_sofbuddy_minfilter_ui = orig_Cvar_Get("_sofbuddy_minfilter_ui","GL_NEAREST",CVAR_ARCHIVE,&minfilter_change);
	_sofbuddy_magfilter_ui = orig_Cvar_Get("_sofbuddy_magfilter_ui","GL_NEAREST",CVAR_ARCHIVE,&magfilter_change);

	_sofbuddy_whiteraven_lighting = orig_Cvar_Get("_sofbuddy_whiteraven_lighting","0",CVAR_ARCHIVE,&whiteraven_change);
}
#endif