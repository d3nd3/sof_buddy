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
extern bool fontScaleAuto;
void apply_auto_font_scale(void);
void font_scale_auto_change(cvar_t* cvar);
extern float consoleSize;
extern bool isFontInner;
extern bool isDrawingTeamicons;

extern float hudScale;
extern bool hudScaleAuto;
void apply_auto_hud_scale(void);
void hud_scale_auto_change(cvar_t* cvar);
extern float scaleRoundRatio;
extern bool scaleRoundAuto;
float round_scale_value(float v);
float effective_auto_scale(float raw);
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
extern bool g_scaleCinematicPics;

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
extern char g_lastCenterPrintText[1024];
extern unsigned int g_lastCenterPrintSeq;
extern int g_lastCenterPrintLineCount;
extern float g_centerPrintAnchorY;
extern unsigned int g_centerPrintAnchorSeq;
extern float g_centerPrintBottomY;
extern float g_centerPrintTargetBottomY;
extern unsigned int g_centerPrintScaleSeq;
extern float g_centerPrintLineStep;

// Bottom-anchored text scaling (typematic + center print): preserve 480p margin %.
void computeTextBottomAnchor(float topLineY, int lineCount, float lineHeight,
    float& bottomY, float& targetBottomY);
void applyBottomAnchoredScale(float& x, float& y, float bottomY, float targetBottomY,
    float scale, bool centerX);
extern float g_missionStatusAnchorY;

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
    Scoreboard,
    Cinematic
};

extern uiRenderType g_activeRenderType;

// Nesting depth for SCR_DrawCinematicString only; Draw_CharExtra scales via this, not g_activeRenderType.
extern int g_cinematicDrawDepth;
extern int g_cineTextLineCount;   // lines in typematic buffer (split on \n)
extern float g_cineTextBaseY;     // first drawn line Y (set on first char)
extern float g_cineTextBottomY;   // last line Y at scale 1 (base + (lines-1)*8)
extern float g_cineTextTargetBottomY; // bottom Y preserving 480p margin percentage

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

// Reset quad-vertex counters used by glVertex2f E8 handlers (call at draw-scope entry).
void resetGlVertexQuadState();

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

// Low-level vertex hooks (E8 patches in ref.dll draw routines)
#if FEATURE_SCALED_CON
void __stdcall my_glVertex2f_DrawChar_1(float x, float y);
void __stdcall my_glVertex2f_DrawChar_2(float x, float y);
void __stdcall my_glVertex2f_DrawChar_3(float x, float y);
void __stdcall my_glVertex2f_DrawChar_4(float x, float y);
#endif
#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
void __stdcall my_glVertex2f_StretchPic_1(float x, float y);
void __stdcall my_glVertex2f_StretchPic_2(float x, float y);
void __stdcall my_glVertex2f_StretchPic_3(float x, float y);
void __stdcall my_glVertex2f_StretchPic_4(float x, float y);
void __stdcall my_glVertex2f_DrawPic_1(float x, float y);
void __stdcall my_glVertex2f_DrawPic_2(float x, float y);
void __stdcall my_glVertex2f_DrawPic_3(float x, float y);
void __stdcall my_glVertex2f_DrawPic_4(float x, float y);
void __stdcall my_glVertex2f_DrawFont_1(float x, float y);
void __stdcall my_glVertex2f_DrawFont_2(float x, float y);
void __stdcall my_glVertex2f_DrawFont_3(float x, float y);
void __stdcall my_glVertex2f_DrawFont_4(float x, float y);
#endif
#if FEATURE_SCALED_HUD
void __stdcall my_glVertex2f_PicOptions_1(float x, float y);
void __stdcall my_glVertex2f_PicOptions_2(float x, float y);
void __stdcall my_glVertex2f_PicOptions_3(float x, float y);
void __stdcall my_glVertex2f_PicOptions_4(float x, float y);
void __stdcall my_glVertex2f_CroppedPic_1(float x, float y);
void __stdcall my_glVertex2f_CroppedPic_2(float x, float y);
void __stdcall my_glVertex2f_CroppedPic_3(float x, float y);
void __stdcall my_glVertex2f_CroppedPic_4(float x, float y);
#endif

// Additional font scaling variables
extern float draw_con_frac;
extern int* cls_state;

// CVars
extern cvar_t * _sofbuddy_font_scale;
extern cvar_t * _sofbuddy_font_scale_auto;
extern cvar_t * _sofbuddy_console_size;
extern cvar_t * _sofbuddy_hud_scale;
extern cvar_t * _sofbuddy_hud_scale_auto;
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
void hkSCR_CenterPrint(char * text, detour_SCR_CenterPrint::tSCR_CenterPrint original);
void hkSCR_DrawCenterPrint(detour_SCR_DrawCenterPrint::tSCR_DrawCenterPrint original);
void hkSCR_DrawPause(detour_SCR_DrawPause::tSCR_DrawPause original);
void hkSCR_DrawCinematicString(int speed, int x, int y, detour_SCR_DrawCinematicString::tSCR_DrawCinematicString original);
void hkSCR_DrawCinemaScope(detour_SCR_DrawCinemaScope::tSCR_DrawCinemaScope original);
void hkDraw_CharExtra(float x, float y, float scale, void* palette, int ch, detour_Draw_CharExtra::tDraw_CharExtra original);
#endif
#if FEATURE_SCALED_HUD
void hkcInventory2_And_cGunAmmo2_Draw(void * self, detour_cInventory2_And_cGunAmmo2_Draw::tcInventory2_And_cGunAmmo2_Draw original);
void hkcHealthArmor2_Draw(void * self, detour_cHealthArmor2_Draw::tcHealthArmor2_Draw original);
void hkcMissionStatus_Draw(void * self, detour_cMissionStatus_Draw::tcMissionStatus_Draw original);
void hkcDMRanking_Draw(void * self, detour_cDMRanking_Draw::tcDMRanking_Draw original);
void hkcCtfFlag_Draw(void * self, detour_cCtfFlag_Draw::tcCtfFlag_Draw original);
#endif
#if FEATURE_SCALED_HUD
void hudscale_change(cvar_t * cvar);
void crosshairscale_change(cvar_t * cvar);
#endif
realFontEnum_t getRealFontEnum(const char* realFont);
void drawCroppedPicVertex(bool top, bool left, float & x, float & y);
void resetDmRankingFontPhase(void);

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
