#pragma once

#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
#endif

enum class Module {
	Unknown = 0,
	SofExe,
	RefDll,
	PlayerDll,
	GameDll,
	SpclDll,
};

struct CallerInfo {
	Module module;
	uintptr_t rva; // Return address offset within module
	uintptr_t functionStartRva; // Best-known function start containing rva (0 if unknown)
	const char *name; // Optional function name (export or synthesized); may be nullptr
};

namespace CallsiteClassifier {

// Initialize maps; 'mapsDirectory' is optional. If null, defaults to "rsrc/funcmaps".
void initialize(const char *mapsDirectory = nullptr);

// Classify a return address. Returns false on failure (e.g., module not resolved).
bool classify(void *returnAddress, CallerInfo &out);

// Utility predicate: match by name or fallback RVA.
bool matchesNameOrRva(const CallerInfo &info, const char *name, uintptr_t rva);

// Verify that a function start RVA exists in the loaded map for a module
bool hasFunctionStart(Module m, uint32_t fnStartRva);

// Internal: non-inline impl used by the inline wrapper below
uintptr_t getModuleBase_impl(Module m);

// Inline accessor with lightweight fallback via impl (kept inline for call sites)
inline uintptr_t getModuleBase(Module m) { return getModuleBase_impl(m); }

// Cache module handle and range when DLL loads (called from module_loaders.cpp)
void cacheModuleLoaded(Module m);

// Invalidate module cache when DLL unloads (called from module_loaders.cpp)
void invalidateModuleCache(Module m);

}


