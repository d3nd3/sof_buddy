#include "feature_config.h"

#if FEATURE_SCALED_CON

#include "sof_compat.h"
#include "util.h"
#include "../../scaled_ui_base/shared.h"

extern int* cls_state;

extern void( * orig_SRC_AddDirtyPoint)(int x, int y);

extern int real_refdef_width;

void scaledCon_EarlyStartup(void)
{
    PrintOut(PRINT_LOG, "scaled_con: Early startup - applying memory patches\n");
    
    cls_state = (int*)rvaToAbsExe((void*)0x001C1F00);
    SOFBUDDY_ASSERT(cls_state != nullptr);

    orig_SRC_AddDirtyPoint = (void(*)(int,int))rvaToAbsExe((void*)0x000140B0);
    SOFBUDDY_ASSERT(orig_SRC_AddDirtyPoint != nullptr);

    //ensure the dirty point in con_drawnotify uses the actual width still
    writeIntegerAt(rvaToAbsExe((void*)0x00020F6F), (int)&real_refdef_width);
    
    WriteByte(rvaToAbsExe((void*)0x00021039), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002103A), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002103B), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002103C), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002103D), 0x90);

    WriteByte(rvaToAbsExe((void*)0x00021049), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002104A), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002104B), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002104C), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0002104D), 0x90);

    // CON_DrawConsole draws the bottom-right version string at
    // viddef_width - 44. Draw_String advances 8 px per glyph, so the six
    // characters of "v1.07f" are 48 px wide and clip even before scaling.
    // Use 56 (48 + 8 px margin). The value is in the virtual width that
    // hkCon_DrawConsole divides by the console font scale, so the margin
    // scales with the font instead of shrinking to zero.
    WriteByte(rvaToAbsExe((void*)0x00021074), 0xC8);

    PrintOut(PRINT_LOG, "scaled_con: Early startup complete\n");
}

#endif // FEATURE_SCALED_CON

