#include <windows.h>
#include <string>
#include "detours.h"
#include "DetourXS/detourxs.h"
#include "util.h"

#define DETOUR_TYPE_JMP 0
#define DETOUR_LEN_AUTO 0

DetourSystem& DetourSystem::Instance() {
    static DetourSystem instance;
    return instance;
}

DetourModule DetourSystem::DetermineModule(void* address) const {
    uintptr_t addr = reinterpret_cast<uintptr_t>(address);
    
    if (addr < 0x10000000) {
        return DetourModule::Unknown;
    }
    
    if (addr >= 0x20000000 && addr < 0x30000000) {
        return DetourModule::SofExe;
    }
    
    if (addr >= 0x30000000 && addr < 0x40000000) {
        return DetourModule::RefDll;
    }
    
    if (addr >= 0x50000000 && addr < 0x60000000) {
        return DetourModule::GameDll;
    }
    
    return DetourModule::Unknown;
}

void* DetourSystem::ResolveAddress(void* address, DetourModule module, bool suppress_errors) const {
    uintptr_t address_as_value = reinterpret_cast<uintptr_t>(address);
    if (address_as_value >= 0x10000000) {
        return address;
    }
    
    uintptr_t rva_value = address_as_value;
    const char* module_name_str = nullptr;
    
    switch (module) {
        case DetourModule::RefDll: module_name_str = "ref_gl.dll"; break;
        case DetourModule::GameDll: module_name_str = "gamex86.dll"; break;
        case DetourModule::SofExe: module_name_str = "SoF.exe"; break;
        case DetourModule::PlayerDll: module_name_str = "player.dll"; break;
        default: break;
    }
    
    if (module_name_str) {
        void* dll_base = GetModuleBase(module_name_str);
        if (dll_base) {
            void* resolved = (void*)((uintptr_t)dll_base + rva_value);
            PrintOut(PRINT_LOG, "ResolveAddress: RVA 0x%p + base 0x%p = 0x%p for %s\n", address, dll_base, resolved, module_name_str);
            return resolved;
        } else {
            if (!suppress_errors) {
                PrintOut(PRINT_BAD, "ResolveAddress: GetModuleBase(%s) returned NULL for RVA 0x%p\n", module_name_str, address);
            }
        }
    }
    
    return nullptr;
}

void DetourSystem::RegisterDetour(void* address, void* detour_func, void** original_storage, const char* name, DetourModule module, size_t detour_len) {
    if (!name) {
        if (initialized) {
            PrintOut(PRINT_BAD, "RegisterDetour: detour name is required\n");
        }
        return;
    }
    
    if (!initialized) {
        DeferredDetourEntry* entry = static_cast<DeferredDetourEntry*>(malloc(sizeof(DeferredDetourEntry)));
        if (!entry) {
            return;
        }
        entry->address = address;
        entry->detour_func = detour_func;
        entry->original_storage = original_storage;
        entry->name = name;
        entry->module = module;
        entry->detour_len = detour_len;
        entry->next = deferred_registrations;
        deferred_registrations = entry;
        return;
    }
    
    RegisterDetourInternal(address, detour_func, original_storage, name, module, detour_len);
}

void DetourSystem::RegisterDetourInternal(void* address, void* detour_func, void** original_storage, const char* name, DetourModule module, size_t detour_len) {
    if (!name) {
        PrintOut(PRINT_BAD, "RegisterDetour: detour name is required\n");
        return;
    }
    
    std::string detour_name(name);
    if (registered_detour_names.find(detour_name) != registered_detour_names.end()) {
        PrintOut(PRINT_LOG, "Detour '%s' already registered, skipping duplicate registration (use REGISTER_CALLBACK instead)\n", name);
        return;
    }
    
    detours.push_back({address, detour_func, original_storage, name, module, detour_len});
    registered_detour_names[detour_name] = address;
    
    const char* module_name = "";
    switch (module) {
        case DetourModule::SofExe: module_name = "SoF.exe"; break;
        case DetourModule::RefDll: module_name = "ref.dll"; break;
        case DetourModule::GameDll: module_name = "game.dll"; break;
        case DetourModule::PlayerDll: module_name = "player.dll"; break;
        case DetourModule::Unknown: module_name = "Unknown"; break;
    }
    
    void* resolved_addr = ResolveAddress(address, module);
    if (!resolved_addr) {
        PrintOut(PRINT_BAD, "WARNING: Failed to resolve address for %s (raw: 0x%p, %s) - using raw address\n", name, address, module_name);
        resolved_addr = address;
    }
    PrintOut(PRINT_LOG, "Registered detour: %s at 0x%p (raw: 0x%p, %s)\n", name, resolved_addr, address, module_name);
}

void DetourSystem::ProcessDeferredRegistrations() {
    if (initialized) {
        return;
    }
    
    initialized = true;
    
    size_t count = 0;
    while (deferred_registrations) {
        DeferredDetourEntry* entry = deferred_registrations;
        deferred_registrations = deferred_registrations->next;
        
        RegisterDetourInternal(entry->address, entry->detour_func, entry->original_storage, entry->name, entry->module, entry->detour_len);
        
        free(entry);
        count++;
    }
    
    if (count > 0) {
        PrintOut(PRINT_LOG, "Processed %zu deferred detour registrations\n", count);
    }
}

bool DetourSystem::ApplyDetourAtAddress(void* address, void* detour_func, void** original_storage, const char* name, size_t detour_len) {
    if (!address || !detour_func) {
        return false;
    }
    
    if (applied_detours.find(address) != applied_detours.end()) {
        PrintOut(PRINT_LOG, "Detour already applied at %p: %s\n", address, name ? name : "unnamed");
        return true;
    }
    
    if (IsBadReadPtr(address, 1)) {
        PrintOut(PRINT_BAD, "Cannot apply %s detour at 0x%p (memory not accessible)\n", 
                 name ? name : "unnamed", address);
        return false;
    }
    
    size_t effective_len = detour_len == 0 ? DETOUR_LEN_AUTO : detour_len;
    void* trampoline = DetourCreate(address, detour_func, DETOUR_TYPE_JMP, effective_len);
    
    if (trampoline) {
        if (original_storage) {
            *original_storage = trampoline;
        }
        applied_detours[address] = trampoline;
        PrintOut(PRINT_LOG, "Applied detour at %p: %s\n", address, name ? name : "unnamed");
        return true;
    } else {
        PrintOut(PRINT_BAD, "Failed to apply %s detour at 0x%p\n", name ? name : "unnamed", address);
        return false;
    }
}

bool DetourSystem::RemoveDetourAtAddress(void* address) {
    if (!address) {
        return false;
    }
    
    auto it = applied_detours.find(address);
    if (it == applied_detours.end()) {
        return false;
    }
    
    void* trampoline = it->second;
    if (trampoline && !IsBadReadPtr(trampoline, 1)) {
        if (!IsBadReadPtr(address, 1)) {
            DetourRemove(&trampoline);
        }
        applied_detours.erase(it);
        PrintOut(PRINT_LOG, "Removed detour at %p\n", address);
        return true;
    }
    
    return false;
}

bool DetourSystem::IsDetourApplied(void* address) const {
    void* resolved = const_cast<DetourSystem*>(this)->ResolveAddress(address, DetourModule::Unknown);
    if (!resolved) resolved = address;
    return applied_detours.find(resolved) != applied_detours.end();
}

bool DetourSystem::IsDetourRegistered(const char* name) const {
    if (!name) return false;
    return registered_detour_names.find(std::string(name)) != registered_detour_names.end();
}

void DetourSystem::ApplyModuleDetours(DetourModule target_module, const char* module_name) {
    size_t applied_count = 0;
    
    PrintOut(PRINT_LOG, "Applying %s detours...\n", module_name);
    
    if (target_module == DetourModule::Unknown) {
        PrintOut(PRINT_LOG, "System-module detours to apply:\n");
        for (const auto& detour : detours) {
            if (detour.module == DetourModule::Unknown) {
                PrintOut(PRINT_LOG, " - %s at %p\n", detour.name ? detour.name : "unnamed", detour.address);
            }
        }
    }
    
    for (auto& detour : detours) {
        if (detour.module != target_module) {
            continue;
        }
        
        void* absolute_addr = ResolveAddress(detour.address, target_module);
        if (!absolute_addr) {
            const char* module_name_str = nullptr;
            switch (target_module) {
                case DetourModule::RefDll: module_name_str = "ref_gl.dll"; break;
                case DetourModule::GameDll: module_name_str = "gamex86.dll"; break;
                case DetourModule::SofExe: module_name_str = "SoF.exe"; break;
                case DetourModule::PlayerDll: module_name_str = "player.dll"; break;
                default: break;
            }
            if (module_name_str) {
                PrintOut(PRINT_BAD, "Skipping %s detour: Failed to resolve address 0x%p for %s (module may not be loaded or address resolution failed)\n", 
                         detour.name ? detour.name : "unnamed", detour.address, module_name_str);
            }
            continue;
        }
        
        if (applied_detours.find(absolute_addr) != applied_detours.end()) {
            PrintOut(PRINT_LOG, "Detour already applied: %s\n", detour.name ? detour.name : "unnamed");
            continue;
        }
        
        size_t effective_len = detour.detour_len == 0 ? DETOUR_LEN_AUTO : detour.detour_len;
        
        if (IsBadReadPtr(absolute_addr, 1)) {
            PrintOut(PRINT_BAD, "Cannot apply %s detour at 0x%p (memory not accessible)\n", 
                     detour.name ? detour.name : "unnamed", absolute_addr);
            continue;
        }
        
        PrintOut(PRINT_LOG, "Attempting to apply detour: %s at 0x%p -> hook function 0x%p\n", 
                 detour.name ? detour.name : "unnamed", absolute_addr, detour.detour_func);
        
        if (IsBadReadPtr(detour.detour_func, 1)) {
            PrintOut(PRINT_BAD, "Cannot apply %s detour: hook function at 0x%p is not accessible\n", 
                     detour.name ? detour.name : "unnamed", detour.detour_func);
            continue;
        }
        
        void* trampoline = DetourCreate(absolute_addr, detour.detour_func, DETOUR_TYPE_JMP, effective_len);
        
        if (trampoline) {
            if (detour.original_storage) {
                *detour.original_storage = trampoline;
                PrintOut(PRINT_LOG, "Set original_storage for %s: 0x%p -> 0x%p\n", 
                         detour.name ? detour.name : "unnamed", detour.original_storage, trampoline);
            } else {
                PrintOut(PRINT_BAD, "WARNING: No original_storage provided for %s! Original function pointer will not be set!\n", 
                         detour.name ? detour.name : "unnamed");
            }
            applied_detours[absolute_addr] = trampoline;
            PrintOut(PRINT_LOG, "Successfully applied detour: %s at 0x%p -> 0x%p (trampoline: 0x%p)\n", 
                     detour.name ? detour.name : "unnamed", absolute_addr, detour.detour_func, trampoline);
            applied_count++;
        } else {
            PrintOut(PRINT_BAD, "Failed to apply %s detour at 0x%p -> 0x%p (DetourCreate returned NULL)\n", 
                     detour.name ? detour.name : "unnamed", absolute_addr, detour.detour_func);
        }
    }
    
    PrintOut(PRINT_LOG, "Applied %zu %s detours successfully\n", applied_count, module_name);
}

void DetourSystem::RemoveModuleDetours(DetourModule target_module, const char* module_name) {
    size_t removed_count = 0;
    for (auto it = detours.begin(); it != detours.end(); ++it) {
        const auto& detour = *it;
        if (detour.module != target_module) {
            continue;
        }
        
        void* absolute_addr = ResolveAddress(detour.address, target_module, true);
        if (!absolute_addr) {
            absolute_addr = detour.address;
        }
        
        if (applied_detours.count(absolute_addr)) {
            void* trampoline = applied_detours[absolute_addr];
            if (trampoline && !IsBadReadPtr(trampoline, 1)) {
                if (!IsBadReadPtr(absolute_addr, 1)) {
                    DetourRemove(&trampoline);
                }
                if (detour.original_storage && !IsBadWritePtr(detour.original_storage, sizeof(void*))) {
                    *detour.original_storage = nullptr;
                }
                applied_detours.erase(absolute_addr);
                removed_count++;
            }
        }
    }
    if (removed_count) {
        PrintOut(PRINT_LOG, "Removed %zu %s detours\n", removed_count, module_name);
    }
}

void DetourSystem::ApplyExeDetours() {
    ApplyModuleDetours(DetourModule::SofExe, "SoF.exe");
}

void DetourSystem::ApplyRefDetours() {
    ApplyModuleDetours(DetourModule::RefDll, "ref.dll");
}

void DetourSystem::ApplyGameDetours() {
    ApplyModuleDetours(DetourModule::GameDll, "game.dll");
}

void DetourSystem::ApplyPlayerDetours() {
    RemoveModuleDetours(DetourModule::PlayerDll, "player.dll");
    ApplyModuleDetours(DetourModule::PlayerDll, "player.dll");
}

void DetourSystem::ApplySystemDetours() {
    ApplyModuleDetours(DetourModule::Unknown, "system DLL");
}

void DetourSystem::RemoveExeDetours() {
    RemoveModuleDetours(DetourModule::SofExe, "SoF.exe");
}

void DetourSystem::RemoveRefDetours() {
    RemoveModuleDetours(DetourModule::RefDll, "ref.dll");
}

void DetourSystem::RemoveGameDetours() {
    RemoveModuleDetours(DetourModule::GameDll, "game.dll");
}

void DetourSystem::RemovePlayerDetours() {
    RemoveModuleDetours(DetourModule::PlayerDll, "player.dll");
}

void DetourSystem::RemoveAllDetours() {
    PrintOut(PRINT_LOG, "Removing %zu applied detours...\n", applied_detours.size());
    for (auto& detour : detours) {
        void* absolute_addr = ResolveAddress(detour.address, detour.module);
        if (!absolute_addr) {
            absolute_addr = detour.address;
        }
        if (applied_detours.count(absolute_addr)) {
            void* trampoline = applied_detours[absolute_addr];
            if (trampoline) {
                DetourRemove(&trampoline);
                if (detour.original_storage) {
                    *detour.original_storage = nullptr;
                }
                PrintOut(PRINT_LOG, "Successfully removed detour: %s at 0x%p\n", detour.name ? detour.name : "unnamed", absolute_addr);
            }
            applied_detours.erase(absolute_addr);
        }
    }
    detours.clear();
}

size_t DetourSystem::GetDetourCount(DetourModule module) const {
    size_t count = 0;
    for (const auto& detour : detours) {
        if (detour.module == module) {
            count++;
        }
    }
    return count;
}

void DetourSystem::DumpRegisteredDetours() const {
    PrintOut(PRINT_LOG, "Registered detours (%zu):\n", detours.size());
    for (const auto &d : detours) {
        const char* module_name = "Unknown";
        switch (d.module) {
            case DetourModule::SofExe: module_name = "SoF.exe"; break;
            case DetourModule::RefDll: module_name = "ref.dll"; break;
            case DetourModule::GameDll: module_name = "game.dll"; break;
            case DetourModule::PlayerDll: module_name = "player.dll"; break;
            default: break;
        }
        PrintOut(PRINT_LOG, " - %s at %p (%s)\n", d.name ? d.name : "unnamed", d.address, module_name);
    }
}

