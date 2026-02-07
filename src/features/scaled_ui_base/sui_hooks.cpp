#include <stdint.h>
/*
	Scaled UI - Hook Registrations and Lifecycle Management
	
	This file contains:
	- Hook registrations for all scaled_ui functionality
	- Lifecycle callback implementations
	- Global variable definitions
	- Shared function implementations

    Contains 3 glVertex implementations
        - hkglVertex2f (Catch all)
        - my_glVertex2f_CroppedPic_1 (CroppedPicOptions)
        - my_glVertex2f_DrawFont_1 (DrawFont)

    Also contains a shared hook for Draw_Pic and Draw_StretchPic.
    For various things like:
      -console background and ctf ui flag carried.
      -Draw_PicCenter vs Draw_PicTiled modes.

    ==.m32 files==
        -DrawPic hook 
        -GL_FindImage Post hook - called after GL_FindImage, extracts width/height from returned image structure
*/


#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "shared_hook_manager.h"
#include "detours.h"
#include "generated_detours.h"


#include "shared.h"
// Unified active caller state impl
DrawRoutineType g_activeDrawCall = DrawRoutineType::None;
uiRenderType g_activeRenderType = uiRenderType::None;


#include "debug/hook_callsite.h"

#include "detours.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

// =============================================================================
// GLOBAL VARIABLE DEFINITIONS
// =============================================================================

StretchPicCaller g_currentStretchPicCaller = StretchPicCaller::Unknown;
PicCaller g_currentPicCaller = PicCaller::Unknown;
PicOptionsCaller g_currentPicOptionsCaller = PicOptionsCaller::Unknown;

// Mappers moved inline next to enums in shared.h for easy editing

// Shared/base variables (used by base hooks)
int DrawPicWidth = 0;
int DrawPicHeight = 0;
float screen_y_scale = 1.0f;

// Variables used by base hooks (defined here, modified by feature files)
float hudScale = 1;
float crosshairScale = 1.0f;
bool hudStretchPicCenter = false;
bool hudDmRanking = false;
bool hudDmRanking_wasImage = false;
bool hudInventoryAndAmmo = false;
bool hudInventory_wasItem = true;
bool hudHealthArmor = false;
int croppedWidth = 0;
int croppedHeight = 0;
int DrawPicPivotCenterX = 0;
int DrawPicPivotCenterY = 0;
enumCroppedDrawMode hudCroppedEnum = OTHER_UNKNOWN;
bool mainMenuBgTiled = false;

// Feature-specific variables defined in their respective feature files:
// - Console: scaled_con/console.cpp (consoleSize, isFontInner, draw_con_frac, cls_state, real_refdef_width)
// - Menu: scaled_menu/menu.cpp (isMenuSpmSettings, menuLoadboxFirstItemX, menuLoadboxFirstItemY)
// They are declared as extern in shared.h for use by base hooks

// Shared variables used by multiple features (moved from scaled_con for shared access)
float fontScale = 1;
bool isDrawingTeamicons = false;
char g_lastCenterPrintText[1024] = {0};
unsigned int g_lastCenterPrintSeq = 0;
int g_lastCenterPrintLineCount = 1;
float g_centerPrintAnchorY = 0.0f;
unsigned int g_centerPrintAnchorSeq = 0;

// Shared state enums
realFontEnum_t realFont = REALFONT_UNKNOWN;

// Font sizes for each realFontEnum_t, index by enum value
const int realFontSizes[5] = {
    12, // REALFONT_TITLE
    6,  // REALFONT_SMALL
    8,  // REALFONT_MEDIUM
    18, // REALFONT_INTERFACE
    12  // REALFONT_UNKNOWN (default to TITLE size)
};

// =============================================================================
// FUNCTION POINTERS
// =============================================================================

// Font scaling function pointers
void( * orig_Draw_Char)(int, int, int, int) = NULL;
void( * orig_Draw_String)(int a, int b, int c, int d) = NULL;
void( * orig_SRC_AddDirtyPoint)(int x, int y) = NULL;

// glVertex2f is handled specially since its address is resolved at runtime
void(__stdcall * orig_glVertex2f)(float one, float two) = NULL;
void(__stdcall * orig_glVertex2i)(int x, int y) = NULL;
void(__stdcall * orig_glDisable)(int cap) = NULL;

// Interface Draw Functions
void(__thiscall * orig_cScope_Draw)(void * self);
void(__thiscall * orig_cInventory2_And_cGunAmmo2_Draw)(void * self);
void(__thiscall * orig_cHealthArmor2_Draw)(void * self);
void(__thiscall * orig_cCountdown_Draw)(void * self);
void(__thiscall * orig_cInfoTicker_Draw)(void * self);
void(__thiscall * orig_cMissionStatus_Draw)(void * self);
void(__thiscall * orig_cDMRanking_Draw)(void * self);
void(__thiscall * orig_cCtfFlag_Draw)(void * self);
void(__thiscall * orig_cControlFlag_Draw)(void * self);

void( * orig_SCR_CenterPrint)(char * text);
void( * orig_R_Strlen)(char const * text1, char * font) = NULL;
void( * orig_Draw_PicOptions)(int, int, float, float, int, char*) = NULL;
void( * orig_Draw_CroppedPicOptions)(int, int, int, int, int, int, int, char*) = NULL;

#ifdef UI_MENU
// Menu scaling function pointers
void (__thiscall *orig_slider_c_Draw)(void * self);
void (__thiscall *orig_loadbox_c_Draw)(void * self);
void (__thiscall *orig_vbar_c_Draw)(void * self);
void (__thiscall *orig_master_Draw)(void * rect_c_self);
void (__thiscall *orig_frame_c_Constructor)(void* self, void * menu_c, char * width, char * height, void * frame_name) = NULL;
void (__thiscall *orig_stm_c_ParseBlank)(void *self_stm_c, void * toke_c);
int (__thiscall *orig_toke_c_GetNTokens)(void * toke_c, int quantity) = NULL;
char * (__thiscall *orig_toke_c_Token)(void * toke_c, int idx) = NULL;
void * (__cdecl *orig_new_std_string)(int length) = NULL;
void (*orig_DrawGetPicSize)(void * stm_c, int *, int *, char * stdPicName);
void (*orig_Draw_GetPicSize)(int *w, int *h, char *pic) = NULL;
#endif


int current_vid_w = 0;
int current_vid_h = 0;
int* viddef_width;
int* viddef_height;

// vid_checkchanges_post implementation moved to individual feature hook files

// =============================================================================
// HOOK REGISTRATIONS
// =============================================================================
// All hooks are now registered via generated_detours.h
// We override the generated hook functions with our custom implementations
// The generated code creates the detour registrations, but we override the hook functions

#ifdef UI_MENU
// Menu scaling hooks (experimental feature)
// REGISTER_HOOK(M_PushMenu, 0x200C7630, void, __cdecl, const char* name, const char* frame, bool force);
// REGISTER_HOOK_LEN(R_Strlen, 0x300042F0, 7, int, __cdecl, char * str, char * fontStd);
// REGISTER_HOOK_LEN(R_StrHeight, 0x300044C0, 7, int, __cdecl, char * fontStd);
// REGISTER_HOOK_LEN(stm_c_ParseStm, 0x200E7E70, 5, char*, __thiscall, void *self_stm_c, void * toke_c);
#endif

// =============================================================================
// SHARED RENDERING FUNCTIONS
// =============================================================================

// Shared Draw_StretchPic hook - used by both font and menu scaling
// Implementation moved to hooks/draw_stretchpic.cpp

// Shared Draw_Pic hook - used by menu scaling
// Implementation moved to hooks/draw_pic.cpp


// =============================================================================
// VERTEX MANIPULATION FUNCTIONS
// =============================================================================
/*
    Calculates the correct vertex x,y positions for each sub-texture after scaling.

     Required for HUD Vertex scaling - patch each corner of rectangle
        This function is called internally by:
            - cInventory2::Draw() (ammo, gun, item displays)
            - cHealthArmour2::Draw() (HP bar, armour bar)
*/
void drawCroppedPicVertex(bool top, bool left, float & x, float & y) {
    if (g_activeRenderType == uiRenderType::HudHealthArmor) {
        float y_scale = static_cast < float > (current_vid_h) / 480.0f;

        int center_x = current_vid_w / 2;
        float health_frame_start_x = center_x - 80.0f * hudScale;
        float health_frame_start_y = current_vid_h - 15.0f * y_scale;
        //Stealth meter takes up 10 of these 15px.
        //So if stealth meter scales with hudScale, it eats into the fixed border we wanted to preserve.

        //Does it matter? No because ample room is given for it to grow in size anyway.
        //It just limits the max hud scale before it draws offscreen.
        //Eg. at 1080p _sofbuddy_hud_scale 3 is the limit.

        float offset;

        switch (hudCroppedEnum) {
        case HEALTH_FRAME:
            if (croppedWidth == 7) {
                //red flash
                offset = 12.0f * hudScale;
                x = health_frame_start_x + offset;
                if (!left) x += 7.0f * hudScale;

                offset = 18.0f * hudScale;
                y = health_frame_start_y - offset;
                //lower
                if (!top) y += 7.0f * hudScale;
            } else {
                x = health_frame_start_x;
                if (!left) x += 155.0f * hudScale;

                //lowest point -15 from bot edge
                y = health_frame_start_y;
                if (top) y -= 27.0f * hudScale;
            }
            break;

        case HEALTH:
            offset = 26.0f * hudScale;
            x = health_frame_start_x + offset;
            if (!left) x += croppedWidth * hudScale;

            offset = 16.0f * hudScale;
            y = health_frame_start_y - offset;
            //lower
            if (!top) y += 9.0f * hudScale;
            break;

        case ARMOR:
            offset = 16.0f * hudScale;
            x = health_frame_start_x + offset;
            if (!left) x += croppedWidth * hudScale;

            offset = 27.0f * hudScale;
            y = health_frame_start_y - offset;
            //lower
            if (!top) y += 12.0f * hudScale;
            break;

        case STEALTH_FRAME:
            if ( croppedWidth == 152 ) {
                //The actual Frame
                //152x10
                x = health_frame_start_x;
                if (!left) x += 152.0f * hudScale;

                y = health_frame_start_y;
                //We need to position everything relative to this, hm.
                //bottom lower (eats into current_vid_h - 15.0f * y_scale;)
                if (!top) y += croppedHeight * hudScale;
            } else if (croppedWidth == 12 && croppedHeight == 5){
                //12x5
                //flash
                offset = 137.0f * hudScale;
                x = health_frame_start_x + offset;
                if (!left) x += croppedWidth * hudScale;

                offset = -3.0f * hudScale;
                y = health_frame_start_y - offset;
                if (!top) y += croppedHeight * hudScale;
            } else {
                //Stealth Active Gauge - dynamic width
                //115(dynamic)x4
                offset = 21.0f * hudScale;
                x = health_frame_start_x + offset;
                if (!left) x += croppedWidth * hudScale;

                offset = -3.0f * hudScale;
                y = health_frame_start_y - offset;
                if (!top) y += croppedHeight * hudScale;
            }
            
            break;
        
        default:
            // Handle unhandled enum values
            break;
        }
    } else if (g_activeRenderType == uiRenderType::HudInventory) {
        float x_scale = static_cast < float > (current_vid_w) / 640.0f;
        float y_scale = static_cast < float > (current_vid_h) / 480.0f;

        static float Y_BASE = 3.0f;

        switch (hudCroppedEnum) {
        case GUN_AMMO_BOTTOM:
            //Bottom-Right Corner - frame_bottom
            x = static_cast < float > (current_vid_w) - 13.0f * x_scale;
            if (left) x -= 128.0f * hudScale; //width
            y = static_cast < float > (current_vid_h) - Y_BASE * y_scale;
            if (top) y -= 64.0f * hudScale; //height
            break;

        case ITEM_INVEN_BOTTOM:
            // Bottom-Left Corner - frame_bottom
            x = 11.0f * x_scale;
            if (!left) x += 128.0f * hudScale; //width
            y = static_cast < float > (current_vid_h) - Y_BASE * y_scale;
            if (top) y -= 64.0f * hudScale; //height
            break;

        case GUN_AMMO_TOP:
            //Bottom-Right Corner - frame_top2 
            x = static_cast < float > (current_vid_w) - 13.0f * x_scale;
            if (left) x -= 128.0f * hudScale; //width=128
            y = static_cast < float > (current_vid_h) - Y_BASE * y_scale - 33.0f * hudScale;
            if (top) y -= 32.0f * hudScale; //height=28?
            break;

        case ITEM_INVEN_TOP:
            //Bottom-Left Corner - frame_top2
            x = 11.0f * x_scale;
            if (!left) x += 128.0f * hudScale; //width

            y = static_cast < float > (current_vid_h) - Y_BASE * y_scale - 33.0f * hudScale;
            if (top) y -= 32.0f * hudScale; //height
            break;

        case GUN_AMMO_SWITCH: {
            //Bottom-Right Corner - frame_top
            x = static_cast < float > (current_vid_w) - 13.0f * x_scale;
            if (left) x -= 128.0f * hudScale; //width=128

            // float dynamic_pos = (static_cast<float>(current_vid_h) - 7.0f - 33.0f - -9.0f ) - y; //33 - -9
            float dynamic_pos = (static_cast < float > (current_vid_h) - 7.0f - 24.0f) - y; //33 - -9
            y = static_cast < float > (current_vid_h) - Y_BASE * y_scale - 28.0f * hudScale; // 28
            if (top) y -= dynamic_pos * hudScale;
        }
        break;

        case ITEM_INVEN_SWITCH: {
            //Bottom-Left Corner - frame_top
            x = 11.0f * x_scale;
            if (!left) x += 128.0f * hudScale; //width

            // float dynamic_pos = (static_cast<float>(current_vid_h) - 7.0f - 33.0f - -9.0f ) - y;
            float dynamic_pos = (static_cast < float > (current_vid_h) - 7.0f - 24.0f) - y; //33 - -9
            y = static_cast < float > (current_vid_h) - Y_BASE * y_scale - 28.0f * hudScale;
            if (top) y -= dynamic_pos * hudScale;
        }
        break;

        case GUN_AMMO_SWITCH_LIP: {
            //Bottom-Right Corner - frame_lip
            x = static_cast < float > (current_vid_w) - 13.0f * x_scale;
            if (left) x -= 128.0f * hudScale; //width=128

            // float dynamic_pos = (static_cast<float>(current_vid_h) - 7.0f - test->value ) - y; //33 - -9
            y = static_cast < float > (current_vid_h) - Y_BASE * y_scale - 33.0f * hudScale;
            if (top) y -= 32.0f * hudScale;
        }
        break;

        case ITEM_INVEN_SWITCH_LIP: {
            //Bottom-Left Corner - frame_lip
            x = 11.0f * x_scale;
            if (!left) x += 128.0f * hudScale; //width

            // float dynamic_pos = (static_cast<float>(current_vid_h) - 7.0f - test->value ) - y; //33 - -9
            y = static_cast < float > (current_vid_h) - Y_BASE * y_scale - 33.0f * hudScale;
            if (top) y -= 32.0f * hudScale;
        }
        break;

        case ITEM_ACTIVE_LOGO: {
            // Bottom-Left Corner - frame_bottom
            x = 11.0f * x_scale + 29.0f * hudScale;
            if (!left) x += 39.0f * hudScale; //width
            y = static_cast < float > (current_vid_h) - Y_BASE * y_scale - 16.0f * hudScale;
            if (top) y -= 32.0f * hudScale; //height
        }
        break;
        
        default:
            // Handle unhandled enum values
            break;
        }
    }
}

// =============================================================================
// OPENGL VERTEX HOOKS
// =============================================================================

/*
 Required for HUD Vertex scaling - patch each corner of rectangle
        This function is called internally by:
            - cInventory2::Draw() (ammo, gun, item displays)
            - cHealthArmour2::Draw() (HP bar, armour bar)
*/
void __stdcall my_glVertex2f_CroppedPic_1(float x, float y) {
    //top-left
    drawCroppedPicVertex(true, true, x, y);
    orig_glVertex2f(x, y);
}

void __stdcall my_glVertex2f_CroppedPic_2(float x, float y) {
    //top-right
    drawCroppedPicVertex(true, false, x, y);
    orig_glVertex2f(x, y);
}

void __stdcall my_glVertex2f_CroppedPic_3(float x, float y) {
    //bottom-right
    drawCroppedPicVertex(false, false, x, y);
    orig_glVertex2f(x, y);
}

void __stdcall my_glVertex2f_CroppedPic_4(float x, float y) {
    //bottom-left
    drawCroppedPicVertex(false, true, x, y);
    orig_glVertex2f(x, y);
}



/*
===========================================================================
-Console/Notify Text
-Scoreboard
-DrawPicCenter (Draw_Pic)
    SCR_ExecuteLayoutString() (scoreboard) (X)
    SCR_Crosshair()
    SCR_UpdateScreen->pics/console/net (is D/C symbol)
    backdrop_c::Draw() (For menu crosshair))
-DrawPicTiled (Draw_Pic backdrop)
-menuLoadboxDraw
-else

Needed by:
    _sofbuddy_font_scale (console fonts)
    _sofbuddy_hud_scale (scoreboard)
    crosshair/menus rescale (Draw_Pic calls)

===========================================================================
    
*/

static inline void scaleVertexFromScreenCenter(float& x, float& y, float scale) {
    static int vertexCounter = 1;
    static float x_mid_offset;
    static float y_mid_offset;
    static float x_first_vertex;
    static float y_first_vertex;
    if (vertexCounter == 1) {
        x_mid_offset = current_vid_w * 0.5f - x;
        y_mid_offset = current_vid_h * 0.5f - y;
        x_first_vertex = x;
        y_first_vertex = y;
    }
    float final_x = x * scale - x_first_vertex * (scale - 1) - x_mid_offset * (scale - 1);
    float final_y = y * scale - y_first_vertex * (scale - 1) - y_mid_offset * (scale - 1);
    orig_glVertex2f(final_x, final_y);
    vertexCounter++;
    if (vertexCounter > 4) vertexCounter = 1;
}

static inline void scaleVertexFromCenter(float& x, float& y, float scale) {
    static int svfc_vertexCounter = 1;
    static int svfc_centerX;
    static int svfc_centerY;

    if (svfc_vertexCounter == 1) {
        svfc_centerX = x + DrawPicWidth * 0.5f;
        svfc_centerY = y + DrawPicHeight * 0.5f;

        if (DrawPicWidth == 0 || DrawPicHeight == 0) {
            PrintOut(PRINT_LOG, "[DEBUG] scaleVertexFromCenter: FATAL ERROR - DrawPicWidth or DrawPicHeight is 0!\n");
            if (orig_Com_Printf) orig_Com_Printf("FATAL ERROR: DrawPicWidth or DrawPicHeight is 0! This should not happen!\n");
            exit(1);
        }
    }

    x = svfc_centerX + (x - svfc_centerX) * scale;
    y = svfc_centerY + (y - svfc_centerY) * scale;

    orig_glVertex2f(x, y);

    svfc_vertexCounter++;
    if (svfc_vertexCounter > 4) {
        svfc_vertexCounter = 1;
        DrawPicWidth = 0;
        DrawPicHeight = 0;
    }
}

// hkglVertex2f implementation moved to hooks/glvertex2f.cpp

realFontEnum_t getRealFontEnum(const char* realFont) {
	if (!realFont) {
		return REALFONT_UNKNOWN;
	}
	
	if (strcmp(realFont, "title") == 0 || strcmp(realFont, "fonts/title") == 0) {
		return REALFONT_TITLE;
	} else if (strcmp(realFont, "small") == 0 || strcmp(realFont, "fonts/small") == 0) {
		return REALFONT_SMALL;
	} else if (strcmp(realFont, "medium") == 0 || strcmp(realFont, "fonts/medium") == 0) {
		return REALFONT_MEDIUM;
	} else if (strcmp(realFont, "interface") == 0 || strcmp(realFont, "fonts/interface") == 0) {
		return REALFONT_INTERFACE;
	}
	
	return REALFONT_UNKNOWN;
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
