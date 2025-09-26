#include "sof_buddy.h"

#include "sof_compat.h"
#include "features.h"

/*
-commands are processed elsewhere by main() program init code.
===============================================
Cbuf_AddEarlyCommands (false); run it once for getting the basedir/cddir/game/user etc
  create/set basedir/cddir/game/user etc...
  exec default.cfg - now we have basedir/cddir/game/user etc we can exec this
  exec config.cfg - now we have basedir/cddir/game/user etc we can exec this
Cbuf_AddEarlyCommands (true); apply again and clear the +set parts as processed.
  NET_Init()/WSA_Startup/SoFPlus Init/exec sofplus/sofplus.cfg
  CL_Init()/exec autoexec.cfg (autoexec overrides +set in commmand line)
Cbuf_AddLateCommands() - processes all other commands that start with + (except +set)
(alias dedicated_start)exec dedicated.cfg // (alias start)menu disclaimer
===============================================
TLDLR: Above priority startup highest to lowest:
	dedicated.cfg -----------(highest)
	+command commandlines
	autoexec.cfg
	sofplus addons
	+set commandlines
	config.cfg
	default.cfg -------------(lowest)

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
cvar_t * _sofbuddy_sleep_jitter = NULL;
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

    test = orig_Cvar_Get("test","21.1",0,0);
    test2 = orig_Cvar_Get("test2","21.1",0,0);

	// test = orig_Cvar_Get("test","0",NULL,NULL);
}
#endif

#ifdef FEATURE_MEDIA_TIMERS
/*
========MEDA_TIMERS========
*/
extern void high_priority_change(cvar_t * cvar);
extern void sleep_busyticks_change(cvar_t * cvar);
extern void sleep_jitter_change(cvar_t *cvar);
extern void sleep_change(cvar_t *cvar);

void create_mediatimers_cvars(void) {
	//meda_timers.cpp
	_sofbuddy_high_priority = orig_Cvar_Get("_sofbuddy_high_priority","1",CVAR_ARCHIVE,&high_priority_change);


	_sofbuddy_sleep = orig_Cvar_Get("_sofbuddy_sleep","1",CVAR_ARCHIVE,&sleep_change);

	_sofbuddy_sleep_jitter = orig_Cvar_Get("_sofbuddy_sleep_jitter","0", CVAR_ARCHIVE, &sleep_jitter_change);

	//how many 1ms ticks of busytick wait should use as protection from inaccurate Sleep(1) 
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

	//ui - left magfilter_ui at GL_NEAREST for now because fonts look really bad with LINEAR, even tho others might be better at 4k
	_sofbuddy_minfilter_ui = orig_Cvar_Get("_sofbuddy_minfilter_ui","GL_NEAREST",CVAR_ARCHIVE,&minfilter_change);
	_sofbuddy_magfilter_ui = orig_Cvar_Get("_sofbuddy_magfilter_ui","GL_NEAREST",CVAR_ARCHIVE,&magfilter_change);

	_sofbuddy_whiteraven_lighting = orig_Cvar_Get("_sofbuddy_whiteraven_lighting","0",CVAR_ARCHIVE,NULL);
}


#endif