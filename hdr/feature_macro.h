#pragma once
#include <windows.h>
#include "hook_manager.h"

inline HookManager& GetHookManager() { return HookManager::Instance(); }

// Simple macro that defines typedef, original pointer, and auto-registration
// Usage: REGISTER_HOOK(FunctionName, 0x140123450, void, __fastcall, int param1, float param2)
#define REGISTER_HOOK(name, addr, ret, conv, ...)                \
    using t##name = ret(conv*)(__VA_ARGS__);                     \
    t##name o##name = nullptr;                            \
    ret conv hk##name(__VA_ARGS__);                             \
    namespace {                                                  \
        struct AutoHook_##name {                                 \
            AutoHook_##name() {                                  \
                GetHookManager().AddHook(                        \
                    reinterpret_cast<void*>(addr),               \
                    reinterpret_cast<void*>(hk##name),           \
                    reinterpret_cast<void**>(&o##name),          \
                    #name);                                      \
            }                                                    \
        };                                                       \
        static AutoHook_##name g_AutoHook_##name;                \
    }                                                            \
    ret conv hk##name(__VA_ARGS__)

// Variant that allows specifying detour length in bytes
#define REGISTER_HOOK_LEN(name, addr, len, ret, conv, ...)       \
    using t##name = ret(conv*)(__VA_ARGS__);                     \
    t##name o##name = nullptr;                            \
    ret conv hk##name(__VA_ARGS__);                             \
    namespace {                                                  \
        struct AutoHook_##name {                                 \
            AutoHook_##name() {                                  \
                GetHookManager().AddHook(                        \
                    reinterpret_cast<void*>(addr),               \
                    reinterpret_cast<void*>(hk##name),           \
                    reinterpret_cast<void**>(&o##name),          \
                    #name,                                       \
                    static_cast<size_t>(len));                   \
            }                                                    \
        };                                                       \
        static AutoHook_##name g_AutoHook_##name;                \
    }                                                            \
    ret conv hk##name(__VA_ARGS__)

// For hooks without parameters
#define REGISTER_HOOK_VOID(name, addr, ret, conv)                \
    using t##name = ret(conv*)();                                \
    t##name o##name = nullptr;                            \
    ret conv hk##name();                                        \
    namespace {                                                  \
        struct AutoHook_##name {                                 \
            AutoHook_##name() {                                  \
                GetHookManager().AddHook(                        \
                    reinterpret_cast<void*>(addr),               \
                    reinterpret_cast<void*>(hk##name),           \
                    reinterpret_cast<void**>(&o##name),          \
                    #name);                                      \
            }                                                    \
        };                                                       \
        static AutoHook_##name g_AutoHook_##name;                \
    }                                                            \
    ret conv hk##name()

// For module-based hooks (DLL exports)
#define REGISTER_MODULE_HOOK(name, module, proc, ret, conv, ...) \
    using t##name = ret(conv*)(__VA_ARGS__);                     \
    t##name o##name = nullptr;                            \
    ret conv hk##name(__VA_ARGS__);                             \
    namespace {                                                  \
        struct AutoHook_##name {                                 \
            AutoHook_##name() {                                  \
                HMODULE hMod = GetModuleHandleA(module);         \
                if (!hMod) {                                     \
                    PrintOut(PRINT_BAD, "REGISTER_MODULE_HOOK(%s): GetModuleHandleA(\"%s\") failed\n", #name, module); \
                    return;                                      \
                }                                                \
                FARPROC procAddr = GetProcAddress(hMod, proc);   \
                if (!procAddr) {                                 \
                    PrintOut(PRINT_BAD, "REGISTER_MODULE_HOOK(%s): GetProcAddress(\"%s\") failed\n", #name, proc); \
                    return;                                      \
                }                                                \
                void* addr = reinterpret_cast<void*>(procAddr);  \
                GetHookManager().AddHook(                        \
                    addr,                                        \
                    reinterpret_cast<void*>(hk##name),           \
                    reinterpret_cast<void**>(&o##name),          \
                    #name);                                      \
                PrintOut(PRINT_LOG, "REGISTER_MODULE_HOOK(%s): Hooked %s!%s at %p\n", #name, module, proc, addr); \
            }                                                    \
        };                                                       \
        static AutoHook_##name g_AutoHook_##name;                \
    }                                                            \
    ret conv hk##name(__VA_ARGS__)
