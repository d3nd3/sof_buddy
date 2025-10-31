#pragma once

#include <stdint.h>
#if defined(_WIN32)
#include <windows.h>
#endif
#include <string.h>

#ifndef HOOKCALLSITE_DEBUG
#define HOOKCALLSITE_DEBUG 0
#endif

#include "callsite_classifier.h"
#include "parent_recorder.h"
#include "util.h"

namespace HookCallsite {

// Classify the immediate caller of the current hook using __builtin_return_address(0)
inline bool classifyCaller(CallerInfo &out) {
    void *ra = __builtin_return_address(0);
    return CallsiteClassifier::classify(ra, out);
}

// Classify the first external caller (skips our own DLL frames)
inline bool classifyExternalCaller(CallerInfo &out) {
#if defined(_WIN32)
    // First, try portable RA depth scanning to avoid CaptureStackBackTrace quirks
    {
        HMODULE self = nullptr;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&classifyExternalCaller),
            &self);
        auto getReturnAddressAtDepth = [] (int depth) -> void* {
            switch (depth) {
                case 0: return __builtin_return_address(0);
                case 1: return __builtin_return_address(1);
                case 2: return __builtin_return_address(2);
                case 3: return __builtin_return_address(3);
                case 4: return __builtin_return_address(4);
                case 5: return __builtin_return_address(5);
                case 6: return __builtin_return_address(6);
                case 7: return __builtin_return_address(7);
                case 8: return __builtin_return_address(8);
                default: return nullptr;
            }
        };
        for (int depth = 0; depth <= 8; ++depth) {
            void *ra = getReturnAddressAtDepth(depth);
            if (!ra) break;
            if ((uintptr_t)ra < 0x10000) {
                #if HOOKCALLSITE_DEBUG
                PrintOut(PRINT_LOG, "HookCallsite: skipping invalid low ra-depth[%d]=0x%p\n", depth, ra);
                #endif
                continue;
            }
            HMODULE h = nullptr;
            if (!GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(ra), &h)) {
                continue;
            }
            if (!h || (uintptr_t)h < 0x10000) continue;
            char modPath[MAX_PATH] = {0};
            const char *leaf = modPath;
            if (h && GetModuleFileNameA(h, modPath, MAX_PATH)) {
                const char *b1 = strrchr(modPath, '\\');
                const char *b2 = strrchr(modPath, '/');
                const char *last = (b1 && b2) ? (b1 > b2 ? b1 : b2) : (b1 ? b1 : b2);
                leaf = last ? last + 1 : modPath;
            }
            bool isSelf = (h == self);
            bool isOurDllByName = (leaf && _stricmp(leaf, "sof_buddy.dll") == 0);
            #if HOOKCALLSITE_DEBUG
            PrintOut(PRINT_LOG, "HookCallsite: ra-depth[%d]=0x%p module=%s %s%s\n", depth, ra, leaf ? leaf : "<unknown>", isSelf ? "(self, skip)" : "", (!isSelf && isOurDllByName) ? "(name-match, skip)" : "");
            #endif
            if (isSelf || isOurDllByName) continue;
            if (CallsiteClassifier::classify(ra, out) && out.module != Module::Unknown) {
                // Ensure the function start exists for the resolved module; otherwise keep scanning
                if (CallsiteClassifier::hasFunctionStart(out.module, (uint32_t)out.functionStartRva)) {
                    return true;
                }
            }
        }
    }

    // Fallback to OS backtrace if depth walk didn't find anything
    void *frames[32] = {0};
    USHORT n = CaptureStackBackTrace(0, (ULONG)_countof(frames), frames, NULL);
    // Resolve our own module to skip its frames
    HMODULE self = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&classifyExternalCaller),
        &self);
    for (USHORT i = 1; i < n; ++i) { // start at 1 to skip current frame
        if (!frames[i]) continue;
        if ((uintptr_t)frames[i] < 0x10000) {
            #if HOOKCALLSITE_DEBUG
            PrintOut(PRINT_LOG, "HookCallsite: skipping invalid low frame[%u]=0x%p\n", (unsigned)i, frames[i]);
            #endif
            continue;
        }
        // Skip frames that belong to our own DLL
        HMODULE h = nullptr;
        if (GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(frames[i]), &h)) {
            if (!h || (uintptr_t)h < 0x10000) continue;
            char modPath[MAX_PATH] = {0};
            const char *leaf = modPath;
            if (h && GetModuleFileNameA(h, modPath, MAX_PATH)) {
                const char *b1 = strrchr(modPath, '\\');
                const char *b2 = strrchr(modPath, '/');
                const char *last = (b1 && b2) ? (b1 > b2 ? b1 : b2) : (b1 ? b1 : b2);
                leaf = last ? last + 1 : modPath;
            }
            bool isSelf = (h == self);
            // Also skip by filename equality in case module handles differ
            bool isOurDllByName = (leaf && _stricmp(leaf, "sof_buddy.dll") == 0);
            #if HOOKCALLSITE_DEBUG
            PrintOut(PRINT_LOG, "HookCallsite: frame[%u]=0x%p module=%s %s%s\n", (unsigned)i, frames[i], leaf ? leaf : "<unknown>", isSelf ? "(self, skip)" : "", (!isSelf && isOurDllByName) ? "(name-match, skip)" : "");
            #endif
            if (isOurDllByName) continue;
            if (isSelf) continue;
        }
        if (CallsiteClassifier::classify(frames[i], out) && out.module != Module::Unknown) {
            if (CallsiteClassifier::hasFunctionStart(out.module, (uint32_t)out.functionStartRva)) {
                return true;
            }
        }
    }
    // No fallback to self frames; return false if no external frame found
    return false;
#else
    return classifyCaller(out);
#endif
}

// Convenience: return fnStart rva (0 if unknown)
inline uint32_t getCallerFnStart() {
    CallerInfo info{};
    if (!classifyCaller(info)) return 0;
    return info.functionStartRva;
}

// External caller version
inline uint32_t GetFnStartExternal() {
    CallerInfo info{};
    if (!classifyExternalCaller(info)) return 0;
    return info.functionStartRva;
}

// Alias with the requested name (one-liner, no recording)
inline uint32_t GetFnStart() {
    return getCallerFnStart();
}

// Convenience: record parent for a child hook name (no-op if fnStart unknown)
inline void recordParent(const char *childName) {
    CallerInfo info{};
    if (!classifyCaller(info)) return;
    ParentRecorder::Instance().record(childName, info);
}

// One-liner: records parent and returns fnStart (0 if unknown)
inline uint32_t recordAndGetFnStart(const char *childName) {
    CallerInfo info{};
    if (!classifyCaller(info)) return 0;
    #ifndef NDEBUG
    ParentRecorder::Instance().record(childName, info);
    #endif
    return info.functionStartRva;
}

// External caller version that also records
inline uint32_t recordAndGetFnStartExternal(const char *childName) {
    
    CallerInfo info{};
    if (!classifyExternalCaller(info)) return 0;
    #ifndef NDEBUG
    ParentRecorder::Instance().record(childName, info);
    #endif
    return info.functionStartRva;

}

}


