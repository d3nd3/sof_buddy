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
    

    orig_SRC_AddDirtyPoint = (void(*)(int,int))rvaToAbsExe((void*)0x000140B0);

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
    
    PrintOut(PRINT_LOG, "scaled_con: Early startup complete\n");
}

#endif // FEATURE_SCALED_CON

