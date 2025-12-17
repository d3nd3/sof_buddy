/**
 * CallsiteClassifier converts return addresses into function start addresses.
 * 
 * Funcmaps (JSON files in rsrc/funcmaps/) contain function start RVAs for SoF modules.
 * When a hook needs to identify its caller's function, it can:
 *   1. Capture the return address (RVA within the calling module)
 *   2. Look it up in the funcmap to find the containing function's start RVA
 *   3. Optionally match against function names or RVAs for conditional behavior
 * 
 * This enables features to adapt based on which function calls them, enabling
 * context-aware hooking (e.g., scaling UI elements differently for menus vs HUD).
 * Required at runtime.
 */

#include "debug/callsite_classifier.h"
#include <string.h>
#include <vector>
#include <string>
#include <stdio.h>
#include <algorithm>
#include <unordered_map>
#include <shlwapi.h>
#include <psapi.h>
#include "util.h"

namespace {

struct Func { uint32_t rva; std::string name; };

struct ModuleMap { std::vector<Func> functions; bool loaded = false; };

ModuleMap g_maps[6]; // Unknown,SofExe,RefDll,PlayerDll,GameDll,SpclDll
static char g_mapsDir[512] = {0};

struct ModuleRange { uintptr_t base; uintptr_t end; HMODULE h; bool valid; };
static ModuleRange g_modRanges[6] = {0};
static std::unordered_map<uintptr_t, CallerInfo> g_cache;

static inline void cacheRangeFromHandle(Module m, HMODULE h) {
    if (!h) return;
    MODULEINFO mi = {0};
    if (!GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi))) return;
    ModuleRange &mr = g_modRanges[(int)m];
    mr.base = (uintptr_t)mi.lpBaseOfDll;
    mr.end  = mr.base + (uintptr_t)mi.SizeOfImage;
    mr.valid = (mr.base != 0 && mr.end > mr.base);
    mr.h = h;
}

static inline bool addrInRange(uintptr_t addr, const ModuleRange &mr) {
    return mr.valid && addr >= mr.base && addr < mr.end;
}

inline Module identifyModuleFromHandle(HMODULE hmod) {
    if (!hmod) {
        PrintOut(PRINT_LOG, "identifyModuleFromHandle: invalid hmod=0x%p\n", hmod);
        return Module::Unknown;
    }

    char fullPath[MAX_PATH] = {0};
    if (!GetModuleFileNameA(hmod, fullPath, MAX_PATH)) {
        PrintOut(PRINT_LOG, "identifyModuleFromHandle: GetModuleFileNameA failed for hmod=0x%p error=%lu\n", hmod, GetLastError());
        return Module::Unknown;
    }
    const char *leaf = PathFindFileNameA(fullPath);
    if (!leaf || !*leaf) leaf = fullPath;
    

    // Strict filename match, case-insensitive
    if (StrCmpIA(leaf, "SoF.exe") == 0) return Module::SofExe;
    if (StrCmpIA(leaf, "ref_gl.dll") == 0) return Module::RefDll;
    if (StrCmpIA(leaf, "player.dll") == 0) return Module::PlayerDll;
    if (StrCmpIA(leaf, "gamex86.dll") == 0) return Module::GameDll;
    if (StrCmpIA(leaf, "spcl.dll") == 0) return Module::SpclDll;
    // Some environments may rename or change casing; keep relaxed substring checks as a fallback
    if (StrStrIA(fullPath, "SoF.exe")) return Module::SofExe;
    if (StrStrIA(fullPath, "ref_gl.dll")) return Module::RefDll;
    if (StrStrIA(fullPath, "player.dll")) return Module::PlayerDll;
    if (StrStrIA(fullPath, "gamex86.dll")) return Module::GameDll;
    if (StrStrIA(fullPath, "spcl.dll")) return Module::SpclDll;
    return Module::Unknown;
}

static inline bool parseUintAfterKey(const char *line, const char *key, uint32_t &out) {
    const char *p = strstr(line, key);
    if (!p) return false;
    p += strlen(key);
    // Skip spaces
    while (*p == ' ' || *p == '\t') ++p;
    // Read number (decimal)
    unsigned int val = 0;
    if (sscanf(p, "%u", &val) == 1) { out = val; return true; }
    // Try hex like 0x...
    if (sscanf(p, "0x%x", &val) == 1) { out = val; return true; }
    return false;
}

static inline bool parseStringAfterKey(const char *line, const char *key, std::string &out) {
    const char *p = strstr(line, key);
    if (!p) return false;
    p += strlen(key);
    while (*p && *p != '"') ++p;
    if (*p != '"') return false;
    ++p;
    std::string s;
    while (*p && *p != '"') { s.push_back(*p++); }
    if (*p != '"') return false;
    out = s;
    return true;
}

static inline const char* moduleJsonLeaf(Module m) {
    switch (m) {
        case Module::SofExe: return "SoF.exe.json";
        case Module::RefDll: return "ref_gl.dll.json";
        case Module::PlayerDll: return "player.dll.json";
        case Module::GameDll: return "gamex86.dll.json";
        case Module::SpclDll: return "spcl.dll.json";
        default: return nullptr;
    }
}

static inline const char* moduleLeaf(Module m) {
    switch (m) {
        case Module::SofExe:   return "SoF.exe";
        case Module::RefDll:   return "ref_gl.dll";
        case Module::PlayerDll:return "player.dll";
        case Module::GameDll:  return "gamex86.dll";
        case Module::SpclDll:  return "spcl.dll";
        default: return nullptr;
    }
}

static inline void ensureLoadedFor(Module m, const char *dir) {
    int idx = static_cast<int>(m);
    if (idx < 0 || idx >= 6) {
        PrintOut(PRINT_BAD, "CallsiteClassifier: ERROR - Invalid module index %d\n", idx);
        ExitProcess(1);
    }
    ModuleMap &mm = g_maps[idx];
    if (mm.loaded) return;
    const char *leaf = moduleJsonLeaf(m);
    if (!leaf) return;
    char path[1024];
    const char *baseDir = nullptr;
    if (dir && *dir) {
        baseDir = dir;
    } else if (g_mapsDir[0]) {
        baseDir = g_mapsDir;
    } else {
        // Derive alongside our DLL: <dll_folder>/sof_buddy/funcmaps
        static char derivedDir[512];
        HMODULE self = nullptr;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&ensureLoadedFor),
            &self);
        if (self) {
            char modPath[MAX_PATH] = {0};
            if (GetModuleFileNameA(self, modPath, MAX_PATH)) {
                PathRemoveFileSpecA(modPath);
                snprintf(derivedDir, sizeof(derivedDir), "%s/%s", modPath, "sof_buddy/funcmaps");
                baseDir = derivedDir;
            }
        }
        if (!baseDir) baseDir = "sof_buddy/funcmaps"; // last resort
    }
    snprintf(path, sizeof(path), "%s/%s", baseDir, leaf);
    FILE *f = fopen(path, "rb");
    if (!f) {
        char legacyDir[512] = {0};
        if (g_mapsDir[0] == '\0') {
            HMODULE self = nullptr;
            GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&ensureLoadedFor),
                &self);
            if (self) {
                char modPath[MAX_PATH] = {0};
                if (GetModuleFileNameA(self, modPath, MAX_PATH)) {
                    PathRemoveFileSpecA(modPath);
                    snprintf(legacyDir, sizeof(legacyDir), "%s/%s", modPath, "rsrc/funcmaps");
                }
            }
        }
        const char *fallbackDir = legacyDir[0] ? legacyDir : "rsrc/funcmaps";
        snprintf(path, sizeof(path), "%s/%s", fallbackDir, leaf);
        f = fopen(path, "rb");
        if (!f) return;
    }
    char line[2048];
    uint32_t pendingRva = 0; bool haveRva = false; std::string pendingName;
    while (fgets(line, sizeof(line), f)) {
        uint32_t val;
        if (parseUintAfterKey(line, "\"rva\":", val)) { pendingRva = val; haveRva = true; }
        std::string nm;
        if (parseStringAfterKey(line, "\"name\":", nm)) { pendingName = nm; }
        // End of object '}' can occur on the same line as rva/name (e.g., { "rva": 4096 },)
        if (strchr(line, '}')) {
            if (haveRva) {
                mm.functions.push_back(Func{ pendingRva, pendingName });
            }
            haveRva = false; pendingRva = 0; pendingName.clear();
        }
    }
    fclose(f);
    std::sort(mm.functions.begin(), mm.functions.end(), [](const Func&a, const Func&b){ return a.rva < b.rva; });
    mm.loaded = true;
}

}

void CallsiteClassifier::initialize(const char *mapsDirectory) {
	// Resolve default maps directory next to our module if not provided
	if (mapsDirectory && *mapsDirectory) {
		HMODULE self = nullptr;
		GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCSTR>(&CallsiteClassifier::initialize),
			&self);
		char baseDir[MAX_PATH] = {0};
		if (self && GetModuleFileNameA(self, baseDir, MAX_PATH)) {
			PathRemoveFileSpecA(baseDir);
			if (PathIsRelativeA(mapsDirectory)) {
				snprintf(g_mapsDir, sizeof(g_mapsDir), "%s/%s", baseDir, mapsDirectory);
			} else {
				strncpy(g_mapsDir, mapsDirectory, sizeof(g_mapsDir)-1);
				g_mapsDir[sizeof(g_mapsDir)-1] = '\0';
			}
		} else {
			strncpy(g_mapsDir, mapsDirectory, sizeof(g_mapsDir)-1);
			g_mapsDir[sizeof(g_mapsDir)-1] = '\0';
		}
    } else {
        HMODULE self = nullptr;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&CallsiteClassifier::initialize),
            &self);
        if (self) {
            char modPath[MAX_PATH] = {0};
            if (GetModuleFileNameA(self, modPath, MAX_PATH)) {
                // Trim filename to directory
                PathRemoveFileSpecA(modPath);
                // Append sof_buddy/funcmaps
                snprintf(g_mapsDir, sizeof(g_mapsDir), "%s/%s", modPath, "sof_buddy/funcmaps");
            }
        }
    }
	ensureLoadedFor(Module::SofExe, g_mapsDir);
	ensureLoadedFor(Module::RefDll, g_mapsDir);
	ensureLoadedFor(Module::PlayerDll, g_mapsDir);
	ensureLoadedFor(Module::GameDll, g_mapsDir);
	ensureLoadedFor(Module::SpclDll, g_mapsDir);
}

bool CallsiteClassifier::classify(void *returnAddress, CallerInfo &out) {
	uintptr_t ra = (uintptr_t)returnAddress;

	auto it = g_cache.find(ra);
	if (it != g_cache.end()) { out = it->second; return true; }

	Module foundModule = Module::Unknown;
	HMODULE mod = nullptr;
	
	static const int moduleOrder[] = {(int)Module::SofExe,(int)Module::RefDll, (int)Module::SpclDll,(int)Module::GameDll, (int)Module::PlayerDll};
	for (int i = 0; i < 5; ++i) {
		int m = moduleOrder[i];
		ModuleRange &mr = g_modRanges[m];
		if (mr.valid && ra >= mr.base && ra < mr.end) {
			foundModule = (Module)m;
			mod = mr.h;
			if (!mod) {
				if (GetModuleHandleExA(
					GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
					GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					reinterpret_cast<LPCSTR>(returnAddress),
					&mod)) {
					mr.h = mod;
				}
			}
			break;
		}
	}

	if (foundModule == Module::Unknown) {
		if (!GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCSTR>(returnAddress),
			&mod)) {
			return false;
		}
		for (int i = 0; i < 5; ++i) {
			int m = moduleOrder[i];
			if (g_modRanges[m].h == mod) {
				foundModule = (Module)m;
				break;
			}
		}
	}

	if (foundModule == Module::Unknown || !mod) {
		#ifndef NDEBUG
		PrintOut(PRINT_LOG, "CallsiteClassifier: Warning - Unable to classify returnAddress=0x%p (not in tracked modules)\n", returnAddress);
		#endif
		return false;
	}
	
	int modIdx = (int)foundModule;
	out.module = foundModule;
	uintptr_t base = g_modRanges[modIdx].valid ? g_modRanges[modIdx].base : (uintptr_t)mod;
	uintptr_t rva  = ra - base;


	out.rva = rva;
	out.functionStartRva = 0;
	out.name = nullptr;
	const auto &funcs = g_maps[modIdx].functions;
	if (!funcs.empty()) {
		int lo = 0, hi = (int)funcs.size() - 1, best = -1;
		uint32_t key = (rva > 0) ? (uint32_t)(rva - 1) : 0;
		while (lo <= hi) {
			int mid = (lo + hi) >> 1;
			if (funcs[mid].rva <= key) { 
				best = mid; 
				lo = mid + 1; 
			} else { 
				hi = mid - 1; 
			}
		}
		if (best >= 0) {
			out.functionStartRva = funcs[best].rva;
			if (!funcs[best].name.empty()) out.name = funcs[best].name.c_str();
		}
	}
	g_cache[ra] = out;
	return true;
}

inline bool CallsiteClassifier::matchesNameOrRva(const CallerInfo &info, const char *name, uintptr_t rva) {
	if (name && info.name && strcmp(name, info.name) == 0) return true;
	if (rva != 0 && info.rva == rva) return true;
	return false;
}


bool CallsiteClassifier::hasFunctionStart(Module m, uint32_t fnStartRva) {
    if (m == Module::Unknown || fnStartRva == 0) return false;
    int idx = static_cast<int>(m);
    const auto &funcs = g_maps[idx].functions;
    if (funcs.empty()) return false;
    int lo = 0, hi = (int)funcs.size() - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        uint32_t v = funcs[mid].rva;
        if (v == fnStartRva) return true;
        if (v < fnStartRva) lo = mid + 1; else hi = mid - 1;
    }
    return false;
}

uintptr_t CallsiteClassifier::getModuleBase_impl(Module m) {
    int idx = (int)m;
    if (idx <= 0 || idx >= (int)Module::SpclDll + 1) return 0;
    if (g_modRanges[idx].valid) return g_modRanges[idx].base;
#if defined(_WIN32)
    const char *leaf = moduleLeaf(m);
    if (!leaf) return 0;
    if (HMODULE h = GetModuleHandleA(leaf)) {
        cacheRangeFromHandle(m, h);
        if (g_modRanges[idx].valid) return g_modRanges[idx].base;
    }
#endif
    return 0;
}

void CallsiteClassifier::cacheModuleLoaded(Module m) {
    if (m == Module::Unknown) return;
    int idx = (int)m;
    if (idx <= 0 || idx >= (int)Module::SpclDll + 1) return;
    const char *leaf = moduleLeaf(m);
    if (!leaf) return;
    HMODULE h = GetModuleHandleA(leaf);
    if (h) {
        cacheRangeFromHandle(m, h);
        g_modRanges[idx].h = h;
    }
}

void CallsiteClassifier::invalidateModuleCache(Module m) {
    if (m == Module::Unknown) return;
    int idx = (int)m;
    if (idx <= 0 || idx >= (int)Module::SpclDll + 1) return;
    uintptr_t oldBase = g_modRanges[idx].base;
    uintptr_t oldEnd = g_modRanges[idx].end;
    g_modRanges[idx].valid = false;
    g_modRanges[idx].h = nullptr;
    g_modRanges[idx].base = 0;
    g_modRanges[idx].end = 0;
    if (oldBase != 0 && oldEnd > oldBase) {
        for (auto it = g_cache.begin(); it != g_cache.end();) {
            if (it->first >= oldBase && it->first < oldEnd) {
                it = g_cache.erase(it);
            } else {
                ++it;
            }
        }
    }
}


