#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_TEXT || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "detours.h"
#include "../../scaled_ui_base/shared.h"

extern void(__stdcall * orig_glVertex2f)(float one, float two);
extern void(__stdcall * orig_glVertex2i)(int x, int y);
extern void(__stdcall * orig_glDisable)(int cap);
extern void __stdcall hkglVertex2f(float x, float y);

void scaledUIBase_RefDllLoaded(char const* name)
{
    PrintOut(PRINT_LOG, "scaled_ui_base: Installing ref.dll hooks\n");
    
    void* glVertex2f = (void*)*(int*)rvaToAbsRef((void*)0x000A4670);
    orig_glVertex2i = (void(__stdcall*)(int,int))*(int*)rvaToAbsRef((void*)0x000A46D0);
    orig_glDisable = (void(__stdcall*)(int))*(int*)rvaToAbsRef((void*)0x000A44EC);
    
    if (orig_glVertex2f) {
        DetourSystem::Instance().RemoveDetourAtAddress((void*)glVertex2f);
        orig_glVertex2f = nullptr;
    }
    void* trampoline = nullptr;
    if (DetourSystem::Instance().ApplyDetourAtAddress((void*)glVertex2f, (void*)&hkglVertex2f, &trampoline, "glVertex2f", 0)) {
        orig_glVertex2f = (void(__stdcall*)(float,float))trampoline;
    }
    
    PrintOut(PRINT_LOG, "scaled_ui_base: ref.dll hooks installed successfully\n");
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_TEXT || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

