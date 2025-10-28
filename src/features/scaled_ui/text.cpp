/*
	Scaled UI - Text Scaling Functions

	This file handles non-console text scaling:
	- Generic HUD/menu text vertex scaling (ref.dll R_DrawFont)
	- Menu text measurement scaling (R_Strlen/R_StrHeight)
*/

#include "feature_config.h"

#if FEATURE_UI_SCALING

#include "features.h"
#include "scaled_ui.h"
#include "util.h"
#include "hook_callsite.h"
#include "hook_callsite.h"

#include <math.h>
#include <stdint.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif

// Caller context enum for R_DrawFont callers
enum class FontCaller {
	Unknown = 0,
	ScopeCalcXY,           // 0x20006dc0 - cScope::CalcXY
	DMRankingCalcXY,       // 0x20007af0 - cDMRanking::CalcXY
	Inventory2,            // 0x20008260 - cInventory2()
	SCRDrawPause,          // 0x20013710 - SCR_DrawPause()
	SCRUpdateScreen,       // 0x200163c0 - ? Inside SCR_UpdateScreen()
	RectDrawTextItem,      // 0x200cecc0 - rect_c::DrawTextItem()
	RectDrawTextLine,      // 0x200cf0e0 - rect_c::DrawTextLine()
	TickerDraw,            // 0x200d19f0 - ticker_c::Draw()
	InputHandle,           // 0x200d4060 - input_c::Handle() ?
	FileboxHandle,        // 0x200d9f80 - filebox_c::Handle()
	LoadboxGetIndices,    // 0x200dbf10 - loadbox_c::GetIndices()
	ServerboxDraw,        // 0x200de430 - serverbox_c::Draw()
	TipRender,            // 0x200ea8a0 - tip_c::Render()
	DrawLine              // 0x30002a40 - Draw_Line
};

// Global caller context for inner glVertex calls
static FontCaller g_currentFontCaller = FontCaller::Unknown;

// Efficient mapping from function start RVA to FontCaller enum
static FontCaller getFontCallerFromRva(uint32_t fnStartRva) {
	switch (fnStartRva) {
		case 0x00006dc0: return FontCaller::ScopeCalcXY;
		case 0x00007af0: return FontCaller::DMRankingCalcXY;
		case 0x00008260: return FontCaller::Inventory2;
		case 0x00013710: return FontCaller::SCRDrawPause;
		case 0x000163c0: return FontCaller::SCRUpdateScreen;
		case 0x000cecc0: return FontCaller::RectDrawTextItem;
		case 0x000cf0e0: return FontCaller::RectDrawTextLine;
		case 0x000d19f0: return FontCaller::TickerDraw;
		case 0x000d4060: return FontCaller::InputHandle;
		case 0x000d9f80: return FontCaller::FileboxHandle;
		case 0x000dbf10: return FontCaller::LoadboxGetIndices;
		case 0x000de430: return FontCaller::ServerboxDraw;
		case 0x000ea8a0: return FontCaller::TipRender;
		case 0x00002a40: return FontCaller::DrawLine; // ref_gl.dll
		default: 
#ifdef NDEBUG
			PrintOut(PRINT_BAD, "getFontCallerFromRva: unrecognized RVA offset 0x%08X\n", fnStartRva);
			return FontCaller::Unknown;
#else
			return FontCaller::Unknown;
#endif
	}
}

// Getter for current font caller context (for inner glVertex calls)
FontCaller getCurrentFontCaller() {
	return g_currentFontCaller;
}

// Pivot for character quad when scaling font glyphs
static float pivotx;
static float pivoty;

// Font specific state
int characterIndex = 0;

/*
===========================================================================
HUD FONT + MENU FONT

This function is called internally by:
- SCR_DrawPlayerInfo() - teamicons (WE DONT SCALE)
- cScope::Draw() (WE SCALE BELOW)
- cCountdown::Draw() (WE SCALE BELOW)
- cDMRanking::Draw() (WE SCALE in R_DrawFont AND BELOW)
- cInventory2::Draw() (WE SCALE in R_DrawFont AND BELOW)
- cInfoTicker::Draw() (WE SCALE BELOW)
- cMissionStatus::Draw() (WE SCALE BELOW)
- SCR_DrawPause() (WE SCALE BELOW)
- rect_c::DrawTextItem() (WE SCALE BELOW)
- loadbox_c::Draw() (WE SCALE BELOW)
- various other menu items (WE SCALE BELOW)

Note these are not handling realignment.
If data is happy to extend left for larger text and remain stationary, no need to realign.
===========================================================================
*/
// Font Draw vertex hooks moved to text.cpp

void __stdcall my_glVertex2f_DrawFont_1(float x, float y) {
	//top-left
	//Anchor-point fixed top-left
	//When we resize images, we use center as anchor-point, might be an issue.
	pivotx = x;
	pivoty = y;

	// Skip scaling for team icons
	if (isDrawingTeamicons) {
		orig_glVertex2f(x, y);
		return;
	}

	// Handle different caller contexts
	FontCaller caller = getCurrentFontCaller();
	switch (caller) {
		case FontCaller::DMRankingCalcXY:
		case FontCaller::Inventory2:
			// HUD elements - scale by hudScale
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(hudScale-1), y);
			break;
			
		case FontCaller::ScopeCalcXY:
			// Scope text - no scaling
			orig_glVertex2f(x, y);
			break;
			
		case FontCaller::SCRDrawPause:
		case FontCaller::SCRUpdateScreen:
			// Screen overlays - scale by screen_y_scale
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
			break;
			
		case FontCaller::DrawLine:
			// Draw_Line from ref_gl.dll - no scaling
			orig_glVertex2f(x, y);
			break;
			
#ifdef UI_MENU
		case FontCaller::RectDrawTextItem:
		case FontCaller::RectDrawTextLine:
		case FontCaller::TickerDraw:
		case FontCaller::InputHandle:
		case FontCaller::FileboxHandle:
		case FontCaller::LoadboxGetIndices:
		case FontCaller::ServerboxDraw:
		case FontCaller::TipRender:
			// Menu elements - scale by screen_y_scale
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
			break;
#endif
			
		default:
			// Unknown caller - no scaling
			orig_glVertex2f(x, y);
			break;
	}
}

void __stdcall my_glVertex2f_DrawFont_2(float x, float y) {
	//top-right
	// Skip scaling for team icons
	if (isDrawingTeamicons) {
		orig_glVertex2f(x, y);
		return;
	}

	// Handle different caller contexts
	FontCaller caller = getCurrentFontCaller();
	switch (caller) {
		case FontCaller::DMRankingCalcXY:
		case FontCaller::Inventory2:
			// HUD elements - scale by hudScale
			x = pivotx + (x - pivotx) * hudScale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(hudScale-1), y);
			break;
			
		case FontCaller::ScopeCalcXY:
			// Scope text - no scaling
			orig_glVertex2f(x, y);
			break;
			
		case FontCaller::SCRDrawPause:
		case FontCaller::SCRUpdateScreen:
			// Screen overlays - scale by screen_y_scale
			x = pivotx + (x - pivotx) * screen_y_scale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
			break;
			
		case FontCaller::DrawLine:
			// Draw_Line from ref_gl.dll - no scaling
			orig_glVertex2f(x, y);
			break;
			
#ifdef UI_MENU
		case FontCaller::RectDrawTextItem:
		case FontCaller::RectDrawTextLine:
		case FontCaller::TickerDraw:
		case FontCaller::InputHandle:
		case FontCaller::FileboxHandle:
		case FontCaller::LoadboxGetIndices:
		case FontCaller::ServerboxDraw:
		case FontCaller::TipRender:
			// Menu elements - scale by screen_y_scale
			x = pivotx + (x - pivotx) * screen_y_scale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
			break;
#endif
			
		default:
			// Unknown caller - no scaling
			orig_glVertex2f(x, y);
			break;
	}
}

void __stdcall my_glVertex2f_DrawFont_3(float x, float y) {
	//bottom-right
	// Skip scaling for team icons
	if (isDrawingTeamicons) {
		orig_glVertex2f(x, y);
		return;
	}

	// Handle different caller contexts
	FontCaller caller = getCurrentFontCaller();
	switch (caller) {
		case FontCaller::DMRankingCalcXY:
		case FontCaller::Inventory2:
			// HUD elements - scale by hudScale
			x = pivotx + (x - pivotx) * hudScale;
			y = pivoty + (y - pivoty) * hudScale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(hudScale-1), y);
			break;
			
		case FontCaller::ScopeCalcXY:
			// Scope text - no scaling
			orig_glVertex2f(x, y);
			break;
			
		case FontCaller::SCRDrawPause:
		case FontCaller::SCRUpdateScreen:
			// Screen overlays - scale by screen_y_scale
			x = pivotx + (x - pivotx) * screen_y_scale;
			y = pivoty + (y - pivoty) * screen_y_scale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
			break;
			
		case FontCaller::DrawLine:
			// Draw_Line from ref_gl.dll - no scaling
			orig_glVertex2f(x, y);
			break;
			
#ifdef UI_MENU
		case FontCaller::RectDrawTextItem:
		case FontCaller::RectDrawTextLine:
		case FontCaller::TickerDraw:
		case FontCaller::InputHandle:
		case FontCaller::FileboxHandle:
		case FontCaller::LoadboxGetIndices:
		case FontCaller::ServerboxDraw:
		case FontCaller::TipRender:
			// Menu elements - scale by screen_y_scale
			x = pivotx + (x - pivotx) * screen_y_scale;
			y = pivoty + (y - pivoty) * screen_y_scale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
			break;
#endif
			
		default:
			// Unknown caller - no scaling
			orig_glVertex2f(x, y);
			break;
	}
}

void __stdcall my_glVertex2f_DrawFont_4(float x, float y) {
	//bottom-left
	// Skip scaling for team icons
	if (isDrawingTeamicons) {
		orig_glVertex2f(x, y);
		return;
	}

	// Handle different caller contexts
	FontCaller caller = getCurrentFontCaller();
	switch (caller) {
		case FontCaller::DMRankingCalcXY:
		case FontCaller::Inventory2:
			// HUD elements - scale by hudScale
			y = pivoty + (y - pivoty) * hudScale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(hudScale-1), y);
			break;
			
		case FontCaller::ScopeCalcXY:
			// Scope text - no scaling
			orig_glVertex2f(x, y);
			break;
			
		case FontCaller::SCRDrawPause:
		case FontCaller::SCRUpdateScreen:
			// Screen overlays - scale by screen_y_scale
			y = pivoty + (y - pivoty) * screen_y_scale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
			break;
			
		case FontCaller::DrawLine:
			// Draw_Line from ref_gl.dll - no scaling
			orig_glVertex2f(x, y);
			break;
			
#ifdef UI_MENU
		case FontCaller::RectDrawTextItem:
		case FontCaller::RectDrawTextLine:
		case FontCaller::TickerDraw:
		case FontCaller::InputHandle:
		case FontCaller::FileboxHandle:
		case FontCaller::LoadboxGetIndices:
		case FontCaller::ServerboxDraw:
		case FontCaller::TipRender:
			// Menu elements - scale by screen_y_scale
			y = pivoty + (y - pivoty) * screen_y_scale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
			break;
#endif
			
		default:
			// Unknown caller - no scaling
			orig_glVertex2f(x, y);
			break;
	}

	characterIndex++;
}

#ifdef UI_MENU

int hkR_Strlen(char * str, char * fontStd)
{
	int space_count = 0;
	for (char *p = str; *p != '\0'; ++p) {
		if (*p == ' ') {
			space_count++;
		}
	}

	extern int (__cdecl *oR_Strlen)(char*, char*);
	return screen_y_scale * ( oR_Strlen(str, fontStd) - 5 * space_count);
}

int hkR_StrHeight(char * fontStd)
{
	extern int (__cdecl *oR_StrHeight)(char*);
	return screen_y_scale * oR_StrHeight(fontStd);
}

#endif // UI_MENU

#include <stddef.h>

/*
    This function is called internally by:
    - SCR_DrawPlayerInfo() - teamicons (WE DONT SCALE)
    - cScope::Draw() (SCALE IN glVertex2f_DrawFont_1)
    - cCountdown::Draw() (SCALE IN glVertex2f_DrawFont_1)
    - cDMRanking::Draw() (WE SCALE BELOW)
    - cInventory2::Draw() (WE SCALE BELOW)
    - cInfoTicker::Draw() (SCALE IN glVertex2f_DrawFont_1)
    - cMissionStatus::Draw() (SCALE IN glVertex2f_DrawFont_1)
    - SCR_DrawPause() (SCALE IN glVertex2f_DrawFont_1)
    - rect_c::DrawTextItem() (SCALE IN glVertex2f_DrawFont_1)
    - loadbox_c::Draw() (SCALE IN glVertex2f_DrawFont_1)
    - various other menu items (SCALE IN glVertex2f_DrawFont_1)

    Anything text/font that is scaled must be re-aligned.
    This function is centering the dmRanking text.
    And left aligning the item/gun ui/hud text.

	0x20006dc0 - cScope::CalcXY
	0x20007af0 - cDMRanking::CalcXY
	0x20008260 - cInventory2()
	0x20013710 - SCR_DrawPause()
	0x200163c0 - ? Inside SCR_UpdateScreen()
	0x200cecc0 - rect_c::DrawTextItem()
	0x200cf0e0 - rect_c::DrawTextLine()
	0x200d19f0 - ticker_c::Draw()
	0x200d4060 - input_c::Handle() ?
	0x200d9f80 - filebox_c::Handle()
	0x200dbf10 - loadbox_c::GetIndices()
	0x200de430 - serverbox_c::Draw()
	0x200ea8a0 - tip_c::Render()
	0x30002a40 - Draw_Line
*/
void hkR_DrawFont(int screenX, int screenY, char * text, int colorPalette, char * font, bool rememberLastColor) {
	#if 1 // TEMP: RA logger for hkR_DrawFont
    uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("R_DrawFont");
    if (fnStart) {
        // Set caller context for inner glVertex calls
        g_currentFontCaller = getFontCallerFromRva(fnStart);
        // Example gating logic:
        // if (fnStart == 0x000CEF00) { /* ... */ }
    } else {
        g_currentFontCaller = FontCaller::Unknown;
    }
	#endif
	// static bool wasHudDmRanking = false;
	realFont = getRealFontEnum((char*)(* (int * )(font + 4)));
	
	if (hudDmRanking) {
		static bool scorePhase = true;

		int fontWidth = 12;
		int offsetEdge = 40;

		//Text is only drawn in:
		//Spec Chase Mode
		//Spec OFF 
		float x_scale = static_cast<float>(current_vid_w) / 640.0f;

		if (hudDmRanking_wasImage) {
			if ( * (int * ) rvaToAbsExe((void*)0x001E7E94) == 7) { //PM_SPECTATOR PM_SPECTATOR_FREEZE
				//PLAYER NAME IN SPEC CHASE MODE.

				// There is an eye and a LOGO above us, if in TEAM DM
				//Preserve their centering of text calculation.(Don't want to bother with colorCodes).
				//40 is a magic number here.
				//It uses 40 as scaling border, and the remaining is 17px.

				//SoF Formula:40 * vid_width/640 - 16
				//we do + 16 - 16 , to get the center! cancels out to + 0
				int half_text_len = screenX - (current_vid_w - (offsetEdge * x_scale));
				screenX = current_vid_w - offsetEdge * x_scale - 16 * hudScale + half_text_len*hudScale;
				// y = 20*y_scale + 32*hudScale + 6*y_scale;
				screenY = 20 * screen_y_scale + 64 * hudScale + 6 * screen_y_scale;
			} else {
				// TEAM DM PLAYING - 1 Logo above.
				//Specifically for Team Deathmatch where a logo is shown above score.
				screenX = current_vid_w - 16 * hudScale - offsetEdge * x_scale - hudScale*fontWidth * strlen(text) / 2;
				screenY = screenY + (hudScale - 1) * (32 + 3);

				if ( scorePhase) {
					scorePhase = false;
				} else 
				{
					screenY = screenY + (hudScale - 1) * (realFontSizes[realFont] + 3);
					scorePhase = true;
				}
			}

		} else {
			//PLAYING IN NON-TEAM DM, so NO LOGO
			if ( scorePhase) {
				scorePhase = false;
			} else 
			{
				screenY = screenY + (hudScale - 1) * (realFontSizes[realFont] + 3);
				scorePhase = true;
			}
			screenX = current_vid_w - offsetEdge * x_scale - 16 * hudScale - hudScale*fontWidth * strlen(text) / 2;
		}
		/*
			Score -> 0
			Ranking -> 1/1
		*/
		// wasHudDmRanking = true;
	}
	else if (hudInventoryAndAmmo) {
		//repositions/centers text in item and ammo HUD
		if ( hudInventory_wasItem ) {
			// The error here is a typo: 'test[0]' and 'test[1]' are used instead of 'text[0]' and 'text[1]'.
			// It should be 'text[0]' and 'text[1]' in the condition.
			if (text && ( (text[0] >= '0' && text[0] <= '8') || (text[0] =='9' && text[1] != 'm') ) ) {
				float set_x,set_y;
				enumCroppedDrawMode before = hudCroppedEnum;
				hudCroppedEnum = ITEM_INVEN_BOTTOM;
				//bottom-right
				drawCroppedPicVertex(false,false,set_x,set_y);
				hudCroppedEnum = before;

				screenX = set_x + -52.0f * hudScale;
				screenY = set_y + -29.0f * hudScale;
			} else {
				float set_x,set_y;
				enumCroppedDrawMode before = hudCroppedEnum;
				hudCroppedEnum = ITEM_INVEN_SWITCH;
				set_x = screenX;
				set_y = screenY;
				//top-right
				drawCroppedPicVertex(true,false,set_x,set_y);
				hudCroppedEnum = before;

				screenX = set_x + -111.0f * hudScale;
				screenY = set_y;
			}
			
			
		} else {
			if (text && ( (text[0] >= '0' && text[0] <= '8') || (text[0] =='9' && text[1] != 'M') ) ) {
				float set_x,set_y;
				enumCroppedDrawMode before = hudCroppedEnum;
				hudCroppedEnum = GUN_AMMO_BOTTOM;

				//bottom-right
				drawCroppedPicVertex(false,false,set_x,set_y);
				hudCroppedEnum = before;

				screenX = set_x + -97.0f * hudScale;
				screenY = set_y + -29.0f * hudScale;
				
			}
			else {
				//animation from screenY 433->406->433
				float set_x,set_y;
				enumCroppedDrawMode before = hudCroppedEnum;
				hudCroppedEnum = GUN_AMMO_SWITCH;
				set_x = screenX;
				set_y = screenY;
				//top-right
				drawCroppedPicVertex(true,false,set_x,set_y);
				hudCroppedEnum = before;

				screenX = set_x + -79.0f * hudScale;
				screenY = set_y;
			}
		}
	}
	oR_DrawFont(screenX, screenY, text, colorPalette, font, rememberLastColor);

	characterIndex = 0;
}

#endif // FEATURE_UI_SCALING


