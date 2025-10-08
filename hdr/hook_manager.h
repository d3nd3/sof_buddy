#pragma once

#include <vector>
#include <unordered_map>

enum class HookModule {
    SofExe,     // 0x200xxxxx - Main executable
    RefDll,     // 0x5xxxxxxx - ref.dll (ref_gl.dll, ref_soft.dll)
    GameDll,    // 0x5xxxxxxx - game.dll (gamex86.dll)
    PlayerDll,  // 0x5xxxxxxx - player.dll
    Unknown     // Other/unidentified addresses
};

struct HookEntry {
    void* address;
    void* detour_func;
    void** original_storage;
    const char* name;
    HookModule module;
    size_t detour_len; // 0 means auto-length (DETOUR_LEN_AUTO)
};

class HookManager {
public:
    static HookManager& Instance();
    
    // Add a hook to be applied later
    // detour_len: 0 means auto-length (DETOUR_LEN_AUTO)
    void AddHook(void* address, void* detour_func, void** original_storage, const char* name = nullptr, size_t detour_len = 0);
    
    // Apply all registered hooks (legacy - use module-specific versions)
    void ApplyAllHooks();
    
    // Apply hooks for specific modules
    void ApplyExeHooks();     // Apply hooks targeting sof.exe (0x200xxxxx)
    void ApplyRefHooks();     // Apply hooks targeting ref.dll (0x5xxxxxxx, ref module)
    void ApplyGameHooks();    // Apply hooks targeting game.dll (0x5xxxxxxx, game module)
    void ApplyPlayerHooks();  // Apply hooks targeting player.dll (0x5xxxxxxx, player module)
    
    // Remove all hooks
    void RemoveAllHooks();
    // Remove hooks for a specific module
    void RemoveExeHooks();
    void RemoveRefHooks();
    void RemoveGameHooks();
    void RemovePlayerHooks();
    
    // Get count of registered hooks
    size_t GetHookCount() const { return hooks.size(); }
    size_t GetHookCount(HookModule module) const;

private:
    HookManager() = default;
    
    // Determine module from address
    HookModule DetermineModule(void* address) const;
    
    // Apply hooks for a specific module
    void ApplyModuleHooks(HookModule module, const char* module_name);
    void RemoveModuleHooks(HookModule module, const char* module_name);
    
    std::vector<HookEntry> hooks;
    std::unordered_map<void*, void*> applied_hooks; // address -> trampoline
};
