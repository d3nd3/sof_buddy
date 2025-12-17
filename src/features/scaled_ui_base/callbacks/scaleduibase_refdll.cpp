#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "detours.h"
#include "../../scaled_ui_base/shared.h"

extern void(__stdcall * orig_glVertex2f)(float one, float two);
extern void(__stdcall * orig_glVertex2i)(int x, int y);
extern void(__stdcall * orig_glDisable)(int cap);
extern void __stdcall hkglVertex2f(float x, float y);

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
extern void __stdcall my_glVertex2f_DrawFont_1(float x, float y);
extern void __stdcall my_glVertex2f_DrawFont_2(float x, float y);
extern void __stdcall my_glVertex2f_DrawFont_3(float x, float y);
extern void __stdcall my_glVertex2f_DrawFont_4(float x, float y);
#endif

void scaledUIBase_RefDllLoaded(char const* name)
{
    SOFBUDDY_ASSERT(name != nullptr);
    
    PrintOut(PRINT_LOG, "scaled_ui_base: Installing ref.dll hooks\n");
    
    void* glVertex2f = (void*)*(int*)rvaToAbsRef((void*)0x000A4670);
    SOFBUDDY_ASSERT(glVertex2f != nullptr);
    
    orig_glVertex2i = (void(__stdcall*)(int,int))*(int*)rvaToAbsRef((void*)0x000A46D0);
    orig_glDisable = (void(__stdcall*)(int))*(int*)rvaToAbsRef((void*)0x000A44EC);
    SOFBUDDY_ASSERT(orig_glVertex2i != nullptr);
    SOFBUDDY_ASSERT(orig_glDisable != nullptr);
    
    if (orig_glVertex2f) {
        DetourSystem::Instance().RemoveDetourAtAddress((void*)glVertex2f);
        orig_glVertex2f = nullptr;
    }
    void* trampoline = nullptr;
    if (DetourSystem::Instance().ApplyDetourAtAddress((void*)glVertex2f, (void*)&hkglVertex2f, &trampoline, "glVertex2f", 0)) {
        orig_glVertex2f = (void(__stdcall*)(float,float))trampoline;
        SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
    }
    
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

