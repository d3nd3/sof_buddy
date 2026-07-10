#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "../../scaled_ui_base/shared.h"
#include "../../scaled_ui_base/ref_gl_state.h"

extern void(__stdcall * orig_glVertex2f)(float one, float two);
extern void(__stdcall * orig_glVertex2i)(int x, int y);
extern void(__stdcall * orig_glDisable)(int cap);

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
extern void __stdcall my_glVertex2f_DrawFont_1(float x, float y);
extern void __stdcall my_glVertex2f_DrawFont_2(float x, float y);
extern void __stdcall my_glVertex2f_DrawFont_3(float x, float y);
extern void __stdcall my_glVertex2f_DrawFont_4(float x, float y);
#endif

// RefDllLoaded priority must stay above scaled_hud (60): HUD uses orig_glVertex2f from this callback.
void scaledUIBase_RefDllLoaded(char const* name)
{
    SOFBUDDY_ASSERT(name != nullptr);

    PrintOut(PRINT_LOG, "scaled_ui_base: Installing ref.dll hooks\n");

    orig_glVertex2f = (void(__stdcall*)(float,float))*(int*)rvaToAbsRef((void*)0x000A4670);
    SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);

    orig_glVertex2i = (void(__stdcall*)(int,int))*(int*)rvaToAbsRef((void*)0x000A46D0);
    orig_glDisable = (void(__stdcall*)(int))*(int*)rvaToAbsRef((void*)0x000A44EC);
    SOFBUDDY_ASSERT(orig_glVertex2i != nullptr);
    SOFBUDDY_ASSERT(orig_glDisable != nullptr);

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD
    ref_gl_state_init();
#endif

#if FEATURE_SCALED_CON
    PrintOut(PRINT_LOG, "scaled_ui_base: Installing Draw_Char vertex hooks\n");
    static const uintptr_t drawCharSites[] = {
        0x0000192B, 0x00001964, 0x0000199D, 0x000019B1,
        0x00001C07, 0x00001C3B, 0x00001C6F, 0x00001C83,
    };
    for (uintptr_t rva : drawCharSites) {
        void* at = rvaToAbsRef((void*)rva);
        WriteE8Call(at, (void*)&my_glVertex2f_DrawChar_1);
        WriteByte((char*)at + 5, 0x90);
    }
#endif

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
    PrintOut(PRINT_LOG, "scaled_ui_base: Installing Draw_StretchPic vertex hooks\n");
    WriteE8Call(rvaToAbsRef((void*)0x00001E1F), (void*)&my_glVertex2f_StretchPic_1);
    WriteByte(rvaToAbsRef((void*)0x00001E24), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x00001E4E), (void*)&my_glVertex2f_StretchPic_2);
    WriteByte(rvaToAbsRef((void*)0x00001E53), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x00001E7D), (void*)&my_glVertex2f_StretchPic_3);
    WriteByte(rvaToAbsRef((void*)0x00001E82), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x00001E92), (void*)&my_glVertex2f_StretchPic_4);
    WriteByte(rvaToAbsRef((void*)0x00001E97), 0x90);

    PrintOut(PRINT_LOG, "scaled_ui_base: Installing Draw_Pic vertex hooks\n");
    WriteE8Call(rvaToAbsRef((void*)0x00001FDD), (void*)&my_glVertex2f_DrawPic_1);
    WriteByte(rvaToAbsRef((void*)0x00001FE2), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x00002007), (void*)&my_glVertex2f_DrawPic_2);
    WriteByte(rvaToAbsRef((void*)0x0000200C), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x00002047), (void*)&my_glVertex2f_DrawPic_3);
    WriteByte(rvaToAbsRef((void*)0x0000204C), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x0000206D), (void*)&my_glVertex2f_DrawPic_4);
    WriteByte(rvaToAbsRef((void*)0x00002072), 0x90);
#endif

#if FEATURE_SCALED_HUD
    PrintOut(PRINT_LOG, "scaled_ui_base: Installing Draw_PicOptions vertex hooks\n");
    WriteE8Call(rvaToAbsRef((void*)0x00002186), (void*)&my_glVertex2f_PicOptions_1);
    WriteByte(rvaToAbsRef((void*)0x0000218B), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x000021B1), (void*)&my_glVertex2f_PicOptions_2);
    WriteByte(rvaToAbsRef((void*)0x000021B6), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x000021DF), (void*)&my_glVertex2f_PicOptions_3);
    WriteByte(rvaToAbsRef((void*)0x000021E4), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x000021F4), (void*)&my_glVertex2f_PicOptions_4);
    WriteByte(rvaToAbsRef((void*)0x000021F9), 0x90);
#endif

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
    PrintOut(PRINT_LOG, "scaled_ui_base: Installing DrawFont vertex hooks\n");
    WriteE8Call(rvaToAbsRef((void*)0x00004860), (void*)&my_glVertex2f_DrawFont_1);
    WriteByte(rvaToAbsRef((void*)0x00004865), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x00004892), (void*)&my_glVertex2f_DrawFont_2);
    WriteByte(rvaToAbsRef((void*)0x00004897), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x000048D2), (void*)&my_glVertex2f_DrawFont_3);
    WriteByte(rvaToAbsRef((void*)0x000048D7), 0x90);
    WriteE8Call(rvaToAbsRef((void*)0x00004903), (void*)&my_glVertex2f_DrawFont_4);
    WriteByte(rvaToAbsRef((void*)0x00004908), 0x90);
#endif

    PrintOut(PRINT_LOG, "scaled_ui_base: ref.dll hooks installed successfully\n");
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
