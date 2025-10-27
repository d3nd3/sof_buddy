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

#if FEATURE_UI_SCALING

#include "sof_compat.h"
#include "features.h"

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Font scaling variables
extern float fontScale;
extern float consoleSize;
extern bool consoleBeingRendered;
extern bool isFontInner;
extern bool isConsoleBg;
extern bool isDrawingScoreboard;
extern bool isDrawingTeamicons;

// HUD scaling variables
extern float hudScale;
extern bool hudStretchPic;
extern bool hudStretchPicCenter;
extern bool hudDmRanking;
extern bool hudDmRanking_wasImage;
extern bool hudInventoryAndAmmo;
extern bool hudInventory_wasItem;
extern bool hudHealthArmor;

// Menu scaling variables (always defined, but only used when UI_MENU is enabled)
extern bool isDrawPicTiled;
extern bool isDrawPicCenter;
extern int DrawPicWidth;
extern int DrawPicHeight;
extern bool isMenuSpmSettings;

// Menu scaling variables (always defined, but only used when UI_MENU is enabled)
extern bool menuSliderDraw;
extern bool menuLoadboxDraw;
extern bool menuVerticalScrollDraw;
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

// Font specific variables
extern int characterIndex;

// =============================================================================
// ENUMS AND CONSTANTS
// =============================================================================

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
extern const int realFontSizes[4];

// Font caller context enum for R_DrawFont callers (defined in text.cpp)
enum class FontCaller;

// Getter for current font caller context (for inner glVertex calls)
FontCaller getCurrentFontCaller();

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

// Font scaling functions
void hkCon_DrawConsole(float frac);
void hkCon_DrawNotify(void);
void hkCon_CheckResize(void);
void hkDraw_String_Color(int x, int y, char const * text, int length, int colorPalette);
void hkSCR_ExecuteLayoutString(char * text);
void hkSCR_DrawPlayerInfo(void);
void fontscale_change(cvar_t * cvar);
void consolesize_change(cvar_t * cvar);

// Low-level font vertex hooks (used by ref.dll detours)
void __stdcall my_glVertex2f_DrawFont_1(float x, float y);
void __stdcall my_glVertex2f_DrawFont_2(float x, float y);
void __stdcall my_glVertex2f_DrawFont_3(float x, float y);
void __stdcall my_glVertex2f_DrawFont_4(float x, float y);

// Additional font scaling variables
extern float draw_con_frac;
extern int* cls_state;

// Function pointer declarations for original functions
extern void (*oCon_DrawConsole)(float frac);
extern void (*oCon_DrawNotify)(void);
extern void (*oCon_CheckResize)(void);
extern void (*oCon_Init)(void);
extern void (*oDraw_StretchPic)(int x, int y, int w, int h, int palette, char * name, int flags);
extern void* (*oGL_FindImage)(char *filename, int imagetype, char mimap, char allowPicmip);
extern void (*oR_DrawFont)(int screenX, int screenY, char * text, int colorPalette, char * font, bool rememberLastColor);
extern void (*oDraw_Pic)(int x, int y, char const * imgname, int palette);
extern void (*oDraw_PicOptions)(int x, int y, float w_scale, float h_scale, int palette, char * name);
extern void (*oDraw_CroppedPicOptions)(int x, int y, int c1x, int c1y, int c2x, int c2y, int palette, char * name);
extern void (*oDraw_String_Color)(int x, int y, char const * text, int length, int colorPalette);
extern int (*oR_Strlen)(char * str, char * fontStd);
extern int (*oR_StrHeight)(char * fontStd);
extern void (*oSCR_ExecuteLayoutString)(char * text);
extern void (*oSCR_DrawPlayerInfo)(void);
extern void (*oM_PushMenu)(const char * name, const char * frame, bool force);
extern char* (*ostm_c_ParseStm)(void *self_stm_c, void * toke_c);
extern void (__thiscall *ocInventory2_And_cGunAmmo2_Draw)(void * self);
extern void (__thiscall *ocHealthArmor2_Draw)(void * self);
extern void (__thiscall *ocDMRanking_Draw)(void * self);
extern void (__thiscall *ocCtfFlag_Draw)(void * self);

// OpenGL original vertex function (resolved at runtime in hooks.cpp)
extern void(__stdcall * orig_glVertex2f)(float one, float two);
extern void(__stdcall * orig_glVertex2i)(int x, int y);
extern void(__stdcall * orig_glDisable)(int cap);

// HUD scaling functions
void hkDraw_PicOptions(int x, int y, float w_scale, float h_scale, int pal, char * name);
void hkDraw_CroppedPicOptions(int x, int y, int c1x, int c1y, int c2x, int c2y, int palette, char * name);
void hkR_DrawFont(int screenX, int screenY, char * text, int colorPalette, char * font, bool rememberLastColor);
void __thiscall hkcInventory2_And_cGunAmmo2_Draw(void * self);
void __thiscall hkcHealthArmor2_Draw(void * self);
void __thiscall hkcDMRanking_Draw(void * self);
void __thiscall hkcCtfFlag_Draw(void * self);
void hudscale_change(cvar_t * cvar);
realFontEnum_t getRealFontEnum(const char* realFont);
void drawCroppedPicVertex(bool top, bool left, float & x, float & y);

// Shared rendering functions
void hkDraw_StretchPic(int x, int y, int w, int h, int palette, char * name, int flags);
void hkDraw_Pic(int x, int y, char const * imgname, int palette);
void* hkGL_FindImage(char *filename, int imagetype, char mimap, char allowPicmip);

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

#endif // FEATURE_UI_SCALING

#endif // SCALED_UI_H
