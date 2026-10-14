# Debug Module

The debug module provides runtime caller identification and classification for hook functions. It enables features to adapt their behavior based on which function called them, enabling context-aware hooking.

## Debug vs Release Behavior

**Always Active (Release & Debug):**
- Caller classification and identification
- Function RVA lookups
- Module detection and caching
- All `CallsiteClassifier` functions
- All `HookCallsite` classification functions (they always return RVAs)

**Debug-Only (requires `!defined(NDEBUG)`):**
- Warning messages when classification fails
- Debug logging output

**Debug-Only (requires `!defined(NDEBUG) && defined(SOFBUDDY_ENABLE_CALLSITE_LOGGER)`):**
- Parent-child relationship recording
- `ParentRecorder` file I/O operations
- The recording portion of `recordAndGetFnStart()` and `recordAndGetFnStartExternal()`

**Note:** Even in release builds, `recordAndGetFnStart()` and `recordAndGetFnStartExternal()` still return function start RVAs - they just don't record the parent-child relationships.

## Overview

When a hook function is called, it needs to know:
- **Which module** called it (SoF.exe, ref_gl.dll, player.dll, gamex86.dll, spcl.dll)
- **Which function** within that module made the call
- **The function's RVA** (Relative Virtual Address) for matching

This module provides three components that work together to answer these questions:

1. **CallsiteClassifier** - Maps return addresses to function information (always active)
2. **HookCallsite** - Convenient wrappers for hook functions (always active)
3. **ParentRecorder** - Records parent-child call relationships (debug builds only)

## Components

### CallsiteClassifier

**Purpose:** Converts return addresses into structured caller information.

**Status:** ✅ **Always Active** (required for runtime caller identification)

**Key Functions:**
- `initialize(mapsDirectory)` - ✅ Always active - Loads function maps from JSON files
- `classify(returnAddress, out)` - ✅ Always active - Classifies a return address into `CallerInfo`
  - ⚠️ Warning messages only printed in debug builds (`!defined(NDEBUG)`)
- `hasFunctionStart(module, rva)` - ✅ Always active - Verifies a function RVA exists in loaded maps
- `getModuleBase(module)` - ✅ Always active - Gets the base address of a tracked module
- `cacheModuleLoaded(module)` - ✅ Always active - Caches module info when DLL loads
- `invalidateModuleCache(module)` - ✅ Always active - Clears cache when DLL unloads

**Flow:**
```
Return Address → Module Detection → RVA Calculation → Function Lookup → CallerInfo
```

**Data Structures:**
- `CallerInfo` - Contains module, RVA, function start RVA, and optional function name
- `Module` - Enum identifying SoF modules (SofExe, RefDll, PlayerDll, GameDll, SpclDll)

**Function Maps:**
- Loaded from `sof_buddy/funcmaps/*.json` files
- Each JSON contains function RVAs and names for a module
- Used to map return addresses to function start addresses

### HookCallsite

**Purpose:** Provides convenient functions for hook implementations to identify their callers.

**Status:** ✅ **Always Active** (classification always works, recording is debug-only)

**Key Functions:**
- `classifyCaller(out)` - ✅ Always active - Gets immediate caller (one frame up)
- `classifyExternalCaller(out)` - ✅ Always active - Walks stack to find first external caller (skips sof_buddy.dll)
- `getCallerFnStart()` - ✅ Always active - Returns immediate caller's function start RVA
- `GetFnStartExternal()` - ✅ Always active - Returns external caller's function start RVA
- `recordAndGetFnStart(childName)` - ✅ Always returns RVA, ⚠️ Recording is debug-only
  - Returns function start RVA in all builds
  - Only calls `ParentRecorder::record()` when `!defined(NDEBUG) && defined(SOFBUDDY_ENABLE_CALLSITE_LOGGER)`
- `recordAndGetFnStartExternal(childName)` - ✅ Always returns RVA, ⚠️ Recording is debug-only
  - Returns function start RVA in all builds
  - Only calls `ParentRecorder::record()` when `!defined(NDEBUG) && defined(SOFBUDDY_ENABLE_CALLSITE_LOGGER)`

**Usage Pattern:**
```cpp
uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_StretchPic");
if (fnStart) {
    // Adapt behavior based on caller
    detectedCaller = getCallerFromRva(fnStart);
}
```

**Why External?**
When sof_buddy.dll functions call each other internally, `classifyExternalCaller` walks up the stack to find the first caller outside sof_buddy.dll. This identifies the "real" game function that triggered the call chain.

### ParentRecorder

**Purpose:** Records which functions (parents) call which hooks (children) for analysis.

**Status:** ⚠️ **Debug-Only** (requires `!defined(NDEBUG) && defined(SOFBUDDY_ENABLE_CALLSITE_LOGGER)`)

**Key Functions:**
- `initialize(parentsDir)` - ⚠️ Debug-only - Sets output directory and loads existing data
  - Only loads existing data when `SOFBUDDY_ENABLE_CALLSITE_LOGGER` is defined
- `record(childName, callerInfo)` - ⚠️ Debug-only - Records a parent-child relationship
  - No-op in release builds (compiled out)
- `flushAll()` - ⚠️ Debug-only - Writes all recorded data to disk
  - No-op in release builds (compiled out)

**Output Format:**
- JSON files in `sof_buddy/func_parents/`
- One file per child hook name
- Contains list of unique parent functions (module + function start RVA)

**Important Notes:**
- Entire `ParentRecorder` functionality is compiled out in release builds
- Used for understanding call patterns during development
- Files should not be included in release builds
- Does not affect runtime performance in release builds

## Flow

### Initialization (Runtime)

```
1. lifecycle_EarlyStartup()
   ↓
2. ✅ CallsiteClassifier::initialize("sof_buddy/funcmaps")
   - Always active - Loads function maps from JSON files
   - Always active - Caches module base addresses
   ↓
3. ✅ HookCallsite::cacheSelfModule()
   - Always active - Caches sof_buddy.dll module info
   - Always active - Used to filter out internal calls
   ↓
4. ⚠️ ParentRecorder::initialize("sof_buddy/func_parents")
   - Debug-only - Sets up output directory
   - Debug-only - Loads existing parent data
   - No-op in release builds (compiled out)
```

### Hook Execution (Runtime)

```
1. Game calls hooked function (e.g., Draw_StretchPic)
   ↓
2. ✅ Hook function calls HookCallsite::recordAndGetFnStartExternal("Draw_StretchPic")
   - Always active - Function always executes
   ↓
3. ✅ HookCallsite uses __builtin_return_address(0) to get return address
   - Always active - Stack inspection always happens
   ↓
4. ✅ HookCallsite walks stack (if needed) to find external caller
   - Always active - Stack walking always happens for external caller detection
   ↓
5. ✅ CallsiteClassifier::classify() maps return address to CallerInfo
   - Always active - Identifies module from address range
   - Always active - Calculates RVA (return address - module base)
   - Always active - Looks up function start RVA in loaded maps
   - ⚠️ Warning messages only printed in debug builds
   ↓
6. ⚠️ ParentRecorder::record() stores parent-child relationship
   - Debug-only - Only executes when SOFBUDDY_ENABLE_CALLSITE_LOGGER is defined
   - No-op in release builds (compiled out)
   ↓
7. ✅ Function start RVA returned to hook
   - Always active - RVA is always returned (even if recording was skipped)
   ↓
8. ✅ Hook adapts behavior based on caller
   - Always active - Hook behavior adaptation works in all builds
```

### Module Loading (Runtime)

```
1. DLL loads (e.g., ref_gl.dll)
   ↓
2. ✅ CallsiteClassifier::cacheModuleLoaded(Module::RefDll)
   - Always active - Gets module handle
   - Always active - Caches base address and size
   - Always active - Updates module range for classification
```

## Usage Examples

### Basic Caller Identification

```cpp
#include "debug/hook_callsite.h"

void hkDraw_Pic(...) {
    uint32_t fnStart = HookCallsite::getCallerFnStart();
    if (fnStart == 0x1234) {
        // Called from specific function
    }
}
```

### External Caller with Recording

```cpp
#include "debug/hook_callsite.h"

void hkDraw_StretchPic(...) {
    uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_StretchPic");
    if (fnStart) {
        StretchPicCaller caller = getCallerFromRva(fnStart);
        // Adapt behavior based on caller
    }
}
```

### Full Caller Information

```cpp
#include "debug/hook_callsite.h"

void hkSomeFunction(...) {
    CallerInfo info{};
    if (HookCallsite::classifyExternalCaller(info)) {
        // info.module - which module called
        // info.rva - return address RVA
        // info.functionStartRva - function start RVA
        // info.name - function name (if available)
    }
}
```

### Module Verification

```cpp
#include "debug/callsite_classifier.h"

bool isValidCaller(Module m, uint32_t rva) {
    return CallsiteClassifier::hasFunctionStart(m, rva);
}
```

## Dependencies

- **Function Maps:** Requires JSON files in `sof_buddy/funcmaps/` with function RVAs
- **Module Handles:** Requires modules to be loaded and cached
- **Stack Walking:** Uses `__builtin_return_address()` and inline assembly (Windows)

## Error Handling

- ✅ `classify()` returns `false` if address cannot be classified (always active, not an error, just not tracked)
- ⚠️ Debug warnings printed when classification fails (debug builds only - `!defined(NDEBUG)`)
- ✅ Unknown modules are gracefully handled in all builds (returns `false`, no crash)

## Notes

- Function maps must be generated separately (not part of this module)
- ✅ Caller classification works in all builds (release and debug)
- ⚠️ Parent recording is debug-only and should not be enabled in release builds
- ✅ Stack walking is always active (Windows-specific; other platforms fall back to immediate caller)
- ✅ Module caching is always active and critical for performance; invalidate when DLLs unload
- ⚠️ `recordAndGetFnStart()` and `recordAndGetFnStartExternal()` always return RVAs, but only record parent-child relationships in debug builds with `SOFBUDDY_ENABLE_CALLSITE_LOGGER` defined

