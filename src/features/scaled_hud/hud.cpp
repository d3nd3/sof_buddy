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
using detour_Cvar_Set2::oCvar_Set2;
#include "../scaled_ui_base/shared.h"

#include "debug/hook_callsite.h"

#include <cstdio>
#include <math.h>
#include <string.h>

// HUD scaling function implementations

// getRealFontEnum is now defined in scaled_ui_base/sui_hooks.cpp for shared use
// This function is kept here for backward compatibility but should use the shared version

// HUD-specific global variables
// Note: Variables used by base hooks (hudScale, crosshairScale, croppedWidth, etc.) are defined in scaled_ui_base/sui_hooks.cpp
// This file only contains variables that are exclusively used by HUD feature code

// HUD scaling function implementations
/*
    This function is called internally by:
    - cInterface::DrawNum()
    - cDMRanking::Draw()
    - SCR_Crosshair()
*/

void hudscale_change(cvar_t * cvar) {
    SOFBUDDY_ASSERT(cvar != nullptr);
    float v = cvar->value;
    if (v < 0.25f) v = 0.25f;
    if (v > 8.0f) v = 8.0f;
    hudScale = roundf(v * 4.0f) / 4.0f;
    if (hudScale < 0.25f) hudScale = 0.25f;
    if (hudScale > 8.0f) hudScale = 8.0f;
    if (oCvar_Set2) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", hudScale);
        oCvar_Set2(const_cast<char*>("_sofbuddy_hud_scale_rounded"), buf, true);
    }
}

void crosshairscale_change(cvar_t * cvar) {
    SOFBUDDY_ASSERT(cvar != nullptr);
    SOFBUDDY_ASSERT(cvar->value > 0.0f);
    
    crosshairScale = roundf(cvar->value * 4.0f) / 4.0f;
    SOFBUDDY_ASSERT(crosshairScale > 0.0f);
}

#endif // FEATURE_SCALED_HUD
