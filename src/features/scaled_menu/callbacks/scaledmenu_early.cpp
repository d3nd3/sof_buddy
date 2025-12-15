#include "feature_config.h"

#if FEATURE_SCALED_MENU

#ifdef UI_MENU

#include "sof_compat.h"
#include "util.h"
#include "../../scaled_ui_base/shared.h"
#include "../shared.h"

void scaledMenu_EarlyStartup(void)
{
    PrintOut(PRINT_LOG, "scaled_menu: Early startup - applying memory patches\n");
    
    WriteE8Call(rvaToAbsExe((void*)0x000CF8BA), (void*)&my_rect_c_Parse);
    
    WriteE8Call(rvaToAbsExe((void*)0x000C8856), (void*)&my_Draw_GetPicSize);
    WriteByte(rvaToAbsExe((void*)0x000C885B), 0x90);
    
    PrintOut(PRINT_LOG, "scaled_menu: Early startup complete\n");
}

#endif // UI_MENU

#endif // FEATURE_SCALED_MENU

