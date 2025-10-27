/*
 * ParentRecorder tracks and records which functions (parents) call which
 * hooked functions (children), storing this information in JSON files.
 * For each child function, it records all unique parent call sites along
 * with the module they belong to (SoF.exe, ref_gl.dll, player.dll, gamex86.dll)
 * and their relative virtual addresses (RVAs). The recorder writes this data
 * to disk for analysis and debugging purposes.
 * We should only use this feature in debug mode, and the files should not be provided
 * with the release zip.
 */
#include "parent_recorder.h"
#include <stdio.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <shlwapi.h>
#include <direct.h>
#endif
#include "util.h"

ParentRecorder &ParentRecorder::Instance() {
    static ParentRecorder inst;
    return inst;
}

ParentRecorder::ParentRecorder() {}

static void derive_dir_alongside_dll(std::string &outAbs, const char *rel) {
#if defined(_WIN32)
    HMODULE self = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&derive_dir_alongside_dll),
        &self);
    if (self) {
        char modPath[MAX_PATH] = {0};
        if (GetModuleFileNameA(self, modPath, MAX_PATH)) {
            PathRemoveFileSpecA(modPath);
            char buf[MAX_PATH];
            snprintf(buf, sizeof(buf), "%s/%s", modPath, rel);
            outAbs.assign(buf);
            return;
        }
    }
#endif
    outAbs.assign(rel);
}

void ParentRecorder::initialize(const char *parentsDir) {
    if (parentsDir && *parentsDir) {
        std::string abs;
#if defined(_WIN32)
        if (PathIsRelativeA(parentsDir)) {
            derive_dir_alongside_dll(abs, parentsDir);
        } else {
            abs.assign(parentsDir);
        }
#else
        abs.assign(parentsDir);
#endif
        _parentsDir.swap(abs);
    } else {
        derive_dir_alongside_dll(_parentsDir, "sof_buddy/func_parents");
    }
    PrintOut(PRINT_LOG, "ParentRecorder: parentsDir=%s\n", _parentsDir.c_str());
    ensureParentsDir();
}

void ParentRecorder::ensureParentsDir() {
#if defined(_WIN32)
    // naive: rely on mkdir via CRT
#endif
    struct stat st;
    if (stat(_parentsDir.c_str(), &st) != 0) {
#if defined(_WIN32)
        _mkdir(_parentsDir.c_str());
#else
        mkdir(_parentsDir.c_str(), 0755);
#endif
    }
}

const char *ParentRecorder::moduleToLeaf(Module m) {
    switch (m) {
        case Module::SofExe: return "SoF.exe";
        case Module::RefDll: return "ref_gl.dll";
        case Module::PlayerDll: return "player.dll";
        case Module::GameDll: return "gamex86.dll";
        default: return "unknown";
    }
}

static HMODULE moduleHandleFor(Module m) {
#if defined(_WIN32)
    switch (m) {
        case Module::SofExe:   return GetModuleHandleA("SoF.exe");
        case Module::RefDll:   return GetModuleHandleA("ref_gl.dll");
        case Module::PlayerDll:return GetModuleHandleA("player.dll");
        case Module::GameDll:  return GetModuleHandleA("gamex86.dll");
        default: return nullptr;
    }
#else
    (void)m; return nullptr;
#endif
}

void ParentRecorder::flushChildToDisk(const std::string &childName, const ChildSet &set) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.json", _parentsDir.c_str(), childName.c_str());
    FILE *f = fopen(path, "wb");
    if (!f) {
        PrintOut(PRINT_BAD, "ParentRecorder: failed to open %s for write\n", path);
        return;
    }
    fprintf(f, "{\n  \"child\": \"%s\",\n  \"parents\": [\n", childName.c_str());
    bool first = true;
    for (const auto &p : set.parents) {
        if (!first) fprintf(f, ",\n");
        first = false;
        uintptr_t base = 0;
        #if defined(_WIN32)
        if (HMODULE h = moduleHandleFor(p.module)) base = reinterpret_cast<uintptr_t>(h);
        #endif
        uintptr_t absAddr = base + static_cast<uintptr_t>(p.fnStartRva);
        fprintf(f,
            "    { \"module\": \"%s\", \"fnStart\": %u, \"fnStartHex\": \"0x%08X\", \"moduleBaseHex\": \"0x%p\", \"absStartHex\": \"0x%p\" }",
            moduleToLeaf(p.module), (unsigned)p.fnStartRva, (unsigned)p.fnStartRva,
            (void*)base, (void*)absAddr);
    }
    fprintf(f, "\n  ]\n}\n");
    fclose(f);
    PrintOut(PRINT_LOG, "ParentRecorder: wrote %zu parents to %s\n", set.parents.size(), path);
}

void ParentRecorder::record(const char *childName, const CallerInfo &info) {
    #ifndef NDEBUG
    if (!childName || !*childName) return;
    if (info.functionStartRva == 0) return; // unknown -> skip
    ChildSet &set = _childToParents[childName];
    ParentKey key{ info.module, info.functionStartRva };
    auto it = set.parents.find(key);
    if (it == set.parents.end()) {
        set.parents.insert(key);
        set.dirty = true;
        flushChildToDisk(childName, set);
    }
    #else
    (void)childName; (void)info; // no-op in release builds
    #endif
}


