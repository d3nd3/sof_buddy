/*
	Scaled UI - HUD Scaling Functions
	
	Contains:
        hkDraw_PicOptions
            - can position AND scale using parameter to the fn.
            - dmRanking sprites / special "interface" font scaled

        hkDraw_CroppedPicOptions
            - identifies textures from a hud type
            - sets enum from .m32 pathname
            - stores width/height
            - the glVertex specific hook applies the scaling deeper

        hkR_DrawFont
            - realigns/positions HUD text for scaling.
            - eg. dmRanking Names, position
            - ammunition , name of weapon

        hkcInventory2_And_cGunAmmo2_Draw
            - identification of the ammo and gun ui's (2 corners)

        hkcHealthArmor2_Draw
            - identification of the health and armour bar

        hkcDMRanking_Draw
            - identification of the dmRanking sprites

        hkcCtfFlag_Draw
            - identification of the ctf flag being carried sprite

        hkSCR_ExecuteLayoutString
            - identification of the scoreboard
*/

#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include <stdint.h>
#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "shared_hook_manager.h"
#include "detours.h"
#include "generated_detours.h"
using detour_Draw_PicOptions::oDraw_PicOptions;
using detour_Draw_CroppedPicOptions::oDraw_CroppedPicOptions;
using detour_cInventory2_And_cGunAmmo2_Draw::ocInventory2_And_cGunAmmo2_Draw;
using detour_cHealthArmor2_Draw::ocHealthArmor2_Draw;
using detour_cDMRanking_Draw::ocDMRanking_Draw;
using detour_cCtfFlag_Draw::ocCtfFlag_Draw;
using detour_SCR_ExecuteLayoutString::oSCR_ExecuteLayoutString;
#include "../scaled_ui_base/shared.h"

#include "debug/hook_callsite.h"

#include <math.h>
#include <string.h>

// HUD scaling function implementations

realFontEnum_t getRealFontEnum(const char* realFont) {
    if (!realFont) return REALFONT_UNKNOWN;
    if (!strcmp(realFont, "title")) return REALFONT_TITLE;
    if (!strcmp(realFont, "small")) return REALFONT_SMALL;
    if (!strcmp(realFont, "medium")) return REALFONT_MEDIUM;
    if (!strcmp(realFont, "interface")) return REALFONT_INTERFACE;
    return REALFONT_UNKNOWN;
}

// HUD scaling function implementations
/*
    This function is called internally by:
    - cInterface::DrawNum()
    - cDMRanking::Draw()
    - SCR_Crosshair()
*/

void hudscale_change(cvar_t * cvar) {
    //round to nearest quarter
    hudScale = roundf(cvar->value * 4.0f) / 4.0f;
}

void crosshairscale_change(cvar_t * cvar) {
    crosshairScale = roundf(cvar->value * 4.0f) / 4.0f;
}

#endif // FEATURE_SCALED_HUD
