#include <cstring>
#include "util.h"
#include "sof_compat.h"

// R_Register (ref_gl.dll): Cvar_Get("gl_mode", "3", ...) — use default "0" instead.
// Avoids the ugly community-edition refresh glitch on a fresh install (or whenever
// config.cfg is missing/deleted and gl_mode falls back to the stock default).
static void ref_gl_patch_gl_mode_default(void)
{
    constexpr uintptr_t kGlModeDefaultPushRva = 0x0000FF8D;
    constexpr uintptr_t kGlZeroDefaultPushRva = 0x0000FFA1; // push "0" for gl_lightmap
    unsigned char* modeInsn = (unsigned char*)rvaToAbsRef((void*)kGlModeDefaultPushRva);
    unsigned char* zeroInsn = (unsigned char*)rvaToAbsRef((void*)kGlZeroDefaultPushRva);
    if (!modeInsn || !zeroInsn || modeInsn[0] != 0x68 || zeroInsn[0] != 0x68)
        return;
    uint32_t zeroStr = 0;
    memcpy(&zeroStr, zeroInsn + 1, sizeof(zeroStr));
    memcpy(modeInsn + 1, &zeroStr, sizeof(zeroStr));
}

void ref_gl_r_init_pre(void*& hinstance, void*& hWnd, void*& unknown)
{
    (void)hinstance;
    (void)hWnd;
    (void)unknown;
    ref_gl_patch_gl_mode_default();
}
