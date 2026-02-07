#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "debug/hook_callsite.h"
#include <string.h>

static bool fallbackCenterPrintLineMatch(int screenX, const char* text)
{
    if (!text || !*text) {
        return false;
    }
    if (!g_lastCenterPrintText[0]) {
        return false;
    }

    // Centerprint draw calls are centered in X. This gate avoids matching
    // left-anchored notify/console text that may contain similar wording.
    if (screenX < 80) {
        return false;
    }

    return strstr(g_lastCenterPrintText, text) != nullptr;
}

void hkR_DrawFont(int screenX, int screenY, char * text, int colorPalette, char * font, bool rememberLastColor, detour_R_DrawFont::tR_DrawFont original) {
    g_activeDrawCall = DrawRoutineType::Font;

    FontCaller detectedCaller = FontCaller::Unknown;
    uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("R_DrawFont");
    if (fnStart) {
        detectedCaller = getFontCallerFromRva(fnStart);
    }
    if (detectedCaller == FontCaller::Unknown && fallbackCenterPrintLineMatch(screenX, text)) {
        detectedCaller = FontCaller::SCR_DrawCenterPrint;
    }
    if (detectedCaller == FontCaller::SCR_DrawCenterPrint) {
        // Anchor Y to the top-most line of the currently active centerprint payload.
        // This avoids off-screen drift for lower-center centerprints at high font scales.
        if (g_centerPrintAnchorSeq != g_lastCenterPrintSeq) {
            g_centerPrintAnchorSeq = g_lastCenterPrintSeq;
            g_centerPrintAnchorY = static_cast<float>(screenY);
        } else if (static_cast<float>(screenY) < g_centerPrintAnchorY) {
            g_centerPrintAnchorY = static_cast<float>(screenY);
        }
    }
    
    if (g_currentFontCaller == FontCaller::Unknown && detectedCaller != FontCaller::Unknown) {
        g_currentFontCaller = detectedCaller;
    }
	
	SOFBUDDY_ASSERT(font != nullptr);
	if (font) {
		realFont = getRealFontEnum((char*)(* (int * )(font + 4)));
	} else {
		realFont = REALFONT_UNKNOWN;
	}
	
	if (g_activeRenderType == uiRenderType::HudDmRanking) {
		static bool scorePhase = true;

		int fontWidth = 12;
		int offsetEdge = 40;

		float x_scale = static_cast<float>(current_vid_w) / 640.0f;

		if (hudDmRanking_wasImage) {
			if ( * (int * ) rvaToAbsExe((void*)0x001E7E94) == 7) {
				int half_text_len = screenX - (current_vid_w - (offsetEdge * x_scale));
				screenX = current_vid_w - offsetEdge * x_scale - 16 * hudScale + half_text_len*hudScale;
				screenY = 20 * screen_y_scale + 64 * hudScale + 6 * screen_y_scale;
			} else {
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
			if ( scorePhase) {
				scorePhase = false;
			} else 
			{
				screenY = screenY + (hudScale - 1) * (realFontSizes[realFont] + 3);
				scorePhase = true;
			}
			screenX = current_vid_w - offsetEdge * x_scale - 16 * hudScale - hudScale*fontWidth * strlen(text) / 2;
		}
	}
	else if (g_activeRenderType == uiRenderType::HudInventory) {
		if ( hudInventory_wasItem ) {
			if (text && ( (text[0] >= '0' && text[0] <= '8') || (text[0] =='9' && text[1] != 'm') ) ) {
				float set_x,set_y;
				enumCroppedDrawMode before = hudCroppedEnum;
				hudCroppedEnum = ITEM_INVEN_BOTTOM;
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

				drawCroppedPicVertex(false,false,set_x,set_y);
				hudCroppedEnum = before;

				screenX = set_x + -97.0f * hudScale;
				screenY = set_y + -29.0f * hudScale;
			}
			else {
				float set_x,set_y;
				enumCroppedDrawMode before = hudCroppedEnum;
				hudCroppedEnum = GUN_AMMO_SWITCH;
				set_x = screenX;
				set_y = screenY;
				drawCroppedPicVertex(true,false,set_x,set_y);
				hudCroppedEnum = before;

				screenX = set_x + -79.0f * hudScale;
				screenY = set_y;
			}
		}
	}
	original(screenX, screenY, text, colorPalette, font, rememberLastColor);

	if (detectedCaller != FontCaller::Unknown) {
		g_currentFontCaller = FontCaller::Unknown;
	}
	characterIndex = 0;
	g_activeDrawCall = DrawRoutineType::None;
}

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
