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
#include "callsite_classifier.h"

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
    loadExistingData();
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
        case Module::SpclDll: return "spcl.dll";
        default: return "unknown";
    }
}

Module ParentRecorder::moduleFromString(const char *str) {
    if (!str) return Module::SofExe;
    if (strcmp(str, "SoF.exe") == 0) return Module::SofExe;
    if (strcmp(str, "ref_gl.dll") == 0) return Module::RefDll;
    if (strcmp(str, "player.dll") == 0) return Module::PlayerDll;
    if (strcmp(str, "gamex86.dll") == 0) return Module::GameDll;
    if (strcmp(str, "spcl.dll") == 0) return Module::SpclDll;
    return Module::SofExe;
}

static HMODULE moduleHandleFor(Module m) {
#if defined(_WIN32)
    switch (m) {
        case Module::SofExe:   return GetModuleHandleA("SoF.exe");
        case Module::RefDll:   return GetModuleHandleA("ref_gl.dll");
        case Module::PlayerDll:return GetModuleHandleA("player.dll");
        case Module::GameDll:  return GetModuleHandleA("gamex86.dll");
        case Module::SpclDll:  return GetModuleHandleA("spcl.dll");
        default: return nullptr;
    }
#else
    (void)m; return nullptr;
#endif
}

void ParentRecorder::loadExistingData() {
    #ifndef NDEBUG
    PrintOut(PRINT_LOG, "ParentRecorder: Loading existing data from %s\n", _parentsDir.c_str());
    
#if defined(_WIN32)
    char searchPath[1024];
    snprintf(searchPath, sizeof(searchPath), "%s/*.json", _parentsDir.c_str());
    
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        PrintOut(PRINT_LOG, "ParentRecorder: No existing JSON files found\n");
        return;
    }
    
    int filesLoaded = 0;
    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", _parentsDir.c_str(), findData.cFileName);
        
        FILE *f = fopen(fullPath, "rb");
        if (!f) continue;
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        if (size <= 0 || size > 1024*1024) {
            fclose(f);
            continue;
        }
        
        char *buffer = new char[size + 1];
        fread(buffer, 1, size, f);
        buffer[size] = '\0';
        fclose(f);
        
        std::string childName = findData.cFileName;
        if (childName.size() > 5 && childName.substr(childName.size() - 5) == ".json") {
            childName = childName.substr(0, childName.size() - 5);
        }
        
        ChildSet &set = _childToParents[childName];
        
        char *ptr = strstr(buffer, "\"parents\":");
        if (ptr) {
            ptr = strchr(ptr, '[');
            if (ptr) {
                ptr++;
                while (*ptr) {
                    char *moduleStart = strstr(ptr, "\"module\":");
                    char *fnStartStr = strstr(ptr, "\"fnStart\":");
                    if (!moduleStart || !fnStartStr) break;

                    // Parse module value string
                    char moduleName[64] = {0};
                    {
                        char *colon = strchr(moduleStart, ':');
                        if (colon) {
                            char *valOpen = strchr(colon, '"');
                            if (valOpen) {
                                valOpen++;
                                char *valClose = strchr(valOpen, '"');
                                if (valClose && valClose > valOpen) {
                                    size_t len = (size_t)(valClose - valOpen);
                                    if (len < sizeof(moduleName)) {
                                        memcpy(moduleName, valOpen, len);
                                        moduleName[len] = '\0';
                                    }
                                }
                            }
                        }
                    }

                    // Parse fnStart number (decimal)
                    uint32_t fnStart = 0;
                    {
                        char *num = strchr(fnStartStr, ':');
                        if (num) {
                            num++;
                            sscanf(num, "%u", &fnStart);
                        }
                    }

                    if (moduleName[0] != '\0' && fnStart > 0) {
                        Module mod = moduleFromString(moduleName);
                        ParentKey key{ mod, fnStart };
                        set.parents.insert(key);
                    }

                    ptr = strchr(ptr, '}');
                    if (!ptr) break;
                    ptr++;
                    ptr = strchr(ptr, '{');
                    if (!ptr) break;
                }
            }
        }
        
        delete[] buffer;
        filesLoaded++;
        PrintOut(PRINT_LOG, "ParentRecorder: Loaded %zu parents for '%s'\n", 
                 set.parents.size(), childName.c_str());
        
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
    PrintOut(PRINT_LOG, "ParentRecorder: Loaded %d existing JSON files\n", filesLoaded);
#else
    PrintOut(PRINT_LOG, "ParentRecorder: loadExistingData not implemented for non-Windows\n");
#endif
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
        uintptr_t base = CallsiteClassifier::getModuleBase(p.module);
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
    // Guard: only record if (module, fnStart) exists in funcmap to avoid misclassification
    if (!CallsiteClassifier::hasFunctionStart(info.module, (uint32_t)info.functionStartRva)) {
        PrintOut(PRINT_LOG, "ParentRecorder: skip invalid parent (module=%d fnStart=0x%08X) for '%s'\n", (int)info.module, (unsigned)info.functionStartRva, childName);
        return;
    }
    ChildSet &set = _childToParents[childName];
    ParentKey key{ info.module, info.functionStartRva };
    auto it = set.parents.find(key);
    if (it == set.parents.end()) {
        set.parents.insert(key);
        set.dirty = true;
    }
    #else
    (void)childName; (void)info; // no-op in release builds
    #endif
}

void ParentRecorder::flushAll() {
    #ifndef NDEBUG
    for (auto &kv : _childToParents) {
        const std::string &child = kv.first;
        ChildSet &set = kv.second;
        if (set.dirty) {
            flushChildToDisk(child, set);
            set.dirty = false;
        }
    }
    #endif
}


