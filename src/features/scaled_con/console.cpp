/*
	Scaled UI - Console Scaling Functions

	Console font and console background scaling are handled here.
*/

#include "feature_config.h"

#if FEATURE_SCALED_CON

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "generated_detours.h"
using detour_Con_DrawConsole::oCon_DrawConsole;
using detour_Con_DrawNotify::oCon_DrawNotify;
using detour_Con_CheckResize::oCon_CheckResize;
using detour_SCR_DrawPlayerInfo::oSCR_DrawPlayerInfo;
#include "../scaled_ui_base/shared.h"

#include <math.h>
#include <stdint.h>

#define GL_BLEND 0x0BE2

/*
    _sofbuddy_console_size
    _sofbuddy_font_scale
    How much of the console covers the screen.
*/

// Hook implementations moved to hooks/ directory:
// - hkCon_DrawConsole -> hooks/con_drawconsole.cpp
// - hkCon_DrawNotify -> hooks/con_drawnotify.cpp
// - hkCon_CheckResize -> hooks/con_checkresize.cpp

/*
    Not Active
*/
void hkDraw_String_Color(int x, int y, char const * text, int length, int colorPalette) {
	//oDraw_String_Color(x, y, text, length, colorPalette);
}


// Console-specific global variables
// Note: fontScale and isDrawingTeamicons are now defined in scaled_ui_base/sui_hooks.cpp for shared use
float consoleSize = 0.5;
bool isFontInner = false;
float draw_con_frac = 1.0;
int* cls_state = NULL;
int real_refdef_width = 0;

// Font scaling CVar change callbacks
void fontscale_change(cvar_t * cvar) {
	SOFBUDDY_ASSERT(cvar != nullptr);
	
	static bool first_run = true;
	//round to nearest quarter
	fontScale = roundf(cvar->value * 4.0f) / 4.0f;
	PrintOut(PRINT_LOG,"Precise fontScale change: %.10f\n", fontScale);

	//Protect from divide by 0.
	if (fontScale == 0) {
		fontScale = 1;
	}

	SOFBUDDY_ASSERT(fontScale > 0.0f);
	first_run = false;
}

void consolesize_change(cvar_t * cvar) {
	SOFBUDDY_ASSERT(cvar != nullptr);
	
	// Save raw console size from cvar; apply fontScale at runtime in my_Con_DrawConsole
	consoleSize = cvar->value;
	SOFBUDDY_ASSERT(consoleSize >= 0.0f && consoleSize <= 1.0f);
}

/*
	This is the 2d info above in-game players
    Hook for SCR_DrawPlayerInfo() to set isDrawingTeamicons flag
    This prevents font scaling for team icons
*/
// Implementation moved to hooks/scr_drawplayerinfo.cpp

#endif // FEATURE_SCALED_CON


