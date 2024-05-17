#include <windows.h>
#include <math.h>

#include "../DetourXS/detourxs.h"
#include "sof_compat.h"
#include "util.h"


cvar_t * _sofbuddy_font_scale = NULL;
cvar_t * _sofbuddy_console_size = NULL;

void (*orig_Draw_Char) (int x, int y, int num) = NULL;
void (*orig_Con_DrawConsole) (float frac) = NULL;
void (*orig_Con_CheckResize)(void) = NULL;

void (__stdcall *orig_glVertex2f)(float one, float two) = NULL;

float fontScale = 1;
float consoleSize = 0.5;
bool isFont = false;
/*
	It seems easier than imagined, because SoF calls Draw_String, instead of Draw_Char.
	Draw_String calls Draw_Char.
	If we hook Con_DrawConsole(), set a boolean,

	Hook glVertex2f()
	check the boolean:
	scale it.
*/


void my_Draw_Char(int x, int y, int num)  {
	//orig_Draw_Char(x,y,num);
}

int *cls_state = 0x201C1F00; 
void my_Con_DrawConsole (float frac) {

	isFont = true;
	if ( frac == 0.5 && !( *cls_state != 1 && *cls_state != 8 ) ) {
		//When loading screen console, half screen is blanked out.
		//Trying to target when in-game console _only_
		frac = consoleSize;
	}
	else frac = frac/fontScale;
	orig_Con_DrawConsole(frac);
	isFont = false;
}
int * viddef_width = 0x2040365C;
void my_Con_CheckResize(void) {
	int viddef_before = *viddef_width;
	*viddef_width = 1/fontScale * *viddef_width; 
	orig_Con_CheckResize();
	*viddef_width = viddef_before;
}

void __stdcall my_glVertex2f(float one, float two) {
	if (isFont) {
		one = one * fontScale;
		two = two * fontScale;
	} 
	orig_glVertex2f(one,two);
}

void consolesize_change(cvar_t * cvar) {
	consoleSize = cvar->value/fontScale;
}
void fontscale_change(cvar_t * cvar) {
	//round to nearest quarter
	fontScale = roundf(cvar->value * 4.0f) / 4.0f;
	consolesize_change(_sofbuddy_console_size);
}


void scaledFont_apply(void) {

	orig_Con_DrawConsole = DetourCreate((void*)0x20020F90,(void*)&my_Con_DrawConsole,DETOUR_TYPE_JMP,5);
	//Adjusts con.linewidth because less characters fit on line for font-scaling.
	orig_Con_CheckResize = DetourCreate((void*)0x20020880,(void*)&my_Con_CheckResize,DETOUR_TYPE_JMP,5);

	_sofbuddy_console_size = orig_Cvar_Get("_sofbuddy_console_size","0.35",CVAR_ARCHIVE,&consolesize_change);
	_sofbuddy_font_scale = orig_Cvar_Get("_sofbuddy_font_scale","1",CVAR_ARCHIVE,&fontscale_change);
	
}

/*
	Called by ref_fixes.cpp on vid_loadrefresh
*/
void scaledFont_init(void) {
	void * glVertex2f = *(int*)0x300A4670;

/*
	if ( orig_Draw_Char != NULL ) {
		DetourRemove(orig_Draw_Char);
		orig_Draw_Char = NULL;
	}
	orig_Draw_Char = DetourCreate((void*)0x30002240,(void*)&my_Draw_Char,DETOUR_TYPE_JMP,5);
*/
	if ( orig_glVertex2f != NULL ) {
		DetourRemove(orig_glVertex2f);
		orig_glVertex2f = NULL;
	}
	orig_glVertex2f = DetourCreate((void*)glVertex2f,(void*)&my_glVertex2f,DETOUR_TYPE_JMP,5);

}