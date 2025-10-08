#include "../../../hdr/feature_config.h"

#if FEATURE_FONT_SCALING

#include "../../../hdr/sof_compat.h"
#include "../../../hdr/features.h"
#include "../../../hdr/util.h"
#include "../../../hdr/shared_hook_manager.h"
#include "../../../hdr/feature_macro.h"

#include "DetourXS/detourxs.h"

#include <math.h>

#include <stdint.h>

void drawCroppedPicVertex(bool top, bool left, float & x, float & y);

// Lifecycle callback registration placed at the top for visibility
static void scaledFont_EarlyStartup(void);
static void scaledFont_RefDllLoaded(void);

//We create cvars here, because its required this early for this module.
void my_Con_Init(void);

REGISTER_SHARED_HOOK_CALLBACK(EarlyStartup, scaled_font, scaledFont_EarlyStartup, 70, Post);

REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, scaled_font, scaledFont_RefDllLoaded, 60, Post);

// Register all hooks that were previously created manually in EarlyStartup callback
REGISTER_HOOK_VOID(Con_DrawNotify, 0x20020D70, void, __cdecl);
REGISTER_HOOK(Con_DrawConsole, 0x20020F90, void, __cdecl, float frac);
REGISTER_HOOK_VOID(Con_CheckResize, 0x20020880, void, __cdecl);
REGISTER_HOOK_VOID(Con_Init, 0x200208E0, void, __cdecl);
REGISTER_HOOK(cInventory2_And_cGunAmmo2_Draw, 0x20008430, void, __thiscall, void* self);
REGISTER_HOOK(cHealthArmor2_Draw, 0x20008C60, void, __thiscall, void* self);
REGISTER_HOOK(cDMRanking_Draw, 0x20007B30, void, __thiscall, void* self);
REGISTER_HOOK(cCtfFlag_Draw, 0x20006920, void, __thiscall, void* self);
REGISTER_HOOK(SCR_ExecuteLayoutString, 0x20014510, void, __cdecl, char* text);

// Hook registrations using the new macro system
// Note: DrawStretchPic hook is now in dispatchers/ref_gl.cpp
REGISTER_HOOK_LEN(DrawPicOptions, 0x30002080, 6, void, __cdecl, int x, int y, float w_scale, float h_scale, int palette, char * name);
REGISTER_HOOK_LEN(DrawCroppedPicOptions, 0x30002240, 5, void, __cdecl, int x, int y, int c1x, int c1y, int c2x, int c2y, int palette, char * name);
REGISTER_HOOK_LEN(R_DrawFont, 0x300045B0, 6, void, __cdecl, int screenX, int screenY, char * text, int colorPalette, char * font, bool rememberLastColor);
REGISTER_HOOK_LEN(Draw_String_Color, 0x30001A40, 5, void, __cdecl, int x, int y, char const * text, int length, int colorPalette);

void( * orig_Draw_Char)(int, int, int, int) = NULL;
void( * orig_Draw_String)(int a, int b, int c, int d) = NULL;
// void (*orig_Draw_String_Color)(int a,int b,int c,int d, int e) = NULL;
void( * orig_Con_DrawConsole)(float frac) = NULL;
void( * orig_Con_DrawNotify)(void) = NULL;
void( * orig_Con_CheckResize)(void) = NULL;
void( * orig_Con_Init)(void) = NULL;
void( * orig_Con_Initialize)(void) = (void(*)())0x20020720;


// Legacy function pointers (keeping for compatibility)
void( * orig_SRC_AddDirtyPoint)(int x, int y) = (void(*)(int,int))0x200140B0;
void( * orig_SCR_DirtyRect)(int x1, int y1, int x2, int y2) = (void(*)(int,int,int,int))0x20014190;

// glVertex2f is handled specially since its address is resolved at runtime
void(__stdcall * orig_glVertex2f)(float one, float two) = NULL;

void(__stdcall * orig_glVertex2i)(int x, int y) = NULL;

//R_Strlen(char const *, basic_string<char, string_char_traits<char>, __default_alloc_template<true, 0>> &)
void( * orig_R_Strlen)(char const * text1, char * font) = NULL;

/*
	Interface Draw Functions
*/
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
void my_SCR_CenterPrint(char * text);

void( * orig_Draw_String_Color)(int, int, char
  const * , int, int);
void hkDraw_String_Color(int, int, char
  const * , int, int);

void( * orig_SCR_ExecuteLayoutString)(char * text);
void my_SCR_ExecuteLayoutString(char * text);

// Video dimensions (provided by dispatcher/exe.cpp)
extern int current_vid_w;
extern int current_vid_h;

float fontScale = 1;
float consoleSize = 0.5;
bool isFontOuter = false;
bool isFontInner = false;
bool isNotFont = false;
bool isDrawingLayout = false;

float hudScale = 1;
bool hudStretchPic = false;
bool hudStretchPicCenter = false;

bool menuSliderDraw = false;
bool menuLoadboxDraw = false;
bool menuVerticalScrollDraw = false;

int menuLoadboxFirstItemX;
int menuLoadboxFirstItemY;

bool hudDmRanking = false;
bool hudDmRanking_wasImage = false;

bool hudInventoryAndAmmo = false;
bool hudInventory_wasItem = true;

bool hudHealthArmor = false;

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
enumCroppedDrawMode hudCroppedEnum = OTHER_UNKNOWN;

int croppedWidth = 0;
int croppedHeight = 0;

int DrawPicPivotCenterX = 0;
int DrawPicPivotCenterY = 0;

// Additional variables needed for new scaling modes
// These variables are defined in scaled_menu/hooks.cpp
// extern bool isDrawPicCenter;
// extern bool isDrawPicTiled;
// extern int DrawPicWidth;
// extern int DrawPicHeight;
float screen_y_scale = 1.0f;

enum realFontEnum_t {
  REALFONT_TITLE,
  REALFONT_SMALL,
  REALFONT_MEDIUM,
  REALFONT_INTERFACE,
  REALFONT_UNKNOWN
};

// Font sizes for each realFontEnum_t, index by enum value
const int realFontSizes[4] = {
  12, // REALFONT_TITLE
  6,  // REALFONT_SMALL
  8,  // REALFONT_MEDIUM
  18  // REALFONT_INTERFACE
};

realFontEnum_t realFont = REALFONT_UNKNOWN;
//for glVertex hook passing per character. Reset by R_DrawFont()
int characterIndex = 0;
realFontEnum_t getRealFontEnum(const char* realFont) {
  if (!realFont) return REALFONT_UNKNOWN;
  if (!strcmp(realFont, "title")) return REALFONT_TITLE;
  if (!strcmp(realFont, "small")) return REALFONT_SMALL;
  if (!strcmp(realFont, "medium")) return REALFONT_MEDIUM;
  if (!strcmp(realFont, "interface")) return REALFONT_INTERFACE;
  return REALFONT_UNKNOWN;
}

/*
	It seems easier than imagined, because SoF calls Draw_String, instead of Draw_Char.
	Draw_String calls Draw_Char.
	If we hook Con_DrawConsole(), set a boolean,

	Hook glVertex2f()
	check the boolean:
	scale it.
*/
/*
//Draw_String(int, int, const char *, int)
void my_Draw_String(int a,int b,int c,int d) {
	isFontInner = true;
	orig_Draw_String(a,b,c,d);
	isFontInner = false;
}
//int, int, char const *, int, paletteRGBA_c &
void hkDraw_String_Color(int a,int b,int c,int d, int e) {
	isFontInner = true;
	oDraw_String_Color(a,b,c,d,e);
	isFontInner = false;
}

void my_Draw_Char(int a, int b, int c, int d)  {
	isFontInner = true;
	orig_Draw_Char(a,b,c,d);
	isFontInner = false;
}
*/
float draw_con_frac = 1.0;
//int, int, int, int, paletteRGBA_c &, char const *, unsigned int
//glVertex(x,y) -> glVertex(x+w,y+h)
// Note: hkDrawStretchPic function moved to dispatchers/ref_gl.cpp

//SOF FORMULA : vid_width - 40 * vid_width/640 - 16
/*
	gl_mode 0 2560 = yEye = 60, yLogo = 98, difference = 38
	gl_mode 3 640 = yEye = 20, yLogo = 58, difference = 38, 32 for logo, so 6 fixed margin.

	scaled_border = 20
	fixed_margin = 6

	x,y == TOP LEFT.
*/
void hkDrawPicOptions(int x, int y, float w_scale, float h_scale, int pal, char * name) {
  if (hudDmRanking) {
    float x_scale = static_cast<float>(current_vid_w) / 640.0f;
    int offsetEdge = 40; //36??
    if (hudDmRanking_wasImage) {
      // PrintOut(PRINT_LOG,"Logo ypos = %i\n",y);
      //2nd image, means we in spec chasing someone. This image is a DM Logo.
      x = current_vid_w - 32 * hudScale - offsetEdge * x_scale;
      // y = y + (hudScale-1)*32;
      //go to start of border by removing fixed margin, add previous logo + new scaling margin
      y = 20 * screen_y_scale + 32 * hudScale + 6 * screen_y_scale;

      w_scale = hudScale;
      h_scale = hudScale;
    } else {
      // PrintOut(PRINT_LOG,"Eye ypos = %i\n",y);
      //pics/interface2/eye = 32x32
      // PrintOut(PRINT_LOG,"%s:: x : %i, y : %i\n",name,x,y);
      // PrintOut(PRINT_LOG,"%s:: w : %f, h : %f\n",name,w,h);
      //x = 2384, y = 60 at gl_mode 0 2560x1440
      //x = 584, y = 20 at gl_mode 3 640x480
      //x position is off by 12 pixels, suggesting a ride side bias, not taking pic width into consideration.
      //assume they wanted a 640-584=56 , 56-32 = 24

      //Compare 2 screenshots to see if a larger image gets shifted to the left
      //Ok so it maintains its X position, when it increases in size.

      //When the higher resolution is used, it is calculating the X position as a % from edge.
      //This is fine, it either matches the left edge of the image or the right.
      //It is impossible to match both, else image would get larger/smaller with reso.
      //But by positioning left edge, the right border will appear larger on higher reso.
      //Which is why i preferred to posititon it based on the right edge.

      //Left Edge value = -56, Right Edge value would be -56-32 = -24.

      //Used 48 instead because not enough space in 640x480 otherwise.
      //We could just use 56 again perhaps.

      //Since we are scaling the image.
      //Correct its position

      //We might have to NOP out SCR_DirtyRect calls
      //Or just leave them and do an extra SCR_DirtyRect call.

      // float y_scale = vid_height/480; 
      x = current_vid_w - 32 * hudScale - offsetEdge * x_scale;
      //SOF FORMULA : vid_width - 40 * vid_width/640 - 16 
      w_scale = hudScale;
      h_scale = hudScale;
    }
    // To help R_DrawFont know if teamIcon image was drawn.
    hudDmRanking_wasImage = true;

  } else if (hudInventoryAndAmmo) {
    static bool secondDigit = false;

    // //Special Interface Font Wooo (also used in SoFPlus cl_showfps 2)
    float x_scale = static_cast < float > (current_vid_w) / 640.0f;

    // //Bottom-Right Corner - frame_bottom
    x = static_cast < float > (current_vid_w) - 13.0f * x_scale - 63.0f * hudScale;
    if (!secondDigit) x += 18.0f * hudScale;
    // if (left) x -= test2->value * hudScale; //width

    y = static_cast < float > (current_vid_h) - 3.0f * screen_y_scale - 49.0f * hudScale;
    // if (top) y -= 16.0f * hudScale; //height

    w_scale = hudScale;
    h_scale = hudScale;

    if (secondDigit) secondDigit = false;
    else secondDigit = true;
  }

  oDrawPicOptions(x, y, w_scale, h_scale, pal, name);
}

/*
	Used by Inventory + Ammo
	And HealthArmor.

	@2560*1440 : MIDWAY = 720px
	Cropped X: 1200 Y:1398 C1X:3 C1Y:1 C2X:158 C2Y:28 NAME:pics/interface2/frame_health
	VERTEX 1: 1200.000000 1398.000000
	VERTEX 2: 1355.000000 1398.000000
	VERTEX 3: 1355.000000 1425.000000
	VERTEX 4: 1200.000000 1425.000000
	WIDTH: 155
	HEIGHT: 27
	Y_OFFSET: 15
	X_DIST_MID: 480 .. 160
	Cropped X: 1226 Y:1409 C1X:2 C1Y:3 C2X:126 C2Y:12 NAME:pics/interface2/health
	VERTEX 1: 1226.000000 1409.000000
	VERTEX 2: 1350.000000 1409.000000
	VERTEX 3: 1350.000000 1418.000000
	VERTEX 4: 1226.000000 1418.000000
	WIDTH: 124
	HEIGHT: 9
	Y_OFFSET: 22
	X_OFFSET_TO_FRAME: 26
	X_DIST_MID: 506 .. 169
	Cropped X: 1216 Y:1398 C1X:57 C1Y:1 C2X:162 C2Y:13 NAME:pics/interface2/armor
	VERTEX 1: 1216.000000 1398.000000
	VERTEX 2: 1321.000000 1398.000000
	VERTEX 3: 1321.000000 1410.000000
	VERTEX 4: 1216.000000 1410.000000
	WIDTH: 105
	HEIGHT: 12
	Y_OFFSET: 30
	X_OFFSET_TO_FRAME: 16
	X_DIST_MID: 496 .. 165

*/
void hkDrawCroppedPicOptions(int x, int y, int c1x, int c1y, int c2x, int c2y, int palette, char * name) {

  if (hudInventoryAndAmmo) {
    if (!strncmp(name, "pics/interface2/", 16)) {

      if (!strncmp(name + 16, "frame_", 6)) {
        // All used on both left(items) and right(guns).
        if (!strcmp(name + 22, "bottom")) {
          // texture around bottom of default number display
          // This has icon of item pickup and Fonts for count of ammo and item count.
          if (hudInventory_wasItem) {
            //Gun+Ammo
            hudCroppedEnum = ITEM_INVEN_BOTTOM;
          } else {
            //Item
            hudCroppedEnum = GUN_AMMO_BOTTOM;
          }
        } else if (!strcmp(name + 22, "top2")) {
          // PrintOut(PRINT_LOG,"frame_top2");
          //texture around top of default number display
          if (hudInventory_wasItem) {
            // PrintOut(PRINT_LOG,"frame_top2 GUN");
            //Gun+Ammo
            hudCroppedEnum = ITEM_INVEN_TOP;
          } else {
            // PrintOut(PRINT_LOG,"frame_top2 INVEN");
            //Item
            hudCroppedEnum = GUN_AMMO_TOP;
          }
        } else if (!strcmp(name + 22, "top")) {
          //texture around top of when you switch weapons or items, whilst it shows their naem in text.
          if (hudInventory_wasItem) {
            //Gun+Ammo
            hudCroppedEnum = ITEM_INVEN_SWITCH;
          } else {
            //Item
            hudCroppedEnum = GUN_AMMO_SWITCH;
          }
        } else if (!strcmp(name + 22, "lip")) {
          //tiny pixels that appear when you switch weapons or items, whilst it shows thier name in text.
          if (hudInventory_wasItem) {
            //Gun+Ammo
            hudCroppedEnum = ITEM_INVEN_SWITCH_LIP;
          } else {
            //Item
            hudCroppedEnum = GUN_AMMO_SWITCH_LIP;
          }
        }
      } else if (!strncmp(name + 16, "item_", 5)) {
        // cGunAmmo2::DrawDescription() (Icon of Active Item)
        if (!strcmp(name + 21, "c4") || !strcmp(name + 21, "flash") || !strcmp(name + 21, "neuro") || !strcmp(name + 21, "nightvision") ||
          !strcmp(name + 21, "claymore") || !strcmp(name + 21, "medkit") || !strcmp(name + 21, "h_grenade") ||
          !strcmp(name + 21, "flag")) {
          hudCroppedEnum = ITEM_ACTIVE_LOGO;
        }
      }
      //There is 2 veresions of cInterface:DrawNum() that take different paramters and do different things
      //One is a wrapper around the other, just to take in different argument types.
      //It doesnt' target +moveup, this is confusion in IDA, it is just negative offset from it.
      //Really pointing to the no_0 no_1 no_2 no_3 no_4 no_5 etc m32 files in pics/interface2/
    } //pics/interface2
  } else if (hudHealthArmor) {
    //@ 2560x1440 - TopLeft Corner is position Of image.
    //Cropped: X:1200 Y:1398 C1X:3 C1Y:2 C2X:158 C2Y:28 NAME:pics/interface2/frame_health

    if (!strcmp(name, "pics/interface2/frame_health")) {
      // if ( c2x - c1x == 7 ) 
      // 	PrintOut(PRINT_LOG,"Cropped: X:%i Y:%i C1X:%i C1Y:%i C2X:%i C2Y:%i NAME:%s\n",x,y,c1x,c1y,c2x,c2y,name);
      hudCroppedEnum = HEALTH_FRAME;
    } else if (!strcmp(name, "pics/interface2/health")) {
      hudCroppedEnum = HEALTH;
    } else if (!strcmp(name, "pics/interface2/armor")) {
      hudCroppedEnum = ARMOR;
    } else if (!strcmp(name, "pics/interface2/frame_stealth")) {
      hudCroppedEnum = STEALTH_FRAME;
    }
  }
  croppedWidth = c2x - c1x;
  croppedHeight = c2y - c1y;

  //Lets do vertex manipulation
  oDrawCroppedPicOptions(x, y, c1x, c1y, c2x, c2y, palette, name);

  //Reset it.- in case we missed one.
  hudCroppedEnum = OTHER_UNKNOWN;
}

//small(6px), medium(8px), title(12px), interface(18px)
//type in console: menu fonts
//to see them as example in action.
//imagelist shows size of m32 images.
//The formula for image position in default SoF is:
//vid_width - 40 * vid_width/640 - 16 
void hkR_DrawFont(int screenX, int screenY, char * text, int colorPalette, char * font, bool rememberLastColor) {
  // static bool wasHudDmRanking = false;
  realFont = getRealFontEnum((char*)(* (int * )(font + 4)));
  if (hudDmRanking) {
    static bool scorePhase = true;

    int fontWidth = 12;
    int offsetEdge = 40;
    /*
    if (!strcmp(realFont,"title")) {
    	fontWidth = 12;
    } else if (!strcmp(realFont,"small")) {
    	fontWidth = 6; 7????
    } else if (!strcmp(realFont,"medium")) {
    	fontWidth = 8;
    } else if (!strcmp(realFont,"interface")) {
    	fontWidth = 18px;
    }
    */

    /*
    if ( !wasHudDmRanking ) {
    	//score first is centered.

    	screenX = current_vid_w - 32*hudScale - 24*(current_vid_w/640) + hudScale*16 - fontWidth*strlen(text)/2;

    } else {
    	//position in dmrank
    	screenX = current_vid_w - 32*hudScale - 24*(current_vid_w/640) + hudScale*16 - fontWidth*strlen(text)/2;
    }
    */

    //Text is only drawn in:
    //Spec Chase Mode
    //Spec OFF 
    float x_scale = static_cast<float>(current_vid_w) / 640.0f;

    if (hudDmRanking_wasImage) {
      //We are SPEC or TEAM DMMODE playing.
      if ( * (int * ) 0x201E7E94 == 7) { //PM_SPECTATOR PM_SPECTATOR_FREEZE
        //PLAYER NAME IN SPEC CHASE MODE.

        // We are in spec chasing someone, so this font is the name of the player we chase.
        // There is an eye and a LOGO above us, if in TEAM DM
        // x = current_vid_w - 32*hudScale - offsetEdge*x_scale;
        //Preserve their centering of text calculation.(Don't want to bother with colorCodes).
        // screenX += 56 * x_scale;
        //40 is a magic number here.
        //It uses 40 as scaling border, and the remaining is 17px.

        //Why is this negative?
        //Oh because it extends to the left, for long names
        int center_offset = screenX - (current_vid_w - (offsetEdge * x_scale));
        // PrintOut(PRINT_LOG,"Center offset is %i\n",center_offset);
        // screenX -= -16 + 32*hudScale - center_offset*(hudScale-1);
        // screenX = current_vid_w - 40 * x_scale - 16 
        //x = width - scaled_border - scaledHud/2 + centered_text
        screenX = current_vid_w - offsetEdge * x_scale - 16 * hudScale + center_offset;
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
    // PrintOut(PRINT_LOG,"%s : %s : %i\n",text,realFont,someFlag);
    // __asm__("int $3");

    // wasHudDmRanking = true;
  }
  else if (hudInventoryAndAmmo) {
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
        set_x = screenY;
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
        set_x = screenY;
        set_y = screenY;
        //top-right
        drawCroppedPicVertex(true,false,set_x,set_y);
        hudCroppedEnum = before;

        screenX = set_x + -79.0f * hudScale;
        screenY = set_y;
      }
    }
    /*
		if ( !strcmp(text,"18")) {
			__asm__("int $3");
		}
    */
		// PrintOut(PRINT_LOG,"Inventory UI Font: %s\n",text);
	}
  oR_DrawFont(screenX, screenY, text, colorPalette, font, rememberLastColor);

  characterIndex = 0;
}

void my_SCR_CenterPrint(char * text) {
  PrintOut(PRINT_LOG, "SCR_CenterPrint()\n");
  // orig_SCR_CenterPrint(text);
}

/*
	Scoreboard Font scale
	Only called in one location (SCR_UpdateScreen)

	Draw_StretchPic(int x, int y, int size_width, int size_height, paletteRGBA_c &, char const *, unsigned int)
	Draw_Pic(int x, int y, char const *, paletteRGBA_c)
	Draw_Char(int x, int y, int, paletteRGBA_c &)
	Draw_String_Color(int x, int y, char const * text, int length, paletteRGBA_c &)
	SP_GetStringText(ushort)
	char *COM_Parse (char **data_p)
	va(char *,...)

	The plan:
		Tried multiply the vertex positions by 2.
		This should work in theory.
		When you multiply them by 2, their width and height increases.
		But the position changes too.

		We must measure the distance from the center of the screen, initially, before scale.
		So after we scale it, we can reposition them again.

		So:
		  Save Position.
		  Scale.
		  Restore Position.
		
		Have to do it all at once.
		Doesn't make sense.
		Ah.
		That was the idea?
		To delay the functions, make them dummy, so that width could be calculated.

		TexCoord1
		Vertex1
		TexCoord2
		Vertex2
		TexCoord3
		Vertex3
		TexCoord4
		Vertex4


*/
void hkSCR_ExecuteLayoutString(char * text) {
  isDrawingLayout = true;
  oSCR_ExecuteLayoutString(text);
  isDrawingLayout = false;
}

/*
SCOREBOARD:
-160 + somethingx
-120 + somethingy

Length is seems to be max length. =  1024 fixed.
*/
void hkDraw_String_Color(int x, int y, char
  const * text, int length, int colorPalette) {
  //Why is fontScale stuck at 2.0??
  /*
  	//adjust position here for scale mb.
  	if ( isDrawingLayout) {
  		// PrintOut(PRINT_LOG,"%s Length is they:%i us:%i raw:%i\n",text,length, strlen_custom(text),strlen(text));
  		// int subx = (fontScale * 8.0f * strlen_custom(text))/2;

  		// y = y - (fontScale * 8.0f * length)/2;
  		// PrintOut(PRINT_LOG,"Precise fontScale: %.10f\n", fontScale); // Print with many decimal places
  		if ( fontScale > 1.0f ) {
  			// y = y + ((fontScale-1) * test->value);
  			int subx = (fontScale * test->value * 8.0f);
  			// PrintOut(PRINT_LOG,"Subtracting %i from x\n",subx);
  			x -= subx;
  		} else if ( fontScale < 1.0f) {
  			y = y - ((1-fontScale) * test->value);
  			int subx = (fontScale * test->value * 8.0f);
  			// PrintOut(PRINT_LOG,"Subtracting %i from x\n",subx);
  			x += subx;
  		}
  		
  	}
  	*/
  oDraw_String_Color(x, y, text, length, colorPalette);
}

int * cls_state = (int*)0x201C1F00;
/*
From JediKnight
typedef enum {
	CA_UNINITIALIZED,
	CA_DISCONNECTED, 	// not talking to a server
	CA_CONNECTING,		// sending request packets to the server
	CA_CHALLENGING,		// sending challenge packets to the server
	CA_CONNECTED,		// netchan_t established, getting gamestate
	CA_LOADING,			// only during cgame initialization, never during main loop
	CA_PRIMED,			// got gamestate, waiting for first frame
	CA_ACTIVE,			// game views should be displayed
	CA_CINEMATIC		// playing a cinematic or a static pic, not connected to a server
} connstate_t;

typedef enum
{
	ca_uninitialized,
	ca_disconnected=1,
	ca_connecting=7,
	ca_connected,
	ca_active=8
} connstate_t;
*/

/*
	frac calculates the height/pixel-space of the console.

	consoleSize is calculated as _sofbuddy_console_size/fontScale
	fontScale is calculated as _sofbuddy_font_scale (rounded)

	Actually lines in q2 src refers to the pixel-space taken by all lines.
	To get number of rows, you do lines / 8, if each character is 8px tall.
	Since we scale the character height, it should be lines / 8*fontScale ?

	The bigger the font, the less lines. 

	int lines = draw_con_frac * vid_h;
	y = -vid_h + lines;
	oDrawStretchPic(x,y,w,h,palette,name,flags);

	Higher fontScale means less lines.
	We want to preserve consoleHeight.

	con.linewidth is faked in con_checkresize()

	There is rare crash with the console system, not sure what causes it. (This got solved. Was not sof_buddy).
*/
void hkCon_DrawConsole(float frac) {
  draw_con_frac = frac;
  isFontOuter = true;

  //!( *cls_state != 1 && *cls_state != 8
  if (frac == 0.5 && * cls_state == 8) {
    //When loading screen console, half screen is blanked out.
    //Trying to target when in-game console _only_

    //Whilst in-game...

    //_sofbuddy_console_size is stored raw; apply fontScale at runtime so cvars are independent
    //Smaller frac means fewer lines (pixel-space); larger fontScale reduces visible lines
    frac = consoleSize / fontScale;

    /*
    	lines = viddef.height * frac;
    	less frac means less lines(pixel-space)

    	for (i=0 ; i<rows ; i++, y-=8, row--)
    	{
    		//Draw Row..
    	}
    */

    //adjust consoleHeight for backgroundPic
    draw_con_frac = _sofbuddy_console_size -> value;

  } else {
    //While not in-game...
    //adjust rows
    //Essentially is frac = 1/fontScale, becasue fullscreen whilst disconnected.
    frac = frac / fontScale;
  }

  oCon_DrawConsole(frac);
  isFontOuter = false;
}

int real_refdef_width = 0;
void hkCon_DrawNotify(void) {
  real_refdef_width = current_vid_w;
  * viddef_width = 1 / fontScale * current_vid_w;
  isFontOuter = true;
  oCon_DrawNotify();
  isFontOuter = false;
  * viddef_width = real_refdef_width;
}

/*
	This functions' job is :
	  Do we have to a set a new con.linewidth due to resolution change or fontScale now.

	con.text buffer is populated by Con_Print(), it is not re-created each frame
	thus, changing fontScale cannot unwrap previous wrapped lines.
	So when changing from larger font to smaller font, lines are over-wrapped.
	When changing from smaller font to larger font, lines extend off screen.
	Only solution: re-initailise console?
	Clearing console is a bad idea because lose information for user.
	When user changes resolution, he faces same issue as font change, so doesn't matter.
*/
void hkCon_CheckResize(void) {
  //This makes con.linewidth smaller in order to reduce the character count per line.
  int viddef_before = current_vid_w;
  * viddef_width = 1 / fontScale * current_vid_w;
  oCon_CheckResize();
  * viddef_width = viddef_before;
}

/*
	More efficient than many if statements in low level glVertex() call.

	TODO: Don't use hudScale-1 so that we can use scales < 1.
	UPDATE: Seems hudScale-1 works even for scales < 1

	frame_health
		WIDTH: 155
		HEIGHT: 27
		Y_OFFSET: 15
		TOP_PIXEL: 42
	health
		WIDTH: 124
		HEIGHT: 9
		Y_OFFSET: 22
		PIXLES_INTO_FRAME_FROM_BOTTOM_EDGE: 7
		TOP_PIXEL: 29
	armor
		WIDTH: 105
		HEIGHT: 12
		Y_OFFSET: 30
		PIXELS_INTO_FRAME_FROM_BOTTOM_EDGE: 15
		TOP_PIXEL: 42


	Fixed Vertical Border of 15.

	FrameY_upper = current_vid_h - 15*y_scale - 27*hudScale
	FrameY_lower = current_vid_h - 15*y_scale

	healthY_upper = current_vid_h - 15*y_scale - 16*hudScale
	healthY_lower = current_vid_h - 15*y_scale - 7*hudScale

	armorY_upper = current_vid_h - 15*y_scale - 27*hudScale
	armorY_lower = current_vid_h - 15*y_scale - 15*hudScale

	The issue with scaling like this is when the transparency around image is not symmetric.
	So it actually moves its position relative to other objects when its scaled up?
	Is that even true?

	HP and Armor behaving ... Weird problem.
	I think it is related to symmetry.
	Eg. if you scale up 50% on left side only, but all the data is on left side of image.
	You only scale the image by 50%, instead of 100%? But it was already that small and fitted. so should hold.

	Parent fully symmetric: expand outwards both direction 50% each way.

	HEALTH_OFFSET.
	100hp/(124px + 4/9) == 55hp/(68px+4/9)
*/
// float combined_verts[4][2];

/*
	Vertex1 is the position of the object. So TopLeft Of Image.

1398 - top frame
1414 - top buzzer
1421 - low buzzer
1425 - lower frame
1440 - egde screen

x position of frame.
300 -> 455

155 px long
338.75?
339
300 -> 339 ??
39 px long.


linux screenshot frame width = 396-241 = 155
320 is mid.
320 - 241 = 79
396 - 320 = 76

How we got 1200 -> 1355
2560 /2 = 1280

1280-1200 = 80
1355-1280 = 75

center_x-79

center_x - 60 == start_pos.
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
    	// orig_Com_Printf("Stealth Width = %i:%i\n",croppedWidth,croppedHeight);
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

void __stdcall my_glVertex2f_CroppedPic_1(float x, float y) {
  //top-left
  drawCroppedPicVertex(true, true, x, y);
  // orig_glVertex2i(static_cast<int>(std::round(x)), static_cast<int>(std::round(y)));
  orig_glVertex2f(x, y);
}
void __stdcall my_glVertex2f_CroppedPic_2(float x, float y) {
  //top-right
  drawCroppedPicVertex(true, false, x, y);
  // orig_glVertex2i(static_cast<int>(std::round(x)), static_cast<int>(std::round(y)));
  orig_glVertex2f(x, y);
}
void __stdcall my_glVertex2f_CroppedPic_3(float x, float y) {
  //bottom-right
  drawCroppedPicVertex(false, false, x, y);
  // orig_glVertex2i(static_cast<int>(std::round(x)), static_cast<int>(std::round(y)));
  orig_glVertex2f(x, y);
}
void __stdcall my_glVertex2f_CroppedPic_4(float x, float y) {
  //bottom-left
  drawCroppedPicVertex(false, true, x, y);
  // orig_glVertex2i(static_cast<int>(std::round(x)), static_cast<int>(std::round(y)));
  orig_glVertex2f(x, y);
}

/*
===============================================================
//For ui-font scaling ( _NOT_ DrawString DrawChar from console+notify. )
============================================================
*/
/*
enum realFontEnum_t {
  REALFONT_TITLE,
  REALFONT_SMALL,
  REALFONT_MEDIUM,
  REALFONT_INTERFACE,
  REALFONT_UNKNOWN
};
The direct multiplication of glVertex parameters worked with console text because
the characters existed all the way from x=0, thus there was no lost from initial scale.
eg. imagine starting at x = 100, multiply by 2, now font starts at x=200.
it has lost position.
If start at x=0, its just changing a sequence of 0,2,4,6,8,10 to ... 0,4,8,12,12,16,20

*/

float pivotx;
float pivoty;

//This new character (iteration inside of r_drawfont), needs shift along further
//Thus we need concept of character index. R_StrLen + size.
void __stdcall my_glVertex2f_DrawFont_1(float x, float y) {
  //top-left
  //Anchor-point fixed top-left
  //When we resize images, we use center as anchor-point, might be an issue.
  pivotx = x;
  pivoty = y;
  if ( hudInventoryAndAmmo || hudDmRanking ) {
    orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(hudScale-1), y);
  } else 
  orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
}

void __stdcall my_glVertex2f_DrawFont_2(float x, float y) {
  //top-right
  if ( hudInventoryAndAmmo || hudDmRanking ) {
    x = pivotx + (x - pivotx) * hudScale;
    orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(hudScale-1) , y);
  } else {
  x = pivotx + (x - pivotx) * screen_y_scale;
  orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1) , y);
  }
}

void __stdcall my_glVertex2f_DrawFont_3(float x, float y) {
  //bottom-right
  if ( hudInventoryAndAmmo || hudDmRanking ) {
    x = pivotx + (x - pivotx) * hudScale;
    y = pivoty + (y - pivoty) * hudScale;
    orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(hudScale-1), y);
  } else {
    x = pivotx + (x - pivotx) * screen_y_scale;
    y = pivoty + (y - pivoty) * screen_y_scale;
    orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
  }
  
}

void __stdcall my_glVertex2f_DrawFont_4(float x, float y) {
  //bottom-left
  if ( hudInventoryAndAmmo || hudDmRanking ) {
    y = pivoty + (y - pivoty) * hudScale;
    orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(hudScale-1), y);
  } else {
    y = pivoty + (y - pivoty) * screen_y_scale;
    orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
  }
  

  characterIndex++;
}

/*
	Because when rescaling font, its also intended to reposition font, this technique works.
	However its not so simple for HUD rescaling, as their position has to remain same.

	TODO: Could move this into its own hook too! (like CroppedPic)

	Below 2^24 all integers can be represented exactly by 32 bit floats.

	When Quake 2 is using inputs as whole numbers, its fine. If we multiply them by a non whole number its not fine.
	Should we round? But rounding is still bad, because why should it fill the next pixel? Pixels span across 0->1.
	If the vertex lands between those values, its worthy to fill the pixel.

	This makes believe that rounding to whole integers can do more damage. The floating precision needs to be preserved.
*/
void __stdcall hkglVertex2f(float x, float y) {
  if ((isFontOuter && !isNotFont)) {
    //Preserving the actual vertex position then letting it map naturally to pixels seems better than rounding
    orig_glVertex2f(x * fontScale, y * fontScale);
    return;
  } else if (isDrawingLayout) {
    //This is vertices for font and images

    static int vertexCounter = 1;
    static float x_mid_offset;
    static float y_mid_offset;

    static float x_first_vertex;
    static float y_first_vertex;

    if (vertexCounter == 1) {
      // PrintOut(PRINT_LOG,"Vertex: %f\n",x);
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
    orig_glVertex2f(x * hudScale - x_first_vertex * (hudScale - 1) - x_mid_offset * (hudScale - 1),
      y * hudScale - y_first_vertex * (hudScale - 1) - y_mid_offset * (hudScale - 1));

    vertexCounter++;
    if (vertexCounter > 4) vertexCounter = 1;
    return;
  } else if (isDrawPicCenter) 
  {
    //Default DrawPic situation.
    #if 1
    static int vertexCounter = 1;
    
    if (vertexCounter == 1) {
      DrawPicPivotCenterX = x + DrawPicWidth * 0.5f;
      DrawPicPivotCenterY = y + DrawPicHeight * 0.5f;

      // Ah it breaks TILED .m32, I didn't know there was such a thing as tiling.
      // Same problem as anything laid out in sequence, needs to be repositioned.
      // Should really targer higher level draw routines, for this reason.

      // But such sequences, if start from pos=0, benefit from multiply co ordinates.

      // orig_Com_Printf("%i %i\n",DrawPicWidth,DrawPicHeight);

      if ( DrawPicWidth == 0 || DrawPicHeight == 0 ) {
        // DrawPicWidth was not set, Draw_FindImage
        orig_Com_Printf("This is not supposed to happen!\n");
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
    #endif
  } else if ( isDrawPicTiled ) {
    /*
      For rare tile-based (pivot-point 0) textures. (eg. backdrop main menu)
    */
    //orig_Com_Printf("x: %f ... y: %f\n",x,y);
    // orig_glVertex2f(x * current_vid_h/480, y*current_vid_h/480);
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

/*
	Draw_StretchPic()
	R_DrawFont(mode=small)
*/
void __thiscall my_cScope_Draw(void * self) {
  orig_cScope_Draw(self);
}

/*
	Draw_CroppedPicOptions()
	cInventory2::GetCounts(void)
	cInventory2::GetName(int)
	R_DrawFont(mode=small)
*/
void __thiscall hkcInventory2_And_cGunAmmo2_Draw(void * self) {
  //This is the ammo (bottom right) and inventory (bottom left)
  // So positioned from left and bottom edge and right and bottom edge

  
  hudInventoryAndAmmo = true;
  ocInventory2_And_cGunAmmo2_Draw(self);
  hudInventoryAndAmmo = false;

  if (hudInventory_wasItem) {
    hudInventory_wasItem = false;
  } else {
    hudInventory_wasItem = true;
  }
}

/*
	SCR_DirtyRect()
	Draw_CroppedPicOptions()
*/
void __thiscall hkcHealthArmor2_Draw(void * self) {
  //This is the health and armour bar at bottom
  //positioned from bottom edge of screen, centered horizontally.

  hudHealthArmor = true;
  ocHealthArmor2_Draw(self);
  hudHealthArmor = false;
}

/*
	SCR_DirtyRect()
	R_DrawFont(mode=title)
*/
void __thiscall my_cCountdown_Draw(void * self) {
  orig_cCountdown_Draw(self);
}

/*
	SCR_DirtyRect()
	R_DrawFont(mode=small)
*/
void __thiscall my_cInfoTicker_Draw(void * self) {
  orig_cInfoTicker_Draw(self);
}

/*
	SCR_DirtyRect()
	R_DrawFont(mode=medium && mode=title)
*/
void __thiscall my_cMissionStatus_Draw(void * self) {
  orig_cMissionStatus_Draw(self);
}

/*
	SCR_DirtyRect()
	Draw_GetPicSize()
	Draw_PicOptions()
	R_DrawFont(mode=small && mode=title)

	In spectator mode in CTF, if you follow a player, it also shows the team LOGO below the eye.
*/
void __thiscall hkcDMRanking_Draw(void * self) {
  // Top right of screen, your score and rank
  // So relative to top and right edge

  hudDmRanking = true;
  ocDMRanking_Draw(self);
  hudDmRanking = false;
  hudDmRanking_wasImage = false;
}

/*
	SCR_DirtyRect()
	Draw_StretchPic()
*/
void __thiscall hkcCtfFlag_Draw(void * self) {

  hudStretchPic = true;
  //Whether you are carrying the flag or the flag is missing.
  //Top left of screen, so relative to top and left edge.
  orig_cCtfFlag_Draw(self);
  hudStretchPic = false;

}

/*
	SCR_DirtyRect()
	Draw_StretchPic()
	SCR_DrawString()
*/
void __thiscall my_cControlFlag_Draw(void * self) {
  orig_cControlFlag_Draw(self);
}

/*
===================================================================================================
	CVar Change Callbacks
===================================================================================================
*/

void consolesize_change(cvar_t * cvar) {
  // Save raw console size from cvar; apply fontScale at runtime in my_Con_DrawConsole
  consoleSize = cvar->value;
}

void fontscale_change(cvar_t * cvar) {
  static bool first_run = true;
  //round to nearest quarter
  fontScale = roundf(cvar->value * 4.0f) / 4.0f;
  // PrintOut(PRINT_LOG,"Precise fontScale change: %.10f\n", fontScale);

  //Protect from divide by 0.
  if (fontScale == 0) fontScale = 1;

  first_run = false;
}

void hudscale_change(cvar_t * cvar) {
  //round to nearest quarter
  hudScale = roundf(cvar->value * 4.0f) / 4.0f;

}

/*
===================================================================================================
	Lifecycle Callback Implementations
===================================================================================================
*/

static void scaledFont_EarlyStartup(void)
{
	PrintOut(PRINT_LOG, "scaled_font: Early startup - applying memory patches\n");
	
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
	
  // All hooks are now automatically registered via REGISTER_HOOK macros
	PrintOut(PRINT_LOG, "scaled_font: All hooks registered automatically\n");
	
	PrintOut(PRINT_LOG, "scaled_font: Early startup complete\n");
}


/*
	RefDllLoaded Callback
	
	Called when ref.dll is loaded. This is where we install hooks to ref.dll functions
	(addresses starting with 0x300xxxxx) for rendering and vertex manipulation.
*/
static void scaledFont_RefDllLoaded(void)
{
	PrintOut(PRINT_LOG, "scaled_font: Installing ref.dll hooks\n");
	
	// Get OpenGL vertex function pointers from ref.dll
	void* glVertex2f = (void*)*(int*)0x300A4670;
	orig_glVertex2i = (void(__stdcall*)(int,int))*(int*)0x300A46D0;
	
	// Hook OpenGL vertex functions for font/HUD scaling
	// glVertex2f is handled specially since its address is resolved at runtime
	DetourRemove((void**)&orig_glVertex2f);
	orig_glVertex2f = (void(__stdcall*)(float,float))DetourCreate((void*)glVertex2f, (void*)&hkglVertex2f, DETOUR_TYPE_JMP, DETOUR_LEN_AUTO);
	
	/*
		Hooks are now automatically registered using REGISTER_HOOK_LEN macros.
		The hook manager intelligently checks addresses to determine which module they belong to.
		No manual DetourCreate calls needed for the following functions:
		- DrawStretchPic
		- DrawPicOptions  
		- DrawCroppedPicOptions
		- R_DrawFont
		- Draw_String_Color
	*/
	
	// Required for CroppedPicOptions Vertex scaling - patch each corner of rectangle
	WriteE8Call((void*)0x3000239E, (void*)&my_glVertex2f_CroppedPic_1);
	WriteByte((void*)0x300023A3, 0x90);
	
	WriteE8Call((void*)0x300023CC, (void*)&my_glVertex2f_CroppedPic_2);
	WriteByte((void*)0X300023D1, 0X90);
	
	WriteE8Call((void*)0x300023F6, (void*)&my_glVertex2f_CroppedPic_3);
	WriteByte((void*)0x300023FB, 0X90);
	
	WriteE8Call((void*)0x3000240E, (void*)&my_glVertex2f_CroppedPic_4);
	WriteByte((void*)0x30002413, 0X90);

	/*
	  Actual resizing on fonts
	  hudInventoryAndAmmo || hudDmRanking
	  All Font used in Menus
	*/
	//R_DrawFont
	WriteE8Call((void*)0x30004860, (void*)&my_glVertex2f_DrawFont_1);
	WriteByte((void*)0x30004865, 0X90);

	WriteE8Call((void*)0x30004892, (void*)&my_glVertex2f_DrawFont_2);
	WriteByte((void*)0x30004897, 0X90);

	WriteE8Call((void*)0x300048D2, (void*)&my_glVertex2f_DrawFont_3);
	WriteByte((void*)0x300048D7, 0X90);

	WriteE8Call((void*)0x30004903, (void*)&my_glVertex2f_DrawFont_4);
	WriteByte((void*)0x30004908, 0X90);
	
	PrintOut(PRINT_LOG, "scaled_font: ref.dll hooks installed successfully\n");
}

/*
	my_Con_Init - Console Initialization Hook
	
  We create cvars here, because its required this early for this module.
  Internally orig_Con_Init calls Con_CheckResize, so we hook it too.
	- Hook Con_CheckResize for dynamic line width adjustment
*/
void hkCon_Init(void) {
  PrintOut(PRINT_LOG, "scaled_font: Registering CVars\n");
	
	// Register all font scaling CVars
	create_scaled_fonts_cvars();

  // Call original Con_Init (initializes console buffer, etc.)
  oCon_Init(); 
}

#endif