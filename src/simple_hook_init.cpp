/*
 * Module-Specific Hook Initialization
 * =====================================
 * 
 * This file provides centralized initialization functions for applying hooks to specific
 * SoF modules (SoF.exe, ref.dll, game.dll, player.dll) and system DLLs (user32.dll, etc.).
 * 
 * PURPOSE:
 * --------
 * These functions wrap HookManager::ApplyXHooks() methods and are called at specific 
 * lifecycle phases to ensure hooks are applied in the correct order and at the right time.
 * 
 * COMPARISON WITH HookManager::ApplyAllHooks():
 * ---------------------------------------------
 * 
 * HookManager::ApplyAllHooks() is a LEGACY function that:
 * - Applies ALL registered hooks at once
 * - Doesn't distinguish between modules
 * - Used only for backward compatibility
 * - Less controlled timing and ordering
 * 
 * The module-specific initialization pattern (this file):
 * - Applies hooks for ONE specific module type per call
 * - Called at appropriate lifecycle phases
 * - Provides better separation and timing control
 * - Allows modules to be hooked/unhooked independently
 * - Enables per-module debugging and logging
 * 
 * USAGE PATTERN:
 * -------------
 * InitializeExeHooks()    - Called during EarlyStartup phase
 *                           (hooks SoF.exe functions at 0x200xxxxx)
 * 
 * InitializeSystemHooks() - Called during EarlyStartup phase
 *                           (hooks system DLLs like user32.dll, kernel32.dll, etc.)
 *                 
 * 
 * InitializeRefHooks()    - Called when ref.dll loads
 *                           (hooks ref.dll functions at 0x5xxxxxxx)
 * 
 * InitializeGameHooks()   - Called when game.dll loads
 *                           (hooks game.dll functions at 0x5xxxxxxx)
 * 
 * InitializePlayerHooks() - Called when player.dll loads
 *                           (hooks player.dll functions at 0x5xxxxxxx)
 * 
 * Each function:
 * 1. Logs the module being initialized
 * 2. Gets the hook count for that specific module type
 * 3. Applies only hooks registered for that module
 * 4. Provides module-specific success/failure logging
 * 
 * This provides granular control over when and how hooks are applied, compared to
 * the "apply everything at once" approach of HookManager::ApplyAllHooks().
 */

#include "hook_manager.h"
#include "util.h"

// Initialize hooks targeting SoF.exe (0x200xxxxx addresses)
// Called during EarlyStartup lifecycle phase
extern "C" void InitializeExeHooks() {
    PrintOut(PRINT_LOG, "=== Initializing SoF.exe hooks ===\n");
    
    HookManager& manager = HookManager::Instance();
    PrintOut(PRINT_LOG, "Found %zu SoF.exe hooks to apply\n", manager.GetHookCount(HookModule::SofExe));
    
    manager.ApplyExeHooks();
    
    PrintOut(PRINT_LOG, "=== SoF.exe hook initialization complete ===\n");
}

// Initialize hooks targeting ref.dll (0x5xxxxxxx addresses, ref module)
// Called when ref.dll is loaded
extern "C" void InitializeRefHooks() {
    PrintOut(PRINT_LOG, "=== Initializing ref.dll hooks ===\n");
    
    HookManager& manager = HookManager::Instance();
    PrintOut(PRINT_LOG, "Found %zu ref.dll hooks to apply\n", manager.GetHookCount(HookModule::RefDll));
    
    manager.ApplyRefHooks();
    
    PrintOut(PRINT_LOG, "=== ref.dll hook initialization complete ===\n");
}

// Initialize hooks targeting game.dll (0x5xxxxxxx addresses, game module)  
// Called when game.dll is loaded
extern "C" void InitializeGameHooks() {
    PrintOut(PRINT_LOG, "=== Initializing game.dll hooks ===\n");
    
    HookManager& manager = HookManager::Instance();
    PrintOut(PRINT_LOG, "Found %zu game.dll hooks to apply\n", manager.GetHookCount(HookModule::GameDll));
    
    manager.ApplyGameHooks();
    
    PrintOut(PRINT_LOG, "=== game.dll hook initialization complete ===\n");
}

// Initialize hooks targeting player.dll (0x5xxxxxxx addresses, player module)
// Called when player.dll is loaded  
extern "C" void InitializePlayerHooks() {
    PrintOut(PRINT_LOG, "=== Initializing player.dll hooks ===\n");
    
    HookManager& manager = HookManager::Instance();
    PrintOut(PRINT_LOG, "Found %zu player.dll hooks to apply\n", manager.GetHookCount(HookModule::PlayerDll));
    
    manager.ApplyPlayerHooks();
    
    PrintOut(PRINT_LOG, "=== player.dll hook initialization complete ===\n");
}

// Initialize hooks targeting system DLLs (user32.dll, kernel32.dll, etc.)
// Called during PostCvarInit lifecycle phase (after CVars are initialized)
// Must NOT be called during EarlyStartup as it causes crashes during DLL init
extern "C" void InitializeSystemHooks() {
    PrintOut(PRINT_LOG, "=== Initializing system DLL hooks ===\n");
    
    HookManager& manager = HookManager::Instance();
    PrintOut(PRINT_LOG, "Found %zu system DLL hooks to apply\n", manager.GetHookCount(HookModule::Unknown));
    
    manager.ApplySystemHooks();
    
    PrintOut(PRINT_LOG, "=== system DLL hook initialization complete ===\n");
}

// This function should be called during shutdown to clean up all hooks
extern "C" void ShutdownAllHooks() {
    PrintOut(PRINT_LOG, "=== Shutting down all hooks ===\n");
    
    HookManager& manager = HookManager::Instance();
    manager.RemoveAllHooks();
    
    PrintOut(PRINT_LOG, "=== Hook shutdown complete ===\n");
}
