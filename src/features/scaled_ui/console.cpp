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

/*
    _sofbuddy_console_size
    _sofbuddy_font_scale
    How much of the console covers the screen.
*/
void hkCon_DrawConsole(float frac) {
	extern float draw_con_frac;
	extern int* cls_state;
	
	consoleBeingRendered = true;

	//!( *cls_state != 1 && *cls_state != 8
	if (frac == 0.5 && *cls_state == 8) {
		//Console is half height thus we are connected to a server.

		//Smaller frac means fewer lines (pixel-space); larger fontScale reduces visible lines
		frac = consoleSize / fontScale;

		//adjust consoleHeight for backgroundPic
		draw_con_frac = consoleSize;

	} else {
		//While not in-game...
		//adjust rows
		//Essentially is frac = 1/fontScale, becasue fullscreen whilst disconnected.
		
		//frac is 0.5 here, when 'connecting' to server.

		//Do not adjust the console size when not in-game.
		draw_con_frac = frac;

		//frac is 0.5 or 1.0 here.
		frac = frac / fontScale;
	}

	//pass the modified frac to the original function
	oCon_DrawConsole(frac);

	consoleBeingRendered = false;
}
/*
    _sofbuddy_font_scale
    The last 4 lines of console that appears at the top of the screen in-game.
*/
void hkCon_DrawNotify(void) {
	real_refdef_width = current_vid_w;
	*viddef_width = 1 / fontScale * current_vid_w;
	
	consoleBeingRendered = true;
	
	oCon_DrawNotify();
	
	consoleBeingRendered = false;
	*viddef_width = real_refdef_width;
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
    Hook for SCR_DrawPlayerInfo() to set isDrawingTeamicons flag
    This prevents font scaling for team icons
*/
void hkSCR_DrawPlayerInfo(void) {
	isDrawingTeamicons = true;
	oSCR_DrawPlayerInfo();
	isDrawingTeamicons = false;
}

#endif // FEATURE_UI_SCALING


