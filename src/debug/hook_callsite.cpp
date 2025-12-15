#include "debug/hook_callsite.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#include <string.h>
#endif

#ifndef HOOKCALLSITE_DEBUG
#define HOOKCALLSITE_DEBUG 0
#endif

#include "util.h"
#include "debug/parent_recorder.h"

namespace HookCallsite {

static HMODULE cachedSelf = nullptr;
static uintptr_t cachedSelfBase = 0;
static uintptr_t cachedSelfEnd = 0;

static inline HMODULE getCachedSelf() {
    return cachedSelf;
}

static inline bool isInCachedSelf(uintptr_t addr) {
    return addr >= cachedSelfBase && addr < cachedSelfEnd;
}

void cacheSelfModule() {
#if defined(_WIN32)
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&cacheSelfModule),
        &cachedSelf);
    if (!cachedSelf) {
        PrintOut(PRINT_BAD, "HookCallsite: ERROR - Failed to get sof_buddy.dll handle\n");
        ExitProcess(1);
    }
    MODULEINFO mi = {0};
    if (!GetModuleInformation(GetCurrentProcess(), cachedSelf, &mi, sizeof(mi))) {
        PrintOut(PRINT_BAD, "HookCallsite: ERROR - Failed to get sof_buddy.dll module info\n");
        ExitProcess(1);
    }
    cachedSelfBase = (uintptr_t)mi.lpBaseOfDll;
    cachedSelfEnd = cachedSelfBase + (uintptr_t)mi.SizeOfImage;
#endif
}


static inline bool checkAndClassifyAddress(void *addr, CallerInfo &out) {
    uintptr_t addrVal = (uintptr_t)addr;
    if (!addr || addrVal < 0x10000 || isInCachedSelf(addrVal)) return false;
    
    if (!CallsiteClassifier::classify(addr, out)) return false;
    
    if (out.module == Module::Unknown) {
        PrintOut(PRINT_BAD, "HookCallsite: ERROR - Module not in cache for addr=0x%p\n", addr);
        ExitProcess(1);
    }
    
    return CallsiteClassifier::hasFunctionStart(out.module, (uint32_t)out.functionStartRva);
}

inline bool classifyCaller(CallerInfo &out) {
    void *ra = __builtin_return_address(0);
    return CallsiteClassifier::classify(ra, out);
}

inline bool classifyExternalCaller(CallerInfo &out) {
#if defined(_WIN32)
    out.module = Module::Unknown;
    out.rva = 0;
    out.functionStartRva = 0;
    out.name = nullptr;
    
    void *ra0 = __builtin_return_address(0);
    if (checkAndClassifyAddress(ra0, out)) return true;
    
    struct StackFrame {
        StackFrame* ebp;
        void* ret;
    };
    
    StackFrame* frame;
    __asm__ __volatile__("movl %%ebp, %0" : "=r"(frame));
    
    for (int i = 0; i < 32 && frame && (uintptr_t)frame > 0x10000 && (uintptr_t)frame < 0x7FFFFFFF; ++i) {
        if (checkAndClassifyAddress(frame->ret, out)) return true;
        frame = frame->ebp;
    }
    
    return false;
#else
    return classifyCaller(out);
#endif
}

inline uint32_t getCallerFnStart() {
    CallerInfo info{};
    if (!classifyCaller(info)) return 0;
    return info.functionStartRva;
}

inline uint32_t GetFnStartExternal() {
    CallerInfo info{};
    if (!classifyExternalCaller(info)) return 0;
    return info.functionStartRva;
}

inline uint32_t GetFnStart() {
    return getCallerFnStart();
}

inline void recordParent(const char *childName) {
    CallerInfo info{};
    if (!classifyCaller(info)) return;
    ParentRecorder::Instance().record(childName, info);
}

// Get the immediate caller's function start RVA (the function that directly called this hook)
inline uint32_t recordAndGetFnStart(const char *childName) {
    CallerInfo info{};
    if (!classifyCaller(info)) return 0;
    #if !defined(NDEBUG) && defined(SOFBUDDY_ENABLE_CALLSITE_LOGGER)
    ParentRecorder::Instance().record(childName, info);
    #endif
    return info.functionStartRva;
}

// Get the first external caller's function start RVA (walks up the stack to find the first
// caller outside of sof_buddy.dll). Useful when sof_buddy.dll functions call each other
// internally, but you need to know which external module (SoF.exe, ref_gl.dll, etc.) is
// the "real" caller that triggered the call chain.
uint32_t recordAndGetFnStartExternal(const char *childName) {
    CallerInfo info{};
    if (!classifyExternalCaller(info)) return 0;
    #if !defined(NDEBUG) && defined(SOFBUDDY_ENABLE_CALLSITE_LOGGER)
    ParentRecorder::Instance().record(childName, info);
    #endif
    return info.functionStartRva;
}

}

