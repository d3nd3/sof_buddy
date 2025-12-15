#include "feature_config.h"

#if FEATURE_SCALED_CON

#include "sof_compat.h"
#include "util.h"
#include "../../scaled_ui_base/shared.h"

extern int* cls_state;
extern void( * orig_Con_Initialize)(void);
extern void( * orig_SRC_AddDirtyPoint)(int x, int y);
extern void( * orig_SCR_DirtyRect)(int x1, int y1, int x2, int y2);
extern int real_refdef_width;

void scaledCon_EarlyStartup(void)
{
    PrintOut(PRINT_LOG, "scaled_con: Early startup - applying memory patches\n");
    
    cls_state = (int*)rvaToAbsExe((void*)0x001C1F00);
    
    orig_Con_Initialize = (void(*)())rvaToAbsExe((void*)0x00020720);
    orig_SRC_AddDirtyPoint = (void(*)(int,int))rvaToAbsExe((void*)0x000140B0);
    orig_SCR_DirtyRect = (void(*)(int,int,int,int))rvaToAbsExe((void*)0x00014190);
    
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

