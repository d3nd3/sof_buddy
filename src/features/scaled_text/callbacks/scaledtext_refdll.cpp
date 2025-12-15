#include "feature_config.h"

#if FEATURE_SCALED_TEXT

#include "sof_compat.h"
#include "util.h"
#include "detours.h"
#include "../../scaled_ui_base/shared.h"

void scaledText_RefDllLoaded(char const* name)
{
    PrintOut(PRINT_LOG, "scaled_text: Installing ref.dll hooks\n");
    
    WriteE8Call(rvaToAbsRef((void*)0x00004860), (void*)&my_glVertex2f_DrawFont_1);
    WriteByte(rvaToAbsRef((void*)0x00004865), 0x90);

    WriteE8Call(rvaToAbsRef((void*)0x00004892), (void*)&my_glVertex2f_DrawFont_2);
    WriteByte(rvaToAbsRef((void*)0x00004897), 0x90);

    WriteE8Call(rvaToAbsRef((void*)0x000048D2), (void*)&my_glVertex2f_DrawFont_3);
    WriteByte(rvaToAbsRef((void*)0x000048D7), 0x90);

    WriteE8Call(rvaToAbsRef((void*)0x00004903), (void*)&my_glVertex2f_DrawFont_4);
    WriteByte(rvaToAbsRef((void*)0x00004908), 0x90);
    
    PrintOut(PRINT_LOG, "scaled_text: ref.dll hooks installed successfully\n");
}

#endif // FEATURE_SCALED_TEXT

