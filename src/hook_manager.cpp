#include <windows.h>
#include "hook_manager.h"
#include "DetourXS/detourxs.h"
#include "util.h"

// Define patch types for DetourXS
#define DETOUR_TYPE_JMP 0
#define DETOUR_LEN_AUTO 0 // Placeholder for auto-length detection

HookManager& HookManager::Instance() {
    static HookManager instance;
    return instance;
}

HookModule HookManager::DetermineModule(void* address) const {
    uintptr_t addr = reinterpret_cast<uintptr_t>(address);
    
    // SoF.exe: 0x200xxxxx range
    if (addr >= 0x20000000 && addr < 0x30000000) {
        return HookModule::SofExe;
    }
    
    // ref.dll: observed in some environments at 0x300xxxxx range
    if (addr >= 0x30000000 && addr < 0x40000000) {
        return HookModule::RefDll;
    }
    
    // Generic DLL addresses: 0x5xxxxxxx range
    // Default to game.dll for these unless specified otherwise
    if (addr >= 0x50000000 && addr < 0x60000000) {
        return HookModule::GameDll;
    }
    
    return HookModule::Unknown;
}

void HookManager::AddHook(void* address, void* detour_func, void** original_storage, const char* name, size_t detour_len) {
    HookModule module = DetermineModule(address);
    hooks.push_back({address, detour_func, original_storage, name, module, detour_len});
    
    const char* module_name = "Unknown";
    switch (module) {
        case HookModule::SofExe: module_name = "SoF.exe"; break;
        case HookModule::RefDll: module_name = "ref.dll"; break;
        case HookModule::GameDll: module_name = "game.dll"; break;
        case HookModule::PlayerDll: module_name = "player.dll"; break;
        default: break;
    }
    
    PrintOut(PRINT_LOG, "Registered hook: %s at 0x%p (%s)\n", 
             name ? name : "unnamed", address, module_name);
}

void HookManager::ApplyModuleHooks(HookModule target_module, const char* module_name) {
    size_t applied_count = 0;
    size_t total_count = 0;
    
    // Count hooks for this module
    for (const auto& hook : hooks) {
        if (hook.module == target_module) {
            total_count++;
        }
    }
    
    if (total_count == 0) {
        PrintOut(PRINT_LOG, "No %s hooks to apply\n", module_name);
        return;
    }
    
    PrintOut(PRINT_LOG, "Applying %zu %s hooks...\n", total_count, module_name);

    // If this is the system-module pass, print the list of system hooks we're about to apply
    if (target_module == HookModule::Unknown) {
        PrintOut(PRINT_LOG, "System-module hooks to apply:\n");
        for (const auto& hook : hooks) {
            if (hook.module == HookModule::Unknown) {
                PrintOut(PRINT_LOG, " - %s at %p\n", hook.name ? hook.name : "unnamed", hook.address);
            }
        }
    }
    
    for (const auto& hook : hooks) {
        if (hook.module != target_module) {
            continue; // Skip hooks for other modules
        }
        
        if (applied_hooks.find(hook.address) != applied_hooks.end()) {
            PrintOut(PRINT_LOG, "Hook already applied: %s\n", hook.name ? hook.name : "unnamed");
            continue;
        }
        
        size_t effective_len = hook.detour_len == 0 ? DETOUR_LEN_AUTO : hook.detour_len;
        
        // Check if the address is readable before attempting to hook
        if (IsBadReadPtr(hook.address, 1)) {
            PrintOut(PRINT_BAD, "Cannot apply %s hook: %s at 0x%p (memory not accessible)\n", 
                     module_name, hook.name ? hook.name : "unnamed", hook.address);
            continue;
        }
        
        void* trampoline = DetourCreate(hook.address, hook.detour_func, DETOUR_TYPE_JMP, effective_len);
        
        if (trampoline) {
            if (hook.original_storage) {
                *hook.original_storage = trampoline;
            }
            applied_hooks[hook.address] = trampoline;
            applied_count++;
            PrintOut(PRINT_LOG, "Successfully applied %s hook: %s (0x%p -> 0x%p)\n", 
                     module_name, hook.name ? hook.name : "unnamed", hook.address, hook.detour_func);
        } else {
            PrintOut(PRINT_BAD, "Failed to apply %s hook: %s at 0x%p\n", 
                     module_name, hook.name ? hook.name : "unnamed", hook.address);
        }
    }
    
    PrintOut(PRINT_LOG, "Applied %zu/%zu %s hooks successfully\n", applied_count, total_count, module_name);
}

void HookManager::RemoveModuleHooks(HookModule target_module, const char* module_name) {
    size_t removed_count = 0;
    for (auto it = hooks.begin(); it != hooks.end(); ++it) {
        const auto& hook = *it;
        if (hook.module != target_module) {
            continue;
        }
        if (applied_hooks.count(hook.address)) {
            void* trampoline = applied_hooks[hook.address];
            if (trampoline) {
                // Check if the original address is still accessible
                // This prevents crashes when a DLL has been unloaded
                if (!IsBadReadPtr(hook.address, 1)) {
                    DetourRemove(&trampoline);
                } else {
                    PrintOut(PRINT_LOG, "Skipping removal of %s hook at 0x%p (memory no longer accessible)\n", 
                             hook.name ? hook.name : "unnamed", hook.address);
                }
                if (hook.original_storage) {
                    *hook.original_storage = nullptr;
                }
                applied_hooks.erase(hook.address);
                removed_count++;
            }
        }
    }
    if (removed_count) {
        PrintOut(PRINT_LOG, "Removed %zu %s hooks\n", removed_count, module_name);
    }
}

void HookManager::ApplyExeHooks() {
    ApplyModuleHooks(HookModule::SofExe, "SoF.exe");
}

void HookManager::ApplyRefHooks() {
    // Remove then apply on reload to handle ref.dll being reloaded
    RemoveModuleHooks(HookModule::RefDll, "ref.dll");
    ApplyModuleHooks(HookModule::RefDll, "ref.dll");
}

void HookManager::ApplyGameHooks() {
    // Remove then apply on reload to handle game.dll being reloaded
    RemoveModuleHooks(HookModule::GameDll, "game.dll");
    ApplyModuleHooks(HookModule::GameDll, "game.dll");
}

void HookManager::ApplyPlayerHooks() {
    // Remove then apply on reload to handle player.dll being reloaded
    RemoveModuleHooks(HookModule::PlayerDll, "player.dll");
    ApplyModuleHooks(HookModule::PlayerDll, "player.dll");
}

void HookManager::ApplySystemHooks() {
    ApplyModuleHooks(HookModule::Unknown, "system DLL");
}

void HookManager::RemoveExeHooks() {
    RemoveModuleHooks(HookModule::SofExe, "SoF.exe");
}

void HookManager::RemoveRefHooks() {
    RemoveModuleHooks(HookModule::RefDll, "ref.dll");
}

void HookManager::RemoveGameHooks() {
    RemoveModuleHooks(HookModule::GameDll, "game.dll");
}

void HookManager::RemovePlayerHooks() {
    RemoveModuleHooks(HookModule::PlayerDll, "player.dll");
}

size_t HookManager::GetHookCount(HookModule module) const {
    size_t count = 0;
    for (const auto& hook : hooks) {
        if (hook.module == module) {
            count++;
        }
    }
    return count;
}

void HookManager::DumpRegisteredHooks() const {
    PrintOut(PRINT_LOG, "Registered hooks (%zu):\n", hooks.size());
    for (const auto &h : hooks) {
        const char* module_name = "Unknown";
        switch (h.module) {
            case HookModule::SofExe: module_name = "SoF.exe"; break;
            case HookModule::RefDll: module_name = "ref.dll"; break;
            case HookModule::GameDll: module_name = "game.dll"; break;
            case HookModule::PlayerDll: module_name = "player.dll"; break;
            default: break;
        }
        PrintOut(PRINT_LOG, " - %s at %p (%s)\n", h.name ? h.name : "unnamed", h.address, module_name);
    }
}

void HookManager::RemoveAllHooks() {
    PrintOut(PRINT_LOG, "Removing %zu applied hooks...\n", applied_hooks.size());
    for (auto& hook : hooks) {
        if (applied_hooks.count(hook.address)) {
            void* trampoline = applied_hooks[hook.address];
            if (trampoline) {
                DetourRemove(&trampoline);
                if (hook.original_storage) {
                    *hook.original_storage = nullptr; // Clear the original pointer
                }
                PrintOut(PRINT_LOG, "Successfully removed hook: %s at 0x%p\n", hook.name ? hook.name : "unnamed", hook.address);
            }
            applied_hooks.erase(hook.address);
        }
    }
    hooks.clear();
}