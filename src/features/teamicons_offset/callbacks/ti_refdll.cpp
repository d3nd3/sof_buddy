#include "feature_config.h"

#if FEATURE_TEAMICONS_OFFSET

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

void teamicons_offset_RefDllLoaded(char const* name) {
    PrintOut(PRINT_LOG, "TeamIcons Offset: Initializing widescreen fixes...\n");

    engine_fovY = (float*)rvaToAbsRef((void*)0x0008FD00);
    engine_fovX = (float*)rvaToAbsRef((void*)0x0008FCFC);
    realHeight = (unsigned int*)rvaToAbsRef((void*)0x0008FFC8);
    virtualHeight = (unsigned int*)rvaToAbsRef((void*)0x0008FCF8);
    icon_min_y = (int*)rvaToAbsExe((void*)0x00225ED4);
    icon_height = (int*)rvaToAbsExe((void*)0x00225EDC);

    writeUnsignedIntegerAt(rvaToAbsRef((void*)0x0000313F), (unsigned int)&fovfix_x);
    writeUnsignedIntegerAt(rvaToAbsRef((void*)0x00003157), (unsigned int)&fovfix_y);

    writeIntegerAt(rvaToAbsRef((void*)0x00003187), (int)&TeamIconInterceptFix - (int)rvaToAbsRef((void*)0x00003187) - 4);

    writeUnsignedIntegerAt(rvaToAbsExe((void*)0x00157A8), (unsigned int)&teamviewFovAngle);

    PrintOut(PRINT_LOG, "TeamIcons Offset: Widescreen fixes applied\n");
}

#endif // FEATURE_TEAMICONS_OFFSET

