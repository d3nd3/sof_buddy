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

#include "callsite_classifier.h"
#include <string.h>
#include <vector>
#include <string>
#include <stdio.h>
#include <algorithm>
#include <shlwapi.h>
#include "util.h"

namespace {

struct Func { uint32_t rva; std::string name; };

struct ModuleMap { std::vector<Func> functions; bool loaded = false; };

ModuleMap g_maps[5]; // Unknown,SofExe,RefDll,PlayerDll,GameDll
static char g_mapsDir[512] = {0};

Module identifyModuleFromHandle(HMODULE hmod) {
    if (!hmod) return Module::Unknown;

    char fullPath[MAX_PATH] = {0};
    GetModuleFileNameA(hmod, fullPath, MAX_PATH);
    const char *leaf = PathFindFileNameA(fullPath);
    if (!leaf || !*leaf) leaf = fullPath; // fallback to full path if needed
    // Strict filename match, case-insensitive
    if (StrCmpIA(leaf, "SoF.exe") == 0) return Module::SofExe;
    if (StrCmpIA(leaf, "ref_gl.dll") == 0) return Module::RefDll;
    if (StrCmpIA(leaf, "player.dll") == 0) return Module::PlayerDll;
    if (StrCmpIA(leaf, "gamex86.dll") == 0) return Module::GameDll;
    // Some environments may rename or change casing; keep relaxed substring checks as a fallback
    if (StrStrIA(fullPath, "SoF.exe")) return Module::SofExe;
    if (StrStrIA(fullPath, "ref_gl.dll")) return Module::RefDll;
    if (StrStrIA(fullPath, "player.dll")) return Module::PlayerDll;
    if (StrStrIA(fullPath, "gamex86.dll")) return Module::GameDll;
    return Module::Unknown;
}

static bool parseUintAfterKey(const char *line, const char *key, uint32_t &out) {
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

static bool parseStringAfterKey(const char *line, const char *key, std::string &out) {
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

static const char* moduleJsonLeaf(Module m) {
    switch (m) {
        case Module::SofExe: return "SoF.exe.json";
        case Module::RefDll: return "ref_gl.dll.json";
        case Module::PlayerDll: return "player.dll.json";
        case Module::GameDll: return "gamex86.dll.json";
        default: return nullptr;
    }
}

static void ensureLoadedFor(Module m, const char *dir) {
    ModuleMap &mm = g_maps[static_cast<int>(m)];
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
    PrintOut(PRINT_LOG, "CallsiteClassifier: ensureLoadedFor m=%d baseDir=%s leaf=%s\n", (int)m, baseDir, leaf);
    snprintf(path, sizeof(path), "%s/%s", baseDir, leaf);
    FILE *f = fopen(path, "rb");
    if (!f) {
        // Try legacy fallback in place: switch baseDir to rsrc/funcmaps next to DLL
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
        if (!f) {
            PrintOut(PRINT_LOG, "CallsiteClassifier: could not open map %s\n", path);
            return;
        }
        PrintOut(PRINT_LOG, "CallsiteClassifier: loaded from legacy path %s\n", path);
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
    PrintOut(PRINT_LOG, "CallsiteClassifier: loaded %u functions from %s\n", (unsigned)mm.functions.size(), path);
    mm.loaded = true; // mark only after successful load
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
	PrintOut(PRINT_LOG, "CallsiteClassifier: mapsDir=%s\n", g_mapsDir[0] ? g_mapsDir : "<none>");
	// Preload all known modules
	ensureLoadedFor(Module::SofExe, g_mapsDir);
	ensureLoadedFor(Module::RefDll, g_mapsDir);
	ensureLoadedFor(Module::PlayerDll, g_mapsDir);
	ensureLoadedFor(Module::GameDll, g_mapsDir);
}

bool CallsiteClassifier::classify(void *returnAddress, CallerInfo &out) {
	if (!returnAddress) return false;

	HMODULE mod = nullptr;
	if (!GetModuleHandleExA(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCSTR>(returnAddress),
		&mod)) {
		return false;
	}

	uintptr_t base = reinterpret_cast<uintptr_t>(mod);
	uintptr_t rva  = reinterpret_cast<uintptr_t>(returnAddress) - base;

    out.module = identifyModuleFromHandle(mod);
    char modPath[MAX_PATH] = {0};
    GetModuleFileNameA(mod, modPath, MAX_PATH);
    const char *leaf = PathFindFileNameA(modPath);
    if (!leaf || !*leaf) leaf = modPath;
    // PrintOut(PRINT_LOG, "CallsiteClassifier: classify module=%d file=%s rva=0x%08X base=0x%p ra=0x%p\n", (int)out.module, leaf, (unsigned)rva, (void*)base, returnAddress);
	// Lazy-load this module's map if needed
	ensureLoadedFor(out.module, nullptr);

	out.rva = rva;
	out.functionStartRva = 0;
	out.name = nullptr;
	// Lookup greatest function start <= (rva-1) to ensure strict containment of the return site
	const auto &funcs = g_maps[static_cast<int>(out.module)].functions;
	if (!funcs.empty()) {
		int lo = 0, hi = (int)funcs.size() - 1, best = -1;
		uint32_t key = (rva > 0) ? (uint32_t)(rva - 1) : 0; // use rva-1 for containment
		while (lo <= hi) {
			int mid = (lo + hi) / 2;
			if (funcs[mid].rva <= key) { best = mid; lo = mid + 1; }
			else { hi = mid - 1; }
		}
		if (best >= 0) {
			out.functionStartRva = funcs[best].rva;
			if (!funcs[best].name.empty()) out.name = funcs[best].name.c_str();
		}
	}
	else {
		PrintOut(PRINT_LOG, "CallsiteClassifier: no functions loaded for module=%d (rva=0x%08X)\n", (int)out.module, (unsigned)rva);
	}
	// No fallback here: if not found, functionStartRva remains 0
	return true;
}

bool CallsiteClassifier::matchesNameOrRva(const CallerInfo &info, const char *name, uintptr_t rva) {
	if (name && info.name && strcmp(name, info.name) == 0) return true;
	if (rva != 0 && info.rva == rva) return true;
	return false;
}


