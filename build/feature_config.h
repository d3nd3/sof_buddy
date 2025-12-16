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
#define FEATURE_SCALED_HUD 1  // enabled
#define FEATURE_SCALED_TEXT 1  // enabled
#define FEATURE_SCALED_MENU 0  // disabled
#define FEATURE_HD_TEXTURES 1  // enabled
#define FEATURE_VSYNC_TOGGLE 1  // enabled
#define FEATURE_LIGHTING_BLEND 1  // enabled
#define FEATURE_TEAMICONS_OFFSET 1  // enabled
#define FEATURE_NEW_SYSTEM_BUG 1  // enabled
#define FEATURE_CONSOLE_PROTECTION 1  // enabled
#define FEATURE_CL_MAXFPS_SINGLEPLAYER 1  // enabled
#define FEATURE_RAW_MOUSE 1  // enabled

/*
    Usage in feature files:
    
    #include "feature_config.h"
    
    #if FEATURE_MY_FEATURE
    REGISTER_HOOK(MyFunction, 0x12345678, void, __cdecl);
    // ... feature implementation
    #endif
*/
