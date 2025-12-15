#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "detours.h"
#include "../../scaled_ui_base/shared.h"

void scaledHud_RefDllLoaded(char const* name)
{
    PrintOut(PRINT_LOG, "scaled_hud: Installing ref.dll hooks\n");
    
    WriteE8Call(rvaToAbsRef((void*)0x0000239E), (void*)&my_glVertex2f_CroppedPic_1);
    WriteByte(rvaToAbsRef((void*)0x000023A3), 0x90);
    
    WriteE8Call(rvaToAbsRef((void*)0x000023CC), (void*)&my_glVertex2f_CroppedPic_2);
    WriteByte(rvaToAbsRef((void*)0x000023D1), 0x90);
    
    WriteE8Call(rvaToAbsRef((void*)0x000023F6), (void*)&my_glVertex2f_CroppedPic_3);
    WriteByte(rvaToAbsRef((void*)0x000023FB), 0x90);
    
    WriteE8Call(rvaToAbsRef((void*)0x0000240E), (void*)&my_glVertex2f_CroppedPic_4);
    WriteByte(rvaToAbsRef((void*)0x00002413), 0x90);
    
    PrintOut(PRINT_LOG, "scaled_hud: ref.dll hooks installed successfully\n");
}

#endif // FEATURE_SCALED_HUD

