// Module-specific hook initialization for the new macro-based system
#include "../hdr/hook_manager.h"
#include "../hdr/util.h"

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

// Legacy function - initializes ALL hooks (use module-specific versions instead)
extern "C" void InitializeAllHooks() {
    PrintOut(PRINT_LOG, "=== Initializing ALL hooks (legacy mode) ===\n");
    
    HookManager& manager = HookManager::Instance();
    PrintOut(PRINT_LOG, "Found %zu total registered hooks\n", manager.GetHookCount());
    
    manager.ApplyAllHooks();
    
    PrintOut(PRINT_LOG, "=== All hooks initialization complete ===\n");
}

// This function should be called during shutdown to clean up all hooks
extern "C" void ShutdownAllHooks() {
    PrintOut(PRINT_LOG, "=== Shutting down all hooks ===\n");
    
    HookManager& manager = HookManager::Instance();
    manager.RemoveAllHooks();
    
    PrintOut(PRINT_LOG, "=== Hook shutdown complete ===\n");
}
