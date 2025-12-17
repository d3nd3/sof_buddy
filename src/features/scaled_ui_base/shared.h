/*
	Scaled UI - Shared Header
	
	This file contains all shared declarations for the scaled_ui feature including:
	- Global variables
	- Function declarations
	- Enums and constants
*/

#ifndef SCALED_UI_H
#define SCALED_UI_H

#include "feature_config.h"
#include "features.h"
#include "debug/callsite_classifier.h"
#include "caller_from.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "generated_detours.h"

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================


extern float fontScale;
extern float consoleSize;
extern bool isFontInner;
extern bool isDrawingTeamicons;

extern float hudScale;
extern float crosshairScale;
extern bool hudStretchPicCenter;
extern bool hudDmRanking;
extern bool hudDmRanking_wasImage;
extern bool hudInventoryAndAmmo;
extern bool hudInventory_wasItem;
extern bool hudHealthArmor;

extern int DrawPicWidth;
extern int DrawPicHeight;
extern bool isMenuSpmSettings;

extern int menuLoadboxFirstItemX;
extern int menuLoadboxFirstItemY;

// Shared variables
extern float screen_y_scale;
extern int current_vid_w;
extern int current_vid_h;
extern int* viddef_width;
extern int real_refdef_width;

// HUD specific variables
extern int croppedWidth;
extern int croppedHeight;
extern int DrawPicPivotCenterX;
extern int DrawPicPivotCenterY;

// Track active Draw_PicOptions caller (shared)
extern PicOptionsCaller g_currentPicOptionsCaller;

// Track active Draw_StretchPic caller (shared)
extern StretchPicCaller g_currentStretchPicCaller;

// Track active Draw_Pic caller (shared)
extern PicCaller g_currentPicCaller;

// Font specific variables
extern int characterIndex;
extern FontCaller g_currentFontCaller;

// =============================================================================
// ENUMS AND CONSTANTS
// =============================================================================

// Unified active caller type for context propagation
enum class DrawRoutineType {
    None = 0,
    StretchPic,
    Pic,
    PicOptions,
    CroppedPic,
    Font
};

extern DrawRoutineType g_activeDrawCall;

enum class uiRenderType {
    None = 0,
    Console,
    HudCtfFlag,
    HudDmRanking,
    HudInventory,
    HudHealthArmor,
    Scoreboard
};

extern uiRenderType g_activeRenderType;

enum enumCroppedDrawMode {
    HEALTH_FRAME,
    HEALTH,
    ARMOR,
    STEALTH_FRAME,
    STEALTH,
    GUN_AMMO_TOP,
    GUN_AMMO_BOTTOM,
    GUN_AMMO_SWITCH,
    GUN_AMMO_SWITCH_LIP,
    ITEM_INVEN_TOP,
    ITEM_INVEN_BOTTOM,
    ITEM_INVEN_SWITCH,
    ITEM_INVEN_SWITCH_LIP,
    ITEM_ACTIVE_LOGO,
    OTHER_UNKNOWN
};

extern enumCroppedDrawMode hudCroppedEnum;

enum realFontEnum_t {
    REALFONT_TITLE,
    REALFONT_SMALL,
    REALFONT_MEDIUM,
    REALFONT_INTERFACE,
    REALFONT_UNKNOWN
};

extern realFontEnum_t realFont;

// Font sizes for each realFontEnum_t, index by enum value
extern const int realFontSizes[5];

// Font caller context enum for R_DrawFont callers (defined in text.cpp)
enum class FontCaller;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

// Font scaling functions
#if FEATURE_SCALED_CON
void hkCon_DrawConsole(float frac, detour_Con_DrawConsole::tCon_DrawConsole original);
void hkCon_DrawNotify(detour_Con_DrawNotify::tCon_DrawNotify original);
void hkCon_CheckResize(detour_Con_CheckResize::tCon_CheckResize original);
void hkSCR_DrawPlayerInfo(detour_SCR_DrawPlayerInfo::tSCR_DrawPlayerInfo original);
void fontscale_change(cvar_t * cvar);
void consolesize_change(cvar_t * cvar);
#endif
void hkDraw_String_Color(int x, int y, char const * text, int length, int colorPalette);
#if FEATURE_SCALED_HUD
void hkSCR_ExecuteLayoutString(char * text, detour_SCR_ExecuteLayoutString::tSCR_ExecuteLayoutString original);
#endif

// Low-level font vertex hooks (used by ref.dll detours)
void __stdcall my_glVertex2f_DrawFont_1(float x, float y);
void __stdcall my_glVertex2f_DrawFont_2(float x, float y);
void __stdcall my_glVertex2f_DrawFont_3(float x, float y);
void __stdcall my_glVertex2f_DrawFont_4(float x, float y);
void __stdcall my_glVertex2f_CroppedPic_1(float x, float y);
void __stdcall my_glVertex2f_CroppedPic_2(float x, float y);
void __stdcall my_glVertex2f_CroppedPic_3(float x, float y);
void __stdcall my_glVertex2f_CroppedPic_4(float x, float y);

// Additional font scaling variables
extern float draw_con_frac;
extern int* cls_state;

// CVars
extern cvar_t * _sofbuddy_font_scale;
extern cvar_t * _sofbuddy_console_size;
extern cvar_t * _sofbuddy_hud_scale;
extern cvar_t * _sofbuddy_crossh_scale;
void create_scaled_ui_cvars(void);

// Function pointer declarations for original functions
// These are now accessed via using declarations in sui_hooks.cpp from generated_detours.h namespaces
// The pointers are declared as extern in generated_detours.h and defined in generated_detours.cpp
// This ensures a single shared instance across all translation units

// OpenGL original vertex function (resolved at runtime in hooks.cpp)
extern void(__stdcall * orig_glVertex2f)(float one, float two);
extern void(__stdcall * orig_glVertex2i)(int x, int y);
extern void(__stdcall * orig_glDisable)(int cap);
extern void (*orig_Draw_GetPicSize)(int *w, int *h, char *pic);
extern void * (__cdecl *orig_new_std_string)(int length);

// HUD scaling functions
#if FEATURE_SCALED_HUD
void hkDraw_PicOptions(int x, int y, float w_scale, float h_scale, int pal, char * name, detour_Draw_PicOptions::tDraw_PicOptions original);
void hkDraw_CroppedPicOptions(int x, int y, int c1x, int c1y, int c2x, int c2y, int palette, char * name, detour_Draw_CroppedPicOptions::tDraw_CroppedPicOptions original);
#endif
#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
void hkR_DrawFont(int screenX, int screenY, char * text, int colorPalette, char * font, bool rememberLastColor, detour_R_DrawFont::tR_DrawFont original);
#endif
#if FEATURE_SCALED_HUD
void hkcInventory2_And_cGunAmmo2_Draw(void * self, detour_cInventory2_And_cGunAmmo2_Draw::tcInventory2_And_cGunAmmo2_Draw original);
void hkcHealthArmor2_Draw(void * self, detour_cHealthArmor2_Draw::tcHealthArmor2_Draw original);
void hkcDMRanking_Draw(void * self, detour_cDMRanking_Draw::tcDMRanking_Draw original);
void hkcCtfFlag_Draw(void * self, detour_cCtfFlag_Draw::tcCtfFlag_Draw original);
#endif
#if FEATURE_SCALED_HUD
void hudscale_change(cvar_t * cvar);
void crosshairscale_change(cvar_t * cvar);
#endif
realFontEnum_t getRealFontEnum(const char* realFont);
void drawCroppedPicVertex(bool top, bool left, float & x, float & y);

// Shared rendering functions
#ifdef UI_MENU
// Menu scaling functions
void my_Draw_GetPicSize(int *w, int *h, char *pic);
int hkR_Strlen(char * str, char * fontStd);
int hkR_StrHeight(char * fontStd);
void my_M_PushMenu(const char * name, const char * frame, bool force);
void hkM_PushMenu(const char * name, const char * frame, bool force);
char * findClosingBracket(char * start);
void mutateWidthTokeC_resize(void * toke_c);
void mutateWidthTokeC_width_height(void * toke_c, char * match);
void mutateBlankTokeC_width_height(void * toke_c);
void __thiscall my_master_Draw(void * self);
void __thiscall my_slider_c_Draw(void * self);
void __thiscall my_loadbox_c_Draw(void * self);
void __thiscall my_vbar_c_Draw(void * self);
void __thiscall my_frame_c_Constructor(void* self, void * menu_c, char * width, char * height, void * frame_name);
char * __thiscall hkstm_c_ParseStm(void *self_stm_c, void * toke_c);
char * __thiscall my_rect_c_Parse(void* toke_c, int idx);
void __thiscall my_stm_c_ParseBlank(void *self_stm_c, void * toke_c);
const char* get_nth_entry(const char* str, int n);
#endif

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#endif // SCALED_UI_H
