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
#include <cstdio>

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

	// Auto mode is controlled by _sofbuddy_font_scale_auto; kept in sync on
	// resolution changes by scaled_ui_refresh_vid_dimensions_from_engine().
    if (fontScaleAuto) {
        apply_auto_font_scale();
        return;
    }

    SOFBUDDY_ASSERT(cvar->value > 0.0f);
    fontScale = round_scale_value(cvar->value);
    fontScale = snap_font_scale_to_glyph_grid(fontScale);
	PrintOut(PRINT_LOG,"Precise fontScale change: %.10f\n", fontScale);

	//Protect from divide by 0.
	if (fontScale == 0) {
		fontScale = 1;
	}

	SOFBUDDY_ASSERT(fontScale > 0.0f);
	if (detour_Cvar_Set2::oCvar_Set2) {
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%.2f", fontScale);
		detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_font_scale_rounded"), buf, true);
	}
}

void font_scale_auto_change(cvar_t* cvar) {
	fontScaleAuto = cvar && cvar->value != 0.0f;
	if (_sofbuddy_font_scale) fontscale_change(_sofbuddy_font_scale);
}

// Sets fontScale from the current resolution ratio when auto mode is active.
void apply_auto_font_scale(void) {
	if (!fontScaleAuto) return;
	fontScale = (screen_y_scale > 0.0f) ? screen_y_scale : 1.0f;
	if (scaleRoundAuto) fontScale = round_scale_value(fontScale);
	fontScale = snap_font_scale_to_glyph_grid(fontScale);
	SOFBUDDY_ASSERT(fontScale > 0.0f);
	if (detour_Cvar_Set2::oCvar_Set2) {
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%.2f", fontScale);
		detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_font_scale_rounded"), buf, true);
	}
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


