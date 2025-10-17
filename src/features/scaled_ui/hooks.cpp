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
*/

#include "feature_config.h"

#if FEATURE_UI_SCALING

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "shared_hook_manager.h"
#include "feature_macro.h"
#include "scaled_ui.h"

#include "DetourXS/detourxs.h"
#include <math.h>
#include <stdint.h>

// =============================================================================
// GLOBAL VARIABLE DEFINITIONS
// =============================================================================

// Font scaling variables
float fontScale = 1;
float consoleSize = 0.5;
bool consoleBeingRendered = false;
bool isFontInner = false;
bool isConsoleBg = false;
bool isDrawingScoreboard = false;
bool isDrawingTeamicons = false;

// HUD scaling variables
float hudScale = 1;
bool hudStretchPic = false;
bool hudStretchPicCenter = false;
bool hudDmRanking = false;
bool hudDmRanking_wasImage = false;
bool hudInventoryAndAmmo = false;
bool hudInventory_wasItem = true;
bool hudHealthArmor = false;

// Menu scaling variables (always defined, but only used when UI_MENU is enabled)
bool isDrawPicTiled = false;
bool isDrawPicCenter = false;
int DrawPicWidth = 0;
int DrawPicHeight = 0;
bool isMenuSpmSettings = false;

// Shared variables
float screen_y_scale = 1.0f;
int real_refdef_width = 0;

// Menu scaling variables (always defined, but only used when UI_MENU is enabled)
bool menuSliderDraw = false;
bool menuLoadboxDraw = false;
bool menuVerticalScrollDraw = false;

int menuLoadboxFirstItemX;
int menuLoadboxFirstItemY;

// HUD specific variables
int croppedWidth = 0;
int croppedHeight = 0;
int DrawPicPivotCenterX = 0;
int DrawPicPivotCenterY = 0;

// Font specific variables
float draw_con_frac = 1.0;
int* cls_state = (int*)0x201C1F00;

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
void( * orig_Con_Initialize)(void) = (void(*)())0x20020720;
// Legacy function pointers (keeping for compatibility)
void( * orig_SRC_AddDirtyPoint)(int x, int y) = (void(*)(int,int))0x200140B0;
void( * orig_SCR_DirtyRect)(int x1, int y1, int x2, int y2) = (void(*)(int,int,int,int))0x20014190;

// glVertex2f is handled specially since its address is resolved at runtime
void(__stdcall * orig_glVertex2f)(float one, float two) = NULL;
void(__stdcall * orig_glVertex2i)(int x, int y) = NULL;

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
void (__thiscall *orig_frame_c_Constructor)(void* self, void * menu_c, char * width, char * height, void * frame_name) = (void(__thiscall*)(void*, void*, char*, char*, void*))0x200C60A0;
void (__thiscall *orig_stm_c_ParseBlank)(void *self_stm_c, void * toke_c);
int (__thiscall *orig_toke_c_GetNTokens)(void * toke_c, int quantity) = (int(__thiscall*)(void*, int))0x200EAFE0;
char * (__thiscall *orig_toke_c_Token)(void * toke_c, int idx) = (char*(__thiscall*)(void*, int))0x200EB440;
void * (__cdecl *orig_new_std_string)(int length) = (void*(__cdecl*)(int))0x200FA352;
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

// Font scaling hooks
REGISTER_HOOK_VOID(Con_DrawNotify, 0x20020D70, void, __cdecl);
REGISTER_HOOK(Con_DrawConsole, 0x20020F90, void, __cdecl, float frac);
REGISTER_HOOK_VOID(SCR_DrawPlayerInfo, 0x20015B10, void, __cdecl);
REGISTER_HOOK_VOID(Con_CheckResize, 0x20020880, void, __cdecl);
REGISTER_HOOK_VOID(Con_Init, 0x200208E0, void, __cdecl);
REGISTER_HOOK(SCR_ExecuteLayoutString, 0x20014510, void, __cdecl, char* text);

// HUD scaling hooks
REGISTER_HOOK(cInventory2_And_cGunAmmo2_Draw, 0x20008430, void, __thiscall, void* self);
REGISTER_HOOK(cHealthArmor2_Draw, 0x20008C60, void, __thiscall, void* self);
REGISTER_HOOK(cDMRanking_Draw, 0x20007B30, void, __thiscall, void* self);
REGISTER_HOOK(cCtfFlag_Draw, 0x20006920, void, __thiscall, void* self);

// Shared rendering hooks
REGISTER_HOOK_LEN(Draw_StretchPic, 0x30001D10, 5, void, __cdecl, int x, int y, int w, int h, int palette, char * name, int flags);
REGISTER_HOOK_LEN(Draw_Pic, 0x30001ED0, 7, void, __cdecl, int x, int y, char const * imgname, int palette);
REGISTER_HOOK_LEN(GL_FindImage, 0x30007380, 6, void*, __cdecl, char *filename, int imagetype, char mimap, char allowPicmip);
REGISTER_HOOK_LEN(Draw_PicOptions, 0x30002080, 6, void, __cdecl, int x, int y, float w_scale, float h_scale, int palette, char * name);
REGISTER_HOOK_LEN(Draw_CroppedPicOptions, 0x30002240, 5, void, __cdecl, int x, int y, int c1x, int c1y, int c2x, int c2y, int palette, char * name);
REGISTER_HOOK_LEN(R_DrawFont, 0x300045B0, 6, void, __cdecl, int screenX, int screenY, char * text, int colorPalette, char * font, bool rememberLastColor);
// REGISTER_HOOK_LEN(Draw_String_Color, 0x30001A40, 5, void, __cdecl, int x, int y, char const * text, int length, int colorPalette);

#ifdef UI_MENU
// Menu scaling hooks (experimental feature)
REGISTER_HOOK(M_PushMenu, 0x200C7630, void, __cdecl, const char* name, const char* frame, bool force);
REGISTER_HOOK_LEN(R_Strlen, 0x300042F0, 7, int, __cdecl, char * str, char * fontStd);
REGISTER_HOOK_LEN(R_StrHeight, 0x300044C0, 7, int, __cdecl, char * fontStd);
REGISTER_HOOK_LEN(stm_c_ParseStm, 0x200E7E70, 5, char*, __thiscall, void *self_stm_c, void * toke_c);
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
void hkDraw_StretchPic(int x, int y, int w, int h, int palette, char * name, int flags) {
    if (consoleBeingRendered) {
        // Console background scaling
        isConsoleBg = true;
        extern float draw_con_frac;
        int consoleHeight = draw_con_frac * current_vid_h;
        y = -1 * current_vid_h + consoleHeight;
        oDraw_StretchPic(x, y, w, h, palette, name, flags);
        orig_SRC_AddDirtyPoint(0, 0);
        orig_SRC_AddDirtyPoint(w - 1, consoleHeight - 1);
        isConsoleBg = false;
        return;
    }
    
    if (hudStretchPic) {
        // HUD scaling (CTF flag, etc.)
        w = w * hudScale;
        h = h * hudScale;
    } else if (menuSliderDraw) {
        // Menu slider scaling (currently disabled)
        // Logic would go here if needed
    } else if (menuVerticalScrollDraw) {
        // Menu vertical scroll scaling (currently disabled)
        // Logic would go here if needed
    } else if (menuLoadboxDraw) {
        // Menu loadbox scaling (currently disabled)
        // Logic would go here if needed
    }

    // Default behavior - call original function
    oDraw_StretchPic(x, y, w, h, palette, name, flags);
}

// Shared Draw_Pic hook - used by menu scaling
/*
    This function is called internally by:
    - SCR_ExecuteLayoutString() (scoreboard)
    - SCR_Crosshair()
    - SCR_UpdateScreen->pics/console/net
    - backdrop_c::Draw() (For menu crosshair)
*/
void hkDraw_Pic(int x, int y, char const * imgname, int palette) {
#ifdef UI_MENU
    if (imgname != nullptr) {
        // Check for backdrop images that should be tiled
        extern const char* get_nth_entry(const char* str, int n);
        const char* thirdEntry = get_nth_entry(imgname, 2); 
        if (thirdEntry) {
            const char* end = thirdEntry;
            while (*end && *end != '/') ++end;
            size_t len = end - thirdEntry;
            if (len == 8 && strncmp(thirdEntry, "backdrop", 8) == 0) {
                //for hkglVertex2f and hkGL_FindImage
                isDrawPicTiled = true;
                oDraw_Pic(x, y, imgname, palette);
                isDrawPicTiled = false;
                return;
            }
        }
    }

    isDrawPicCenter = true;
    oDraw_Pic(x, y, imgname, palette);
    isDrawPicCenter = false;
#else
    // Default behavior - call original function
    oDraw_Pic(x, y, imgname, palette);
#endif
}

/*
    Shared GL_FindImage hook - used by menu scaling
    Any m32 that is loaded with GL_FindImage will be scaled here.
    We should be careful this is only used for 2d sprites etc.
*/
void* hkGL_FindImage(char *filename, int imagetype, char mimap, char allowPicmip) {
    void * image_t = oGL_FindImage(filename, imagetype, mimap, allowPicmip);
    
#ifdef UI_MENU
    if (image_t) {
        if (isDrawPicCenter) {
            DrawPicWidth = *(short*)((char*)image_t + 0x44);
            DrawPicHeight = *(short*)((char*)image_t + 0x46);
        } else if (isDrawPicTiled) {
            DrawPicWidth = *(short*)((char*)image_t + 0x44) * screen_y_scale;
            DrawPicHeight = *(short*)((char*)image_t + 0x46) * screen_y_scale;
        }
    }
#endif
    
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
    if (hudHealthArmor) {
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
    } else if (hudInventoryAndAmmo) {
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
    SCR_UpdateScreen->pics/console/net
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
void __stdcall hkglVertex2f(float x, float y) {
    /*
      This is console fonts.
      _sofbuddy_font_scale
    */
    if ((consoleBeingRendered && !isConsoleBg)) {
        
        //Preserving the actual vertex position then letting it map naturally to pixels seems better than rounding
        orig_glVertex2f(x * fontScale, y * fontScale);
        return;
    } else if (isDrawingScoreboard) {
        //_sofbuddy_hud_scale
        //Layout = Scoreboard
        //This is vertices for font and images

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
        
        /*
            Since Center Aligned.
              Save Position.
            Scale.
            Restore Position.

            NewPoint = Pivot + (OldPoint - Pivot) * Scale

            Px = current_vid_w * 0.5f
            new_x = Px + (x - Px) * hudScale
            
            hudScale - 1 is a clean way to get the percentage of change that needs to be applied 
            to a distance to calculate a positional correction.

            (I calculated it by image in gkeep, scale image = creates correct size, but moves a lot)
            so you shift it back into original place (except for when its 1, no shift needed Hence hudScale-1)
            then subtract previous mid offset N-1 times (because its alrady shifted 1 time)
        */
        
        //rounding is not necessary!
        float final_x = x * hudScale - x_first_vertex * (hudScale - 1) - x_mid_offset * (hudScale - 1);
        float final_y = y * hudScale - y_first_vertex * (hudScale - 1) - y_mid_offset * (hudScale - 1);
        
        orig_glVertex2f(final_x, final_y);

        vertexCounter++;
        if (vertexCounter > 4) vertexCounter = 1;
        return;
    } else if (isDrawPicCenter) 
    {
        /*
            This function is called internally by:
            - SCR_ExecuteLayoutString() (scoreboard)
            - SCR_Crosshair()
            - SCR_UpdateScreen->pics/console/net
            - backdrop_c::Draw() (For menu crosshair)
        */
        //Default DrawPic situation. (Is not Tiled (backdrop))

        static int vertexCounter = 1;
        
        if (vertexCounter == 1) {
            DrawPicPivotCenterX = x + DrawPicWidth * 0.5f;
            DrawPicPivotCenterY = y + DrawPicHeight * 0.5f;

            // But such sequences, if start from pos=0, benefit from multiply co ordinates.

            if ( DrawPicWidth == 0 || DrawPicHeight == 0 ) {
                // DrawPicWidth was not set, Draw_FindImage
                PrintOut(PRINT_LOG, "[DEBUG] hkglVertex2f: FATAL ERROR - DrawPicWidth or DrawPicHeight is 0!\n");
                orig_Com_Printf("FATAL ERROR: DrawPicWidth or DrawPicHeight is 0! This should not happen!\n");
                exit(1);
            }
        }

        x = DrawPicPivotCenterX + (x - DrawPicPivotCenterX) * hudScale;
        y = DrawPicPivotCenterY + (y - DrawPicPivotCenterY) * hudScale;

        
        orig_glVertex2f(x, y);
        vertexCounter++;
        if (vertexCounter > 4) {
            vertexCounter = 1;
            DrawPicWidth = 0;
            DrawPicHeight = 0;
        }
        return;
 
    } else if ( isDrawPicTiled ) {
        /*
            
            This function is called internally by:
            - SCR_ExecuteLayoutString() (scoreboard)
            - SCR_Crosshair()
            - SCR_UpdateScreen->pics/console/net
            - backdrop_c::Draw() (For menu crosshair)
        
            Simply Draw_Pic() with "pics/backdrop/" path
            
            For rare tile-based (pivot-point 0) textures.
            (eg. backdrop main menu)
        */
        
        static int vertexCounter = 1;
        static int startX;
        static int startY;
        if ( vertexCounter == 1 ) {
            startX = x;
            startY = y;
        }

        if ( vertexCounter > 1 && vertexCounter < 4 ) {
            x = startX + DrawPicWidth;
        }

        if ( vertexCounter > 2 ) {      
            y = startY + DrawPicHeight;
        }
        orig_glVertex2f(x, y);

        vertexCounter++;
        if (vertexCounter > 4) {
            vertexCounter = 1;
            DrawPicWidth = 0;
            DrawPicHeight = 0;
        }
        return;
    } else if ( menuLoadboxDraw ) {
        /*
          Too hard to scale these. Not enough information.
        */
        #if 0
        x = menuLoadboxFirstItemX + (x - menuLoadboxFirstItemX) * screen_y_scale;
        y = menuLoadboxFirstItemY + (y - menuLoadboxFirstItemY) * screen_y_scale;
        #endif
        orig_glVertex2f(x, y);
    }
    else {
        orig_glVertex2f(x, y);
    }
}

// =============================================================================
// LIFECYCLE CALLBACK IMPLEMENTATIONS
// =============================================================================

static void scaledUI_EarlyStartup(void)
{
    PrintOut(PRINT_LOG, "scaled_ui: Early startup - applying memory patches\n");
    
    // For bottom of Con_NotifyConsole, remain real width
    writeIntegerAt((void*)0x20020F6F, (int)&real_refdef_width);
    
    // NOP SCR_AddDirtyPoint, we call it ourselves with fixed args
    // In CON_DrawConsole()
    WriteByte((void*)0x20021039, 0x90);
    WriteByte((void*)0x2002103A, 0x90);
    WriteByte((void*)0x2002103B, 0x90);
    WriteByte((void*)0x2002103C, 0x90);
    WriteByte((void*)0x2002103D, 0x90);
    // In CON_DrawConsole()
    WriteByte((void*)0x20021049, 0x90);
    WriteByte((void*)0x2002104A, 0x90);
    WriteByte((void*)0x2002104B, 0x90);
    WriteByte((void*)0x2002104C, 0x90);
    WriteByte((void*)0x2002104D, 0x90);
    
#ifdef UI_MENU
    // Menu scaling memory patches
    //parsing width height on menu options
    extern char * __thiscall my_rect_c_Parse(void* toke_c, int idx);
    WriteE8Call((void*)0x200CF8BA, (void*)&my_rect_c_Parse);
    
    //Menu Pictures sizing override except for icons/teamicons/vend
    //This that need to remain the same size ... for now...
    extern void my_Draw_GetPicSize(int *w, int *h, char *pic);
    WriteE8Call((void*)0x200C8856, (void*)&my_Draw_GetPicSize);
    WriteByte((void*)0x200C885B, 0x90);
#endif
    
    // All hooks are now automatically registered via REGISTER_HOOK macros
    PrintOut(PRINT_LOG, "scaled_ui: All hooks registered automatically\n");
    PrintOut(PRINT_LOG, "scaled_ui: Early startup complete\n");
}

static void scaledUI_RefDllLoaded(void)
{
    PrintOut(PRINT_LOG, "scaled_ui: Installing ref.dll hooks\n");
    
    // Get OpenGL vertex function pointers from ref.dll
    void* glVertex2f = (void*)*(int*)0x300A4670;
    orig_glVertex2i = (void(__stdcall*)(int,int))*(int*)0x300A46D0;
    
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
    
    /*   
        Required for HUD Vertex scaling - patch each corner of rectangle
        This function is called internally by:
            - cInventory2::Draw() (ammo, gun, item displays)
            - cHealthArmour2::Draw() (HP bar, armour bar)
    */
    WriteE8Call((void*)0x3000239E, (void*)&my_glVertex2f_CroppedPic_1);
    WriteByte((void*)0x300023A3, 0x90);
    
    WriteE8Call((void*)0x300023CC, (void*)&my_glVertex2f_CroppedPic_2);
    WriteByte((void*)0X300023D1, 0X90);
    
    WriteE8Call((void*)0x300023F6, (void*)&my_glVertex2f_CroppedPic_3);
    WriteByte((void*)0x300023FB, 0X90);
    
    WriteE8Call((void*)0x3000240E, (void*)&my_glVertex2f_CroppedPic_4);
    WriteByte((void*)0x30002413, 0X90);

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
    //R_DrawFont
    #if 1
    WriteE8Call((void*)0x30004860, (void*)&my_glVertex2f_DrawFont_1);
    WriteByte((void*)0x30004865, 0X90);

    WriteE8Call((void*)0x30004892, (void*)&my_glVertex2f_DrawFont_2);
    WriteByte((void*)0x30004897, 0X90);

    WriteE8Call((void*)0x300048D2, (void*)&my_glVertex2f_DrawFont_3);
    WriteByte((void*)0x300048D7, 0X90);

    WriteE8Call((void*)0x30004903, (void*)&my_glVertex2f_DrawFont_4);
    WriteByte((void*)0x30004908, 0X90);
    #endif
#ifdef UI_MENU
    // Menu scaling ref.dll hooks
    orig_Draw_GetPicSize = (void(*)(int*, int*, char*))*(int*)0x204035B4;
#endif
    
    PrintOut(PRINT_LOG, "scaled_ui: ref.dll hooks installed successfully\n");
}

void hkCon_Init(void) {
    PrintOut(PRINT_LOG, "scaled_ui: Registering CVars\n");
    
    // Register all font scaling CVars
    extern void create_scaled_ui_cvars(void);
    create_scaled_ui_cvars();

    // Call original Con_Init (initializes console buffer, etc.)
    oCon_Init(); 
}

#endif // FEATURE_UI_SCALING