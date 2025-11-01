/*
	Scaled UI - Console Scaling Functions

	Console font and console background scaling are handled here.
*/

#include "feature_config.h"

#if FEATURE_UI_SCALING

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "scaled_ui.h"

#include <math.h>
#include <stdint.h>

#define GL_BLEND 0x0BE2

/*
    _sofbuddy_console_size
    _sofbuddy_font_scale
    How much of the console covers the screen.
*/

// #pragma GCC push_options
// #pragma GCC optimize ("O0")
void hkCon_DrawConsole(float frac) {
	extern float draw_con_frac;
	extern int* cls_state;

	g_activeRenderType = uiRenderType::Console;

	if (frac == 0.5 && *cls_state == 8) {
		frac = consoleSize / fontScale;
		draw_con_frac = consoleSize;
	} else {
		draw_con_frac = frac;
		frac = frac / fontScale;
	}

	oCon_DrawConsole(frac);
	g_activeRenderType = uiRenderType::None;
}
// #pragma GCC pop_options
/*
    _sofbuddy_font_scale
    The last 4 lines of console that appears at the top of the screen in-game.
*/
void hkCon_DrawNotify(void) {
	real_refdef_width = current_vid_w;
	*viddef_width = 1 / fontScale * current_vid_w;
	
	g_activeRenderType = uiRenderType::Console;
	orig_glDisable(GL_BLEND);
	oCon_DrawNotify();
	
	*viddef_width = real_refdef_width;
	g_activeRenderType = uiRenderType::None;
}
/*
    _sofbuddy_font_scale
    Called by Con_Init() and SCR_DrawConsole()
*/
void hkCon_CheckResize(void) {
	//This makes con.linewidth smaller in order to reduce the character count per line.
	int viddef_before = current_vid_w;
	*viddef_width = 1 / fontScale * current_vid_w;
	
	oCon_CheckResize();
	
	*viddef_width = viddef_before;
}

/*
    Not Active
*/
void hkDraw_String_Color(int x, int y, char const * text, int length, int colorPalette) {
	//oDraw_String_Color(x, y, text, length, colorPalette);
}


// Font scaling CVar change callbacks
void fontscale_change(cvar_t * cvar) {
	static bool first_run = true;
	//round to nearest quarter
	fontScale = roundf(cvar->value * 4.0f) / 4.0f;
	PrintOut(PRINT_LOG,"Precise fontScale change: %.10f\n", fontScale);

	//Protect from divide by 0.
	if (fontScale == 0) {
		fontScale = 1;
	}

	first_run = false;
}

void consolesize_change(cvar_t * cvar) {
	// Save raw console size from cvar; apply fontScale at runtime in my_Con_DrawConsole
	consoleSize = cvar->value;
}

/*
	This is the 2d info above in-game players
    Hook for SCR_DrawPlayerInfo() to set isDrawingTeamicons flag
    This prevents font scaling for team icons
*/
void hkSCR_DrawPlayerInfo(void) {
	isDrawingTeamicons = true;
	oSCR_DrawPlayerInfo();
	isDrawingTeamicons = false;
}

#endif // FEATURE_UI_SCALING


