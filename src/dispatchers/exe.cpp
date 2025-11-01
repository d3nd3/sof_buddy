// SoF.exe Hook Dispatchers
// Memory range: 0x200xxxxx
// Main executable functions and core game engine

#include "feature_macro.h"
#include "feature_config.h"
#include "shared_hook_manager.h"
#include "util.h"

// Example: Main game loop (placeholder address)
// REGISTER_HOOK_VOID(GameMainLoop, 0x20066412, void, __cdecl) {
//     PrintOut(PRINT_LOG, "=== Game Main Loop Dispatch ===\n");
//     
//     // Pre-frame callbacks
//     DISPATCH_SHARED_HOOK(PreFrameUpdate);
//     
//     // Call original game loop
//     oGameMainLoop();
//     
//     // Post-frame callbacks  
//     DISPATCH_SHARED_HOOK(PostFrameUpdate);
// }

// =============================================================================
// PLAYER SYSTEMS  
// =============================================================================

// Example: Player update (placeholder address)
// REGISTER_HOOK_VOID(PlayerUpdate, 0x20123456, void, __cdecl, float deltaTime) {
//     PrintOut(PRINT_LOG, "=== Player Update Dispatch ===\n");
//     
//     // Pre-update callbacks
//     DISPATCH_SHARED_HOOK(PrePlayerUpdate);
//     
//     // Call original player update
//     oPlayerUpdate(deltaTime);
//     
//     // Post-update callbacks
//     DISPATCH_SHARED_HOOK(PostPlayerUpdate);
// }

// =============================================================================
// CONSOLE SYSTEM
// =============================================================================

// Example: Console command processing (placeholder address)
// REGISTER_HOOK_VOID(ConsoleCommand, 0x20234567, void, __cdecl, const char* cmd) {
//     PrintOut(PRINT_LOG, "=== Console Command Dispatch: %s ===\n", cmd);
//     
//     // Pre-command callbacks (for command filtering/logging)
//     DISPATCH_SHARED_HOOK(PreConsoleCommand);
//     
//     // Call original command handler
//     oConsoleCommand(cmd);
//     
//     // Post-command callbacks
//     DISPATCH_SHARED_HOOK(PostConsoleCommand);
// }

int current_vid_w = 0;
int current_vid_h = 0;
int* viddef_width;
int* viddef_height;

// External reference to screen_y_scale from scaled_ui feature
#if FEATURE_UI_SCALING
extern float screen_y_scale;
#endif
// Dispatcher for VID_CheckChanges (SoF.exe)
// Allows multiple features to react before/after video/renderer changes
REGISTER_HOOK_VOID(VID_CheckChanges, (void*)0x00670C0, SofExe, void, __cdecl) {
    if (!viddef_width) viddef_width = (int*)rvaToAbsExe((void*)0x0040365C);
    if (!viddef_height) viddef_height = (int*)rvaToAbsExe((void*)0x00403660);

    DISPATCH_SHARED_HOOK(VID_CheckChanges, Pre);
    
    oVID_CheckChanges();
    current_vid_w = *viddef_width;
	current_vid_h = *viddef_height;
	
	// Update screen_y_scale for scaled_ui feature
#if FEATURE_UI_SCALING
	screen_y_scale = (float)current_vid_h / 480.0f;
#endif

    // Post-change callbacks (e.g., cache new resolution, update overlays)
    DISPATCH_SHARED_HOOK(VID_CheckChanges, Post);

    
}

// =============================================================================
// PLACEHOLDER NOTE
// =============================================================================

/*
    This file demonstrates the module-based dispatcher concept.
    
    Real implementations will:
    1. Replace placeholder addresses with actual reverse-engineered addresses
    2. Implement actual hook functions (uncomment the examples above)
    3. Add proper function signatures from reverse engineering
    4. Include all relevant SoF.exe functions that need shared hooking
    
    Current Status: PLACEHOLDER - demonstrates architecture concept
    Migration: Features can register callbacks for these hooks once implemented
*/
