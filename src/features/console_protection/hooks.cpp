/*
	Console security fixes/crashes
	
	This feature provides two main security fixes:
	1. Buffer overflow fix for console drawing on resolutions > 2048px wide
	2. Clipboard paste fix to prevent crashes/overflows when pasting into console
*/

#include "feature_config.h"

#if FEATURE_CONSOLE_PROTECTION

// #include "../../../hdr/feature_macro.h"
#include "shared_hook_manager.h"
#include "util.h"
#include "sof_compat.h"

#include <windows.h>
#include <string.h>

// Console drawing fix defines
#define MAXCMDLINE 256

// Forward declarations
static void my_Con_Draw_Console(void);
static char* hkSys_GetClipboardData(void);
static void console_protection_EarlyStartup(void);

// Original function pointer
char *(*oSys_GetClipboardData)(void) = (char*(*)(void))rvaToAbsExe((void*)0x00065E60);

// Using paletteRGBA_s from sof_compat.h

// Register for EarlyStartup to apply memory patches
REGISTER_SHARED_HOOK_CALLBACK(EarlyStartup, console_protection, console_protection_EarlyStartup, 80, Post);

/*
	This fixes the buffer overflow crash caused by > 2048px wide resolutions whilst entering lines into console.
	This is Con_DrawInput()
*/
static void my_Con_Draw_Console(void)
{
	//jmp to here.

	int		i;
	char	*text;

	static int* edit_line = (int*)rvaToAbsExe((void*)0x00367EA4);
	static char* key_lines = (char*)rvaToAbsExe((void*)0x00365EA0);

	static int* key_linepos = (int*)rvaToAbsExe((void*)0x00365E9C);
	static int* cls_realtime = (int*)rvaToAbsExe((void*)0x001C1F0C);
	static int* con_linewidth = (int*)rvaToAbsExe((void*)0x0024AF98);
	static int* con_vislines = (int*)rvaToAbsExe((void*)0x0024AFA0);

	static int* cls_key_dest = (int*)rvaToAbsExe((void*)0x001C1F04);
	static int* cls_state = (int*)rvaToAbsExe((void*)0x001C1F00);

	static void (*ref_draw_char) (int, int, int, void*) = (void(*)(int,int,int,void*))*(int*)rvaToAbsExe((void*)0x004035C8);
	
	//key_menu
	if (*cls_key_dest == 3)
		return;
	//key_console, ca_active
	if (*cls_key_dest != 1 && *cls_state == 8)
		return;		// don't draw anything (always draw if not active)

	
	text = key_lines + MAXCMDLINE * *edit_line;

	//Blinking underscore animation
	text[*key_linepos] = 0x20;
	if ( (*cls_realtime>>8) & 1 ) {
		text[*key_linepos] = 0x5F;	
	}
	
	int maxchars = *con_linewidth > MAXCMDLINE ? MAXCMDLINE : *con_linewidth;
	
	//Fill spaces
	for (i=*key_linepos+1 ; i < maxchars; i++)
		text[i] = ' ';

	//con_linewidth can be larger than MAXCMDLINE.
	//key_linepos cannot be larger than 255

	//number of characters on line already larger or equal to that than can fit on screen
	//then we show max number of characters.
	//but can we access till the end of buffer always?

	//prestep horizontally.
	if (*key_linepos >= *con_linewidth) {
		text += 1 + *key_linepos - *con_linewidth;
	}
	
	//Refer to image in google notes - text=start_pos_show_left
	int space = &text[MAXCMDLINE-1] - text;
	
	if ( space >= 0 ) {
		maxchars = space < *con_linewidth ? space : *con_linewidth;

		paletteRGBA_t palette;
		palette.c = 0xFFFFFFFF;

		//Draw_Char(int, int, int, paletteRGBA_c &)
		for (i=0 ; i<maxchars; i++) {
			ref_draw_char((i+1)<<3, *con_vislines - 16, text[i], &palette);
		}

		// remove previous cursor
		text = key_lines + MAXCMDLINE * *edit_line;
		text[*key_linepos] = 0;
	}
}

/*
	Clipboard paste fix to prevent buffer overflow when pasting large text into console
*/
char* hkSys_GetClipboardData(void)
{
	char *cbd;
	static void (*Z_Free)(void *pvAddress) = (void(*)(void*))rvaToAbsExe((void*)0x000F9D32);
	static int* key_linepos = (int*)rvaToAbsExe((void*)0x365E9C);
	static char* key_lines = (char*)rvaToAbsExe((void*)0x365EA0);
	static int* edit_line = (int*)rvaToAbsExe((void*)0x00367EA4);

	if ( ( cbd = oSys_GetClipboardData() ) != 0 )
	{
		int allow_to_add_len;
		strtok( cbd, "\n\r\b" );

		//excludes null char
		allow_to_add_len = strlen( cbd );

		//key_linepos points to the null character
		//but its length corresponds to the non-null characters.

		//excludes null char. (MAXCMDLINE-1) non-null characters allowed.
		if ( allow_to_add_len + *key_linepos >= MAXCMDLINE ) {
			//excluding null char.
			allow_to_add_len= (MAXCMDLINE-1) - *key_linepos;
		}
		//allow_to_add_len fits already.

		//key_linepos starts at 1 because of ] or > symbol.
		if ( allow_to_add_len > 0 )
		{
			//ensure clipboard is null character ended?
			cbd[allow_to_add_len]=0; //this clamps cbd to size i+1 , i if exclude null.
			//append cbd to key_lines

			strcat( key_lines + *edit_line * MAXCMDLINE, cbd );

			//key_linepos should only be allowed to be set to 255 max.
			*key_linepos += allow_to_add_len;
		}
		Z_Free( cbd );
	}

	return cbd;
}

/*
	PostCvarInit lifecycle callback - Apply memory patches for console fixes
*/
static void console_protection_EarlyStartup(void)
{
	PrintOut(PRINT_LOG, "Console Protection: Applying security patches...\n");

	//=================== CONSOLE BUG FIXING =========================

	//PASTING INTO CONSOLE CRASH/OVERFLOW
	WriteE8Call(rvaToAbsExe((void*)0x0004BB63), (void*)&hkSys_GetClipboardData);
	WriteE9Jmp(rvaToAbsExe((void*)0x0004BB6C), rvaToAbsExe((void*)0x0004BBFE));

	//Overflow on larger resolutions
	WriteE9Jmp(rvaToAbsExe((void*)0x0002111D), (void*)&my_Con_Draw_Console);
	WriteE9Jmp(rvaToAbsExe((void*)0x00020C90), rvaToAbsExe((void*)0x00020D6C));

	PrintOut(PRINT_LOG, "Console Protection: Security patches applied successfully.\n");
}

#endif // FEATURE_CONSOLE_PROTECTION