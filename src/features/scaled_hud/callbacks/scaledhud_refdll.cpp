#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "detours.h"
#include "../../scaled_ui_base/shared.h"

static void PatchGlVertexE8(void* rva, void* handler) {
    void* at = rvaToAbsRef(rva);
    WriteE8Call(at, handler);
    WriteByte((char*)at + 5, 0x90);
}

void scaledHud_RefDllLoaded(char const* name)
{
    PrintOut(PRINT_LOG, "scaled_hud: Installing ref.dll hooks\n");

    static const uintptr_t croppedPicSites[] = {
        0x0000239E, 0x000023CC, 0x000023F6, 0x0000240E,
        0x0000250E, 0x0000254A, 0x00002586, 0x0000259A,
        0x0000268F, 0x000026B1, 0x000026D3, 0x000026DB,
        0x00002813, 0x00002835, 0x00002857, 0x00002863,
    };
    for (uintptr_t rva : croppedPicSites)
        PatchGlVertexE8((void*)rva, (void*)&my_glVertex2f_CroppedPic_1);

    PrintOut(PRINT_LOG, "scaled_hud: ref.dll hooks installed successfully\n");
}

#endif // FEATURE_SCALED_HUD
