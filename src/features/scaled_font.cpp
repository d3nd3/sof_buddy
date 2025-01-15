#include "sof_buddy.h"
#include <math.h>

#include "sof_compat.h"
#include "features.h"

#include "../DetourXS/detourxs.h"

#include "util.h"

#ifdef FEATURE_FONT_SCALING


void (*orig_Draw_Char) (int, int, int, int) = NULL;
void (*orig_Draw_String)(int a,int b,int c,int d) = NULL;
void (*orig_Draw_String_Color)(int a,int b,int c,int d, int e) = NULL;
void (*orig_Con_DrawConsole) (float frac) = NULL;
void (*orig_Con_DrawNotify)(void) = NULL;
void (*orig_Con_CheckResize)(void) = NULL;
void (*orig_Con_Init)(void) = NULL;
void (*orig_Con_Initialize)(void) = 0x20020720;
void (*orig_DrawStretchPic) (int x, int y, int w, int h,int palette, char *name, int flags) = NULL;
void (*orig_SRC_AddDirtyPoint)(int x,int y) = 0x200140B0;

void (__stdcall *orig_glVertex2f)(float one, float two) = NULL;

float fontScale = 1;
float consoleSize = 0.5;
bool isFontOuter = false;
bool isFontInner = false;
bool isNotFont = false;
/*
	It seems easier than imagined, because SoF calls Draw_String, instead of Draw_Char.
	Draw_String calls Draw_Char.
	If we hook Con_DrawConsole(), set a boolean,

	Hook glVertex2f()
	check the boolean:
	scale it.
*/
//Draw_String(int, int, const char *, int)
void my_Draw_String(int a,int b,int c,int d) {
	isFontInner = true;
	orig_Draw_String(a,b,c,d);
	isFontInner = false;
}
//int, int, char const *, int, paletteRGBA_c &
void my_Draw_String_Color(int a,int b,int c,int d, int e) {
	isFontInner = true;
	orig_Draw_String_Color(a,b,c,d,e);
	isFontInner = false;
}
void my_Draw_Char(int a, int b, int c, int d)  {
	isFontInner = true;
	orig_Draw_Char(a,b,c,d);
	isFontInner = false;
}

float draw_con_frac = 1.0;
//int, int, int, int, paletteRGBA_c &, char const *, unsigned int
//glVertex(x,y) -> glVertex(x+w,y+h)
void my_DrawStretchPic(int x, int y, int w, int h,int palette, char *name, int flags) {
	if ( isFontOuter ) {
		// orig_Com_Printf("Yes %i %i %i %i\n",x,y,w,h);
		isNotFont = true;
		int vid_h = *(int*)0x20403660;
		int lines = draw_con_frac * vid_h;
		y = -vid_h + lines;
		orig_DrawStretchPic(x,y,w,h,palette,name,flags);
		orig_SRC_AddDirtyPoint(0,0);
		orig_SRC_AddDirtyPoint(w-1,lines-1);
		isNotFont = false;
		return;
	}
	orig_DrawStretchPic(x,y,w,h,palette,name,flags);
}


int *cls_state = 0x201C1F00; 

/*
	frac calculates the number of rows that can fit vertically on screen.
*/
void my_Con_DrawConsole (float frac) {
	draw_con_frac = frac;
	isFontOuter = true;
	
	if ( frac == 0.5 && !( *cls_state != 1 && *cls_state != 8 ) ) {
		//When loading screen console, half screen is blanked out.
		//Trying to target when in-game console _only_
		frac = consoleSize;
		draw_con_frac = _sofbuddy_console_size->value;
	}
	else frac = frac/fontScale;
	
	orig_Con_DrawConsole(frac);
	isFontOuter = false;
}

int * viddef_width = 0x2040365C;
int real_refdef_width = 0;
void my_Con_DrawNotify(void) {
	real_refdef_width = *viddef_width;
	*viddef_width = 1/fontScale * *viddef_width;
	isFontOuter = true;
	orig_Con_DrawNotify();
	isFontOuter = false;
	*viddef_width = real_refdef_width;
}

/*
	con.text buffer is populated by Con_Print(), it is not re-created each frame
	thus, changing fontScale cannot unwrap previous wrapped lines.
	So when changing from larger font to smaller font, lines are over-wrapped.
	When changing from smaller font to larger font, lines extend off screen.
	Only solution: re-initailise console?
	Clearing console is a bad idea because lose information for user.
	When user changes resolution, he faces same issue as font change, so doesn't matter.
*/
void my_Con_CheckResize(void) {
	//This makes con.linewidth smaller in order to reduce the character count per line.
	int viddef_before = *viddef_width;

	//width = (vidref.width/8) - 2
	*viddef_width = 1/fontScale * *viddef_width;
	orig_Con_CheckResize();
	*viddef_width = viddef_before;
	// orig_Com_Printf("linewidth is now : %i\n",*(int*)0x2024AF98);
}

/*
	Because when rescaling font, its also intended to reposition font, this technique works.
	However its not so simple for HUD rescaling, as their position has to remain same.
*/
void __stdcall my_glVertex2f(float one, float two) {
	if (isFontOuter  && !isNotFont/*&& isFontInner*/) {
		one = one * fontScale;
		two = two * fontScale;
	} 
	orig_glVertex2f(one,two);
}

void consolesize_change(cvar_t * cvar) {
	//eg. a fontScale of 2, would make a frac of 0.5, when consoleSize is 1.0
	//because it is based on the number of lines, and number of lines decrease if font size is larger.
	consoleSize = cvar->value/fontScale;
}

// Modified callback is called within Cvar_Set2() and Cvar_Get()?
void fontscale_change(cvar_t * cvar) {
	static bool first_run = true;
	//round to nearest quarter
	fontScale = roundf(cvar->value * 4.0f) / 4.0f;

	//Protect from divide by 0.
	if (fontScale == 0) fontScale = 1;

	//update consoleSize based on new font
	consolesize_change(_sofbuddy_console_size);

	/*
	// Clear console.
	if ( !first_run && orig_Con_CheckResize ) {
		//Cannot call this before Con_Init, cos references a cvar from Con_Init.
		orig_Con_Initialize();
	}
	*/
	first_run = false; 
}
//config.cfg is already executed at this point. autoexec not (executed at end of CL_Init).
//important as font_scale is read from config.cfg
//VID_Init is called Immediately after this function. Which will call Com_Printf().
//vidref.width is 0 here, resolution has not been set.
//Ok, in Con_Initialise(), it checks whether vidref_width is 0, and defaults to 78.
//Have to put up with no wrapping for everything PRE- VID_Init(), thats life.
void my_Con_Init(void) {
	_sofbuddy_font_scale = orig_Cvar_Get("_sofbuddy_font_scale","1",CVAR_ARCHIVE,NULL);

	//round to nearest quarter
	fontScale = roundf(_sofbuddy_font_scale->value * 4.0f) / 4.0f;
	_sofbuddy_console_size = orig_Cvar_Get("_sofbuddy_console_size","0.5",CVAR_ARCHIVE,NULL);
	//update consoleSize based on new font
	consolesize_change(_sofbuddy_console_size);

	
	//Calls CheckResize, but doesn't matter, vid not intialised, so width is 0, con.linewidth = 78.
	orig_Con_Init();

	//*viddef_width == 0,
	//1/6 *0 == 0 ... 0 << 8
	// orig_Com_Printf("fontscale is : %f %f %i %i\n",_sofbuddy_font_scale->value, fontScale,*(int*)0x2024AF98,*viddef_width);

	
	//Adjusts con.linewidth because less characters fit on line for font-scaling.
	//This should adjust con.lightwidth based on fontScale.
	DetourRemove(&orig_Con_CheckResize);
	orig_Con_CheckResize = DetourCreate((void*)0x20020880,(void*)&my_Con_CheckResize,DETOUR_TYPE_JMP,5);
}


//Called at end of QCommon_Init()
void scaledFont_apply(void) {

	create_scaled_fonts_cvars();
	
	orig_Con_DrawNotify = DetourCreate((void*)0x20020D70 , (void*)&my_Con_DrawNotify,DETOUR_TYPE_JMP,5);
	orig_Con_DrawConsole = DetourCreate((void*)0x20020F90,(void*)&my_Con_DrawConsole,DETOUR_TYPE_JMP,5);

	
}

//Called before SoF.exe entrypoint.
void scaledFont_early(void) {
	
	// for bottom of Con_NotifyConsole remain real width.
	writeIntegerAt(0x20020F6F, &real_refdef_width);

	// NOP SCR_AddDirtyPoint, cos call ourselves with fixed args
	WriteByte(0x20021039,0x90);
	WriteByte(0x2002103A,0x90);
	WriteByte(0x2002103B,0x90);
	WriteByte(0x2002103C,0x90);
	WriteByte(0x2002103D,0x90);

	WriteByte(0x20021049,0x90);
	WriteByte(0x2002104A,0x90);
	WriteByte(0x2002104B,0x90);
	WriteByte(0x2002104C,0x90);
	WriteByte(0x2002104D,0x90);
	orig_Con_Init = DetourCreate((void*)0x200208E0,(void*)&my_Con_Init,DETOUR_TYPE_JMP,9);
}

/*
	Called by ref_fixes.cpp on vid_loadrefresh
*/
void scaledFont_init(void) {
	void * glVertex2f = *(int*)0x300A4670;
	// PrintOut(PRINT_LOG,"Huh: %08X\n",glVertex2f);

	DetourRemove(&orig_glVertex2f);
	orig_glVertex2f = DetourCreate((void*)glVertex2f,(void*)&my_glVertex2f,DETOUR_TYPE_JMP,DETOUR_LEN_AUTO); //5

	// DetourRemove(&orig_Draw_String_Color);
	// orig_Draw_String_Color = DetourCreate((void*)0x30001A40,(void*)&my_Draw_String_Color,DETOUR_TYPE_JMP, 5);

	// DetourRemove(&orig_Draw_String);
	// orig_Draw_String = DetourCreate((void*)0x300019D0,(void*)&my_Draw_String,DETOUR_TYPE_JMP, 6);

	DetourRemove(&orig_Draw_Char);
	orig_Draw_Char = DetourCreate((void*)0x30001850 ,(void*)&my_Draw_Char,DETOUR_TYPE_JMP,7);

	DetourRemove(&orig_DrawStretchPic);
	orig_DrawStretchPic = DetourCreate((void*)0x30001D10 ,(void*)&my_DrawStretchPic, DETOUR_TYPE_JMP,5);
	
}

#endif