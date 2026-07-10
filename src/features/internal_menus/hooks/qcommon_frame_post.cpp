#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

void internal_menus_qcommon_frame_post(int msec) {
    (void)msec;
    static int* s_cls_state = (int*)rvaToAbsExe((void*)0x001C1F00);
    if (s_cls_state) internal_menus_update_connect_flow(*s_cls_state);
}

#endif
