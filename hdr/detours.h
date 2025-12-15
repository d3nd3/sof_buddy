#pragma once

#include <vector>
#include <unordered_map>
#include <windows.h>
#include "shared_hook_manager.h"
#include "typed_shared_hook_manager.h"
#include "sof_compat.h"
#include <tuple>

enum class DetourModule {
    SofExe,
    RefDll,
    GameDll,
    PlayerDll,
    Unknown
};

struct DetourEntry {
    void* address;
    void* detour_func;
    void** original_storage;
    const char* name;
    DetourModule module;
    size_t detour_len;
};

struct DeferredDetourEntry {
    void* address;
    void* detour_func;
    void** original_storage;
    const char* name;
    DetourModule module;
    size_t detour_len;
    DeferredDetourEntry* next;
};

class DetourSystem {
public:
    static DetourSystem& Instance();
    
    void RegisterDetour(void* address, void* detour_func, void** original_storage, const char* name, DetourModule module, size_t detour_len);
    void ProcessDeferredRegistrations();
    
    bool ApplyDetourAtAddress(void* address, void* detour_func, void** original_storage, const char* name, size_t detour_len);
    bool RemoveDetourAtAddress(void* address);
    
    bool IsDetourApplied(void* address) const;
    bool IsDetourRegistered(const char* name) const;
    
    void ApplyExeDetours();
    void ApplyRefDetours();
    void ApplyGameDetours();
    void ApplyPlayerDetours();
    void ApplySystemDetours();
    
    void RemoveExeDetours();
    void RemoveRefDetours();
    void RemoveGameDetours();
    void RemovePlayerDetours();
    void RemoveAllDetours();
    
    size_t GetDetourCount() const { return detours.size(); }
    size_t GetDetourCount(DetourModule module) const;
    void DumpRegisteredDetours() const;

private:
    DetourSystem() : deferred_registrations(nullptr), initialized(false) {}
    
    DetourModule DetermineModule(void* address) const;
    void ApplyModuleDetours(DetourModule target_module, const char* module_name);
    void RemoveModuleDetours(DetourModule target_module, const char* module_name);
    void* ResolveAddress(void* address, DetourModule module, bool suppress_errors = false) const;
    void RegisterDetourInternal(void* address, void* detour_func, void** original_storage, const char* name, DetourModule module, size_t detour_len);
    
    std::vector<DetourEntry> detours;
    std::unordered_map<void*, void*> applied_detours;
    std::unordered_map<std::string, void*> registered_detour_names;
    
    DeferredDetourEntry* deferred_registrations;
    bool initialized;
};

inline DetourSystem& GetDetourSystem() { return DetourSystem::Instance(); }

template<typename T> struct function_traits;
template<typename Ret, typename... Args> struct function_traits<Ret(Args...)> {
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arg_count = sizeof...(Args);
};

template<typename Ret, typename... Args> struct function_traits<Ret(*)(Args...)> {
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arg_count = sizeof...(Args);
};

template<typename T> struct extract_function_signature;
template<typename Ret, typename... Args> struct extract_function_signature<Ret(*)(Args...)> {
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
};
template<typename Ret, typename... Args> struct extract_function_signature<Ret(__stdcall*)(Args...)> {
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
};

template<typename FuncPtr>
struct MakeTypedManagerFromFunc {
    using return_type = typename extract_function_signature<FuncPtr>::return_type;
    using args_tuple = typename extract_function_signature<FuncPtr>::args_tuple;
};

template<typename Ret, typename Tuple> struct manager_from_tuple;
template<typename Ret, typename... Args>
struct manager_from_tuple<Ret, std::tuple<Args...>> {
    using type = TypedSharedHookManager<Ret, Args...>;
};

// Global variables for main detours (VID_LoadRefresh, Sys_GetGameApi)
extern char* g_vid_loadrefresh_name;
extern qboolean g_vid_loadrefresh_ret;
extern void* g_sys_getgameapi_imports;
extern void* g_sys_getgameapi_ret;

// REGISTER_DETOUR and REGISTER_DETOUR_VOID macros have been removed.
// Use detours.yaml and hooks.json files instead.
// See tools/generate_hooks.py for the new data-driven hook system.

// REGISTER_MODULE_DETOUR macro has been removed.
// Use detours.yaml and hooks.json files instead.
// See tools/generate_hooks.py for the new data-driven hook system.

#define REGISTER_CALLBACK(hook_name, feature_name, callback_name, priority, phase) \
    namespace { \
        struct AutoCallback_##hook_name##_##feature_name##_##callback_name { \
            AutoCallback_##hook_name##_##feature_name##_##callback_name() { \
                SharedHookManager::Instance().RegisterCallback( \
                    #hook_name, #feature_name, #callback_name, \
                    []() { callback_name(); }, priority, SharedHookPhase::phase); \
            } \
        }; \
        static AutoCallback_##hook_name##_##feature_name##_##callback_name \
            g_AutoCallback_##hook_name##_##feature_name##_##callback_name; \
    }

#define REGISTER_DETOUR_CALLBACK(detour_name, feature_name, callback_name, priority, phase) \
    namespace { \
        struct AutoDetourCallback_##detour_name##_##feature_name##_##callback_name { \
            AutoDetourCallback_##detour_name##_##feature_name##_##callback_name() { \
                using namespace detour_##detour_name; \
                if (SharedHookPhase::phase == SharedHookPhase::Pre) { \
                    manager.RegisterPreCallback( \
                        #feature_name, #callback_name, \
                        [](auto&... args) { callback_name(args...); }, priority); \
                } else { \
                    manager.RegisterPostCallback( \
                        #feature_name, #callback_name, \
                        [](auto result, auto... args) { return callback_name(result, args...); }, priority); \
                } \
            } \
        }; \
        static AutoDetourCallback_##detour_name##_##feature_name##_##callback_name \
            g_AutoDetourCallback_##detour_name##_##feature_name##_##callback_name; \
    }

