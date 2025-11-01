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
        -GL_FindImage hook - called before Draw_Pic, work together to get the width height of 
        the image, and it gets centered based on its size and position.
*/


#include "feature_config.h"

#if FEATURE_UI_SCALING

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "shared_hook_manager.h"
#include "feature_macro.h"
#include "scaled_ui.h"
// Unified active caller state impl
DrawRoutineType g_activeDrawCall = DrawRoutineType::None;
uiRenderType g_activeRenderType = uiRenderType::None;


#include "hook_callsite.h"

#include "DetourXS/detourxs.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

// =============================================================================
// GLOBAL VARIABLE DEFINITIONS
// =============================================================================

static StretchPicCaller g_currentStretchPicCaller = StretchPicCaller::Unknown;
static PicCaller g_currentPicCaller = PicCaller::Unknown;
PicOptionsCaller g_currentPicOptionsCaller = PicOptionsCaller::Unknown;

// Mappers moved inline next to enums in scaled_ui.h for easy editing

float fontScale = 1;
float consoleSize = 0.5;
bool isFontInner = false;
bool isDrawingTeamicons = false;

float hudScale = 1;
float crosshairScale = 1.0f;
bool hudStretchPicCenter = false;
bool hudDmRanking = false;
bool hudDmRanking_wasImage = false;
bool hudInventoryAndAmmo = false;
bool hudInventory_wasItem = true;
bool hudHealthArmor = false;

int DrawPicWidth = 0;
int DrawPicHeight = 0;
bool isMenuSpmSettings = false;
bool mainMenuBgTiled = false;

float screen_y_scale = 1.0f;
int real_refdef_width = 0;

int menuLoadboxFirstItemX;
int menuLoadboxFirstItemY;

// HUD specific variables
int croppedWidth = 0;
int croppedHeight = 0;
int DrawPicPivotCenterX = 0;
int DrawPicPivotCenterY = 0;

// Font specific variables
float draw_con_frac = 1.0;
int* cls_state;

// HUD enums
enumCroppedDrawMode hudCroppedEnum = OTHER_UNKNOWN;
realFontEnum_t realFont = REALFONT_UNKNOWN;

// Font sizes for each realFontEnum_t, index by enum value
const int realFontSizes[4] = {
    12, // REALFONT_TITLE
    6,  // REALFONT_SMALL
    8,  // REALFONT_MEDIUM
    18  // REALFONT_INTERFACE
};

// =============================================================================
// FUNCTION POINTERS
// =============================================================================

// Font scaling function pointers
void( * orig_Draw_Char)(int, int, int, int) = NULL;
void( * orig_Draw_String)(int a, int b, int c, int d) = NULL;
void( * orig_Con_Initialize)(void) = NULL;
void( * orig_SRC_AddDirtyPoint)(int x, int y) = NULL;
void( * orig_SCR_DirtyRect)(int x1, int y1, int x2, int y2) = NULL;

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

// =============================================================================
// LIFECYCLE CALLBACK REGISTRATIONS
// =============================================================================

// Lifecycle callback registration placed at the top for visibility
static void scaledUI_EarlyStartup(void);
static void scaledUI_RefDllLoaded(void);

//We create cvars here, because we want cvar to exist before the call the Con_Init
void my_Con_Init(void);

REGISTER_SHARED_HOOK_CALLBACK(EarlyStartup, scaled_ui, scaledUI_EarlyStartup, 70, Post);
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, scaled_ui, scaledUI_RefDllLoaded, 60, Post);

// =============================================================================
// HOOK REGISTRATIONS
// =============================================================================

REGISTER_HOOK_VOID(Con_DrawNotify, (void*)0x00020D70, SofExe, void, __cdecl);
REGISTER_HOOK(Con_DrawConsole, (void*)0x00020F90, SofExe, void, __cdecl, float frac);
REGISTER_HOOK_VOID(SCR_DrawPlayerInfo, (void*)0x00015B10, SofExe, void, __cdecl);
REGISTER_HOOK_VOID(Con_CheckResize, (void*)0x00020880, SofExe, void, __cdecl);
REGISTER_HOOK_VOID(Con_Init, (void*)0x000208E0, SofExe, void, __cdecl);
REGISTER_HOOK(SCR_ExecuteLayoutString, (void*)0x00014510, SofExe, void, __cdecl, char* text);

REGISTER_HOOK(cInventory2_And_cGunAmmo2_Draw, (void*)0x00008430, SofExe, void, __thiscall, void* self);
REGISTER_HOOK(cHealthArmor2_Draw, (void*)0x00008C60, SofExe, void, __thiscall, void* self);
REGISTER_HOOK(cDMRanking_Draw, (void*)0x00007B30, SofExe, void, __thiscall, void* self);
REGISTER_HOOK(cCtfFlag_Draw, (void*)0x00006920, SofExe, void, __thiscall, void* self);

REGISTER_HOOK_LEN(Draw_StretchPic, (void*)0x00001D10, RefDll, 5, void, __cdecl, int x, int y, int w, int h, int palette, char * name, int flags);
REGISTER_HOOK_LEN(Draw_Pic, (void*)0x00001ED0, RefDll, 7, void, __cdecl, int x, int y, char const * imgname, int palette);
REGISTER_HOOK_LEN(GL_FindImage, (void*)0x00007380, RefDll, 6, void*, __cdecl, char *filename, int imagetype, char mimap, char allowPicmip);
REGISTER_HOOK_LEN(Draw_PicOptions, (void*)0x00002080, RefDll, 6, void, __cdecl, int x, int y, float w_scale, float h_scale, int palette, char * name);
REGISTER_HOOK_LEN(Draw_CroppedPicOptions, (void*)0x00002240, RefDll, 5, void, __cdecl, int x, int y, int c1x, int c1y, int c2x, int c2y, int palette, char * name);
REGISTER_HOOK_LEN(R_DrawFont, (void*)0x000045B0, RefDll, 6, void, __cdecl, int screenX, int screenY, char * text, int colorPalette, char * font, bool rememberLastColor);
// REGISTER_HOOK_LEN(Draw_String_Color, 0x30001A40, 5, void, __cdecl, int x, int y, char const * text, int length, int colorPalette);

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
/*
    This function is called internally by:
    - cCtfFlag::Draw()
    - cControlFlag::Draw()
    - cScope::Draw()
    - SCR_ExecuteLayoutString()
    - CON_DrawConsole()
    - loadbox_c::Draw()
    - vbar_c::Draw()
    - SCR_UpdateScreen->pics/monitor/monitor
*/
// #pragma GCC push_options
// #pragma GCC optimize ("O0")
void hkDraw_StretchPic(int x, int y, int w, int h, int palette, char * name, int flags) {
    g_activeDrawCall = DrawRoutineType::StretchPic;
    StretchPicCaller detectedCaller = StretchPicCaller::Unknown;

    uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_StretchPic");
    if (fnStart) {
        detectedCaller = getStretchPicCallerFromRva(fnStart);
    }
    
    if (g_currentStretchPicCaller == StretchPicCaller::Unknown && detectedCaller != StretchPicCaller::Unknown) {
        g_currentStretchPicCaller = detectedCaller;
    }
    
    StretchPicCaller caller = g_currentStretchPicCaller;
    
    if (caller == StretchPicCaller::CON_DrawConsole) {
        //Draw the console background with new height
        extern float draw_con_frac;
        int consoleHeight = draw_con_frac * current_vid_h;
        y = -1 * current_vid_h + consoleHeight;
        oDraw_StretchPic(x, y, w, h, palette, name, flags);
        orig_SRC_AddDirtyPoint(0, 0);
        orig_SRC_AddDirtyPoint(w - 1, consoleHeight - 1);
        g_currentStretchPicCaller = StretchPicCaller::Unknown;
        g_activeDrawCall = DrawRoutineType::None;
 
        return;
    }
    
    if (g_activeRenderType == uiRenderType::HudCtfFlag) {
        w = w * hudScale;
        h = h * hudScale;
    }

    oDraw_StretchPic(x, y, w, h, palette, name, flags);
    

    g_currentStretchPicCaller = StretchPicCaller::Unknown;
    g_activeDrawCall = DrawRoutineType::None;

}
// #pragma GCC pop_options

// Shared Draw_Pic hook - used by menu scaling
/*
    This function is called internally by:
    - SCR_ExecuteLayoutString() (scoreboard)
    - SCR_Crosshair()
    - SCR_UpdateScreen->pics/console/net
    - backdrop_c::Draw() (For menu crosshair)
*/

void hkDraw_Pic(int x, int y, char const * imgname, int palette) {
    g_activeDrawCall = DrawRoutineType::Pic;
    PicCaller detectedCaller = PicCaller::Unknown;
    uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_Pic");
    if (fnStart) {
        detectedCaller = getPicCallerFromRva(fnStart);
    }
    
    if (g_currentPicCaller == PicCaller::Unknown && detectedCaller != PicCaller::Unknown) {
        g_currentPicCaller = detectedCaller;
    }
    
    if (imgname != nullptr) {
        const char* p = imgname;
        int slashes = 0;
        while (*p && slashes < 2) {
            if (*p++ == '/') slashes++;
        }
        if (slashes == 2 && *p) {
            if (p[0] == 'b' && p[1] == 'a' && p[2] == 'c' && p[3] == 'k' &&
                p[4] == 'd' && p[5] == 'o' && p[6] == 'p' && p[7] == 'e' &&
                p[8] == '/') {
                p += 9;
                if (p[0] == 'w' && p[1] == 'e' && p[2] == 'b' && p[3] == '_' &&
                    p[4] == 'b' && p[5] == 'g' && (p[6] == '.' || p[6] == '\0' || p[6] == '/')) {
                    mainMenuBgTiled = true;
                    oDraw_Pic(x, y, imgname, palette);
                    mainMenuBgTiled = false;
                    g_currentPicCaller = PicCaller::Unknown;
                    g_activeDrawCall = DrawRoutineType::None;
                    return;
                }
            }
        }
    }

    oDraw_Pic(x, y, imgname, palette);
    g_currentPicCaller = PicCaller::Unknown;
    g_activeDrawCall = DrawRoutineType::None;

}

/*
    Shared GL_FindImage hook - used by menu scaling
    Any m32 that is loaded with GL_FindImage will be scaled here.
    We should be careful this is only used for 2d sprites etc.
    
    Note: Filename format for crosshairs is "pics/menus/status/ch0" through "pics/menus/status/ch4"
    (no .m32 extension in the filename parameter).
*/

void* hkGL_FindImage(char *filename, int imagetype, char mimap, char allowPicmip) {

    
    //uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("GL_FindImage");
    
    void * image_t = oGL_FindImage(filename, imagetype, mimap, allowPicmip);
    
    if (image_t) {
        #ifdef UI_MENU
        if (g_currentPicCaller == PicCaller::Crosshair || g_currentPicCaller == PicCaller::ExecuteLayoutString ) {
            DrawPicWidth = *(short*)((char*)image_t + 0x44);
            DrawPicHeight = *(short*)((char*)image_t + 0x46);
        } else 
        #endif
        if ( g_activeDrawCall == DrawRoutineType::Pic) {
            if (mainMenuBgTiled) {
                DrawPicWidth = *(short*)((char*)image_t + 0x44) * screen_y_scale;
                DrawPicHeight = *(short*)((char*)image_t + 0x46) * screen_y_scale;
            } else {
                
                DrawPicWidth = *(short*)((char*)image_t + 0x44);
                DrawPicHeight = *(short*)((char*)image_t + 0x46);
            }
        }
    }
    
    return image_t;
}

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
            orig_Com_Printf("FATAL ERROR: DrawPicWidth or DrawPicHeight is 0! This should not happen!\n");
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

void __stdcall hkglVertex2f(float x, float y) {
    HookCallsite::recordAndGetFnStartExternal("glVertex2f");

    switch (g_activeRenderType) {
        case uiRenderType::Console:
            if (g_activeDrawCall != DrawRoutineType::StretchPic) {
                orig_glVertex2f(x * fontScale, y * fontScale);
                return;
            }
        break;
        case uiRenderType::Scoreboard: {
            scaleVertexFromScreenCenter(x, y, hudScale);
            return;
        }
        default:
            // Generic Draw calls
            switch (g_activeDrawCall) {
                case DrawRoutineType::PicOptions: {
                    switch (g_currentPicOptionsCaller) {
                        case PicOptionsCaller::SCR_DrawCrosshair: {
                            scaleVertexFromScreenCenter(x, y, crosshairScale);
                            return;
                        }
                    }
                break;
                }
                case DrawRoutineType::Pic: {
                    /*
                        ExecuteLayoutString, (already filtered)
                        NetworkDisconnectIcon,
                        BackdropDraw,
                        DrawPic
                    */
                    switch (g_currentPicCaller) {
                        case PicCaller::SCR_DrawCrosshair: {
                            scaleVertexFromScreenCenter(x, y, crosshairScale);
                            return;
                        }
                        default: {
                            if (mainMenuBgTiled) {
                                //BackdropDraw
                                static int vertexCounter = 1;
                                static int startX;
                                static int startY;
                                if (vertexCounter == 1) {
                                    startX = x;
                                    startY = y;
                                }
                                if (vertexCounter > 1 && vertexCounter < 4)
                                    x = startX + DrawPicWidth;
                                if (vertexCounter > 2)
                                    y = startY + DrawPicHeight;
                                orig_glVertex2f(x, y);
                                vertexCounter++;
                                if (vertexCounter > 4) { 
                                    vertexCounter = 1;
                                    DrawPicWidth = 0;
                                    DrawPicHeight = 0;
                                }
                                return;
                            } else {
                                // Scale from center of image
                                /*
                                    NetworkDisconnectIcon,
                                    DrawPic (Wrapper)
                                */
                                if (g_currentPicCaller != PicCaller::DrawPicWrapper) {
                                    break;
                                    scaleVertexFromCenter(x, y, hudScale);
                                    return;
                                }
                            }
                            break;
                        }
                    
                    }
                break;

            }
        }
    }
    orig_glVertex2f(x, y);
}

// =============================================================================
// LIFECYCLE CALLBACK IMPLEMENTATIONS
// =============================================================================

static void scaledUI_EarlyStartup(void)
{
    PrintOut(PRINT_LOG, "scaled_ui: Early startup - applying memory patches\n");
    
    cls_state = (int*)rvaToAbsExe((void*)0x001C1F00);
    
    orig_Con_Initialize = (void(*)())rvaToAbsExe((void*)0x00020720);
    orig_SRC_AddDirtyPoint = (void(*)(int,int))rvaToAbsExe((void*)0x000140B0);
    orig_SCR_DirtyRect = (void(*)(int,int,int,int))rvaToAbsExe((void*)0x00014190);
    
    writeIntegerAt(rvaToAbsExe((void*)0x00020F6F), (int)&real_refdef_width);
    
    // NOP SCR_AddDirtyPoint, we call it ourselves with fixed args
    // In CON_DrawConsole()
    WriteByte(rvaToAbsExe((void*)0x00021039), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002103A), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002103B), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002103C), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002103D), 0x90);

    // In CON_DrawConsole()
    WriteByte(rvaToAbsExe((void*)0x00021049), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002104A), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002104B), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002104C), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002104D), 0x90);



    
#ifdef UI_MENU
    extern char * __thiscall my_rect_c_Parse(void* toke_c, int idx);
    WriteE8Call(rvaToAbsExe((void*)0x000CF8BA), (void*)&my_rect_c_Parse);
    
    extern void my_Draw_GetPicSize(int *w, int *h, char *pic);
    WriteE8Call(rvaToAbsExe((void*)0x000C8856), (void*)&my_Draw_GetPicSize);
    WriteByte(rvaToAbsExe((void*)0x000C885B), 0x90);
#endif
    
    // All hooks are now automatically registered via REGISTER_HOOK macros
    PrintOut(PRINT_LOG, "scaled_ui: All hooks registered automatically\n");
    PrintOut(PRINT_LOG, "scaled_ui: Early startup complete\n");
}


static void scaledUI_RefDllLoaded(void)
{
    PrintOut(PRINT_LOG, "scaled_ui: Installing ref.dll hooks\n");
    
    void* glVertex2f = (void*)*(int*)rvaToAbsRef((void*)0x000A4670);
    orig_glVertex2i = (void(__stdcall*)(int,int))*(int*)rvaToAbsRef((void*)0x000A46D0);
    orig_glDisable = (void(__stdcall*)(int))*(int*)rvaToAbsRef((void*)0x000A44EC);

    
    
    /*
        Catch all hook for scaling : 
            -Console/Notify Text
            -Scoreboard (Draw_Pic/Draw_StretchPic/Draw_String_Color/Draw_Char)
            -DrawPicCenter (Draw_Pic)
                SCR_ExecuteLayoutString() (scoreboard) (X)
                SCR_Crosshair()
                SCR_UpdateScreen->pics/console/net
                backdrop_c::Draw() (For menu crosshair))
            -DrawPicTiled (Draw_Pic backdrop)
            -menuLoadboxDraw
            -else

        Needed by:
            _sofbuddy_font_scale (console fonts)
            _sofbuddy_hud_scale (scoreboard)
            crosshair/menus rescale (Draw_Pic calls)
    */
    DetourRemove((void**)&orig_glVertex2f);
    orig_glVertex2f = (void(__stdcall*)(float,float))DetourCreate((void*)glVertex2f, (void*)&hkglVertex2f, DETOUR_TYPE_JMP, DETOUR_LEN_AUTO);
    
    WriteE8Call(rvaToAbsRef((void*)0x0000239E), (void*)&my_glVertex2f_CroppedPic_1);
    WriteByte(rvaToAbsRef((void*)0x000023A3), 0x90);
    
    WriteE8Call(rvaToAbsRef((void*)0x000023CC), (void*)&my_glVertex2f_CroppedPic_2);
    WriteByte(rvaToAbsRef((void*)0x000023D1), 0x90);
    
    WriteE8Call(rvaToAbsRef((void*)0x000023F6), (void*)&my_glVertex2f_CroppedPic_3);
    WriteByte(rvaToAbsRef((void*)0x000023FB), 0x90);
    
    WriteE8Call(rvaToAbsRef((void*)0x0000240E), (void*)&my_glVertex2f_CroppedPic_4);
    WriteByte(rvaToAbsRef((void*)0x00002413), 0x90);

    /*
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
    */
    #if 1
    WriteE8Call(rvaToAbsRef((void*)0x00004860), (void*)&my_glVertex2f_DrawFont_1);
    WriteByte(rvaToAbsRef((void*)0x00004865), 0x90);

    WriteE8Call(rvaToAbsRef((void*)0x00004892), (void*)&my_glVertex2f_DrawFont_2);
    WriteByte(rvaToAbsRef((void*)0x00004897), 0x90);

    WriteE8Call(rvaToAbsRef((void*)0x000048D2), (void*)&my_glVertex2f_DrawFont_3);
    WriteByte(rvaToAbsRef((void*)0x000048D7), 0x90);

    WriteE8Call(rvaToAbsRef((void*)0x00004903), (void*)&my_glVertex2f_DrawFont_4);
    WriteByte(rvaToAbsRef((void*)0x00004908), 0x90);
    #endif
#ifdef UI_MENU
    orig_Draw_GetPicSize = (void(*)(int*, int*, char*))*(int*)rvaToAbsExe((void*)0x004035B4);
#endif
    
    PrintOut(PRINT_LOG, "scaled_ui: ref.dll hooks installed successfully\n");
}

void hkCon_Init(void) {
    PrintOut(PRINT_LOG, "scaled_ui: Registering CVars\n");
    
    extern void create_scaled_ui_cvars(void);
    create_scaled_ui_cvars();
    oCon_Init(); 
}

void crosshairscale_change(cvar_t * cvar) {
    crosshairScale = roundf(cvar->value * 4.0f) / 4.0f;
}

#endif // FEATURE_UI_SCALING