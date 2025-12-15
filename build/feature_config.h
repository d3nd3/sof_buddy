#pragma once

/*
    Compile-time Feature Configuration
    
    Auto-generated from features/FEATURES.txt
    Enable/disable features by commenting/uncommenting lines in features/FEATURES.txt
    
    Format in features/FEATURES.txt:
    - feature_name     (enabled)
    - // feature_name  (disabled)
*/

#define FEATURE_MEDIA_TIMERS 1  // enabled
#define FEATURE_TEXTURE_MAPPING_MIN_MAG 1  // enabled
#define FEATURE_SCALED_CON 1  // enabled
#define FEATURE_SCALED_HUD 0  // disabled
#define FEATURE_SCALED_TEXT 0  // disabled
#define FEATURE_SCALED_MENU 0  // disabled
#define FEATURE_HD_TEXTURES 0  // disabled
#define FEATURE_VSYNC_TOGGLE 0  // disabled
#define FEATURE_LIGHTING_BLEND 0  // disabled
#define FEATURE_TEAMICONS_OFFSET 0  // disabled
#define FEATURE_NEW_SYSTEM_BUG 0  // disabled
#define FEATURE_CONSOLE_PROTECTION 0  // disabled
#define FEATURE_CL_MAXFPS_SINGLEPLAYER 0  // disabled
#define FEATURE_RAW_MOUSE 0  // disabled

/*
    Usage in feature files:
    
    #include "feature_config.h"
    
    #if FEATURE_MY_FEATURE
    REGISTER_HOOK(MyFunction, 0x12345678, void, __cdecl);
    // ... feature implementation
    #endif
*/
