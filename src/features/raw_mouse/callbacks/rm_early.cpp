#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "detours.h"
#include "util.h"
#include "../shared.h"

void raw_mouse_EarlyStartup()
{
    //This one is setting center
    WriteE8Call(rvaToAbsExe((void*)0x0004A0B2), (void*)&hkSetCursorPos);
    WriteByte((unsigned char*)rvaToAbsExe((void*)0x0004A0B2) + 5, 0x90);
    //This one is setting center
    WriteE8Call(rvaToAbsExe((void*)0x0004A410), (void*)&hkSetCursorPos);
    WriteByte((unsigned char*)rvaToAbsExe((void*)0x0004A410) + 5, 0x90);
    //This one is setting center
    WriteE8Call(rvaToAbsExe((void*)0x0004A579), (void*)&hkSetCursorPos);
    WriteByte((unsigned char*)rvaToAbsExe((void*)0x0004A579) + 5, 0x90);
    raw_mouse_install_winmain_peek_detour();
    if (!oSetCursorPos) {
        HMODULE hUser32 = GetModuleHandleA("user32.dll");
        if (hUser32)
            oSetCursorPos = (tSetCursorPos)GetProcAddress(hUser32, "SetCursorPos");
    }
}

#endif // FEATURE_RAW_MOUSE

