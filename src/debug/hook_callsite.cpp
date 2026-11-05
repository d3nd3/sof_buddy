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

struct StackFrame {
    StackFrame* ebp;
    void* ret;
};

struct StackBounds {
    uintptr_t lo;
    uintptr_t hi;
    bool valid;
};

static inline StackBounds currentThreadStackBounds() {
    StackBounds b{0, 0, false};
#if defined(_WIN32) && defined(__i386__)
    uintptr_t base = 0;
    uintptr_t limit = 0;
    __asm__ __volatile__("movl %%fs:4, %0" : "=r"(base));
    __asm__ __volatile__("movl %%fs:8, %0" : "=r"(limit));
    if (base > limit && limit >= 0x10000u) {
        b.lo = limit;
        b.hi = base;
        b.valid = true;
    }
#endif
    return b;
}

#if defined(_WIN32)
static inline bool readableRange(const void* p, size_t len) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const uintptr_t start = reinterpret_cast<uintptr_t>(p);
    const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (regionEnd < start) return false;
    return (regionEnd - start) >= len;
}
#endif

static inline bool plausibleFramePtr(const StackFrame* frame, const StackBounds& sb) {
    if (!frame) return false;
    const uintptr_t v = reinterpret_cast<uintptr_t>(frame);
    if ((v & 3u) != 0u) return false;
    if (sb.valid) return v >= sb.lo && v <= sb.hi - sizeof(StackFrame);
#if defined(_WIN32)
    if (v < 0x10000u || v >= 0x7FFF0000u) return false;
    return readableRange(frame, sizeof(StackFrame));
#else
    return v >= 0x10000u && v < 0x7FFF0000u;
#endif
}

static inline StackFrame* nextStackFrame(StackFrame* frame, const StackBounds& sb) {
    if (!plausibleFramePtr(frame, sb)) return nullptr;
    StackFrame* next = frame->ebp;
    if (!plausibleFramePtr(next, sb)) return nullptr;
    if (reinterpret_cast<uintptr_t>(next) <= reinterpret_cast<uintptr_t>(frame)) return nullptr;
    return next;
}

static inline bool walkExternalFrames(bool (*tryAddr)(void*, void*), void* ctx) {
    if (tryAddr(__builtin_return_address(0), ctx)) return true;

    StackFrame* frame;
    __asm__ __volatile__("movl %%ebp, %0" : "=r"(frame));

    const StackBounds sb = currentThreadStackBounds();
    for (int i = 0; i < 32; ++i) {
        if (!plausibleFramePtr(frame, sb)) break;
        if (tryAddr(frame->ret, ctx)) return true;
        frame = nextStackFrame(frame, sb);
        if (!frame) break;
    }
    return false;
}

struct ExternalWalkCtx { CallerInfo* out; };
struct ModuleWalkCtx { Module target; CallerInfo* out; };

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
    if (addrVal < 0x10000 || isInCachedSelf(addrVal)) return false;
    
    if (!CallsiteClassifier::classify(addr, out)) return false;
    
    if (out.module == Module::Unknown) {
        PrintOut(PRINT_BAD, "HookCallsite: ERROR - Module not in cache for addr=0x%p\n", addr);
        ExitProcess(1);
    }
    
    return CallsiteClassifier::hasFunctionStart(out.module, (uint32_t)out.functionStartRva);
}

static bool tryClassifyExternal(void* addr, void* raw) {
    return checkAndClassifyAddress(addr, *static_cast<ExternalWalkCtx*>(raw)->out);
}

static bool tryClassifyModule(void* addr, void* raw) {
    ModuleWalkCtx* c = static_cast<ModuleWalkCtx*>(raw);
    CallerInfo tmp{};
    if (!checkAndClassifyAddress(addr, tmp)) return false;
    if (tmp.module != c->target) return false;
    *c->out = tmp;
    return true;
}

struct VisitCtx { ExternalCallerVisitor visitor; void* ctx; };

static bool visitAdapter(void* addr, void* raw) {
    VisitCtx* v = static_cast<VisitCtx*>(raw);
    CallerInfo info{};
    if (!checkAndClassifyAddress(addr, info)) return false;
    return v->visitor(info, v->ctx);
}

bool visitExternalCallers(ExternalCallerVisitor visitor, void* ctx) {
#if defined(_WIN32)
    VisitCtx v{visitor, ctx};
    return walkExternalFrames(visitAdapter, &v);
#else
    (void)visitor;
    (void)ctx;
    return false;
#endif
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

    ExternalWalkCtx ctx{&out};
    return walkExternalFrames(tryClassifyExternal, &ctx);
#else
    return classifyCaller(out);
#endif
}

bool classifyStackCallerInModule(Module target, CallerInfo &out) {
#if defined(_WIN32)
    out.module = Module::Unknown;
    out.rva = 0;
    out.functionStartRva = 0;
    out.name = nullptr;

    ModuleWalkCtx ctx{target, &out};
    return walkExternalFrames(tryClassifyModule, &ctx);
#else
    (void)target;
    (void)out;
    return false;
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

uint32_t recordAndGetCallerRvaExternal(const char *childName) {
    CallerInfo info{};
    if (!classifyExternalCaller(info)) return 0;
    #if !defined(NDEBUG) && defined(SOFBUDDY_ENABLE_CALLSITE_LOGGER)
    ParentRecorder::Instance().record(childName, info);
    #endif
    return (uint32_t)info.rva;
}

uint32_t recordAndGetSofExeCallerRva(const char *childName) {
    CallerInfo info{};
    if (!classifyStackCallerInModule(Module::SofExe, info)) return 0;
    #if !defined(NDEBUG) && defined(SOFBUDDY_ENABLE_CALLSITE_LOGGER)
    ParentRecorder::Instance().record(childName, info);
    #endif
    return (uint32_t)info.rva;
}

}
