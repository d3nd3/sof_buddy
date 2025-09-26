#ifndef DETOUR_TRACKER_H
#define DETOUR_TRACKER_H

// Detour tracking helpers. Provides lightweight wrappers you can call via
// DETOUR_PTR/DETOUR_MOD/UNDETOUR macros below to automatically record
// file/line/function and allow runtime dump of active detours.

#ifdef __cplusplus
extern "C" {
#endif

void * detour_create_and_track_ptr(void * orig, void * detour, int patchType, int detourLen,
    const char * file, int line, const char * function,
    const char * origExpr, const char * detourExpr, const char * patchExpr, const char * lenExpr);

void * detour_create_and_track_mod(const char * moduleName, const char * procName, void * detour, int patchType, int detourLen,
    const char * file, int line, const char * function,
    const char * moduleExpr, const char * procExpr, const char * detourExpr, const char * patchExpr, const char * lenExpr);

// Variants that accept a human-readable reason string. If the same detour
// is requested multiple times, the reason is appended and the detour is not
// re-applied.
void * detour_create_and_track_ptr_r(void * orig, void * detour, int patchType, int detourLen, const char * reason,
    const char * file, int line, const char * function,
    const char * origExpr, const char * detourExpr, const char * patchExpr, const char * lenExpr);

void * detour_create_and_track_mod_r(const char * moduleName, const char * procName, void * detour, int patchType, int detourLen, const char * reason,
    const char * file, int line, const char * function,
    const char * moduleExpr, const char * procExpr, const char * detourExpr, const char * patchExpr, const char * lenExpr);

int detour_remove_and_track(void ** detourCreatePtrAddr, const char * file, int line, const char * function);

void detour_tracker_dump(void);
int detour_tracker_active_count(void);

#ifdef __cplusplus
}
#endif

// Convenience macros (explicit use, no global overrides)
#define DETOUR_PTR(orig, detour, patchType, detourLen) \
	detour_create_and_track_ptr((void*)(orig), (void*)(detour), (int)(patchType), (int)(detourLen), \
		__FILE__, __LINE__, __FUNCTION__, #orig, #detour, #patchType, #detourLen)

#define DETOUR_PTR_R(orig, detour, patchType, detourLen, reason) \
	detour_create_and_track_ptr_r((void*)(orig), (void*)(detour), (int)(patchType), (int)(detourLen), (const char*)(reason), \
		__FILE__, __LINE__, __FUNCTION__, #orig, #detour, #patchType, #detourLen)

#define DETOUR_MOD(moduleName, procName, detour, patchType, detourLen) \
	detour_create_and_track_mod((moduleName), (procName), (void*)(detour), (int)(patchType), (int)(detourLen), \
		__FILE__, __LINE__, __FUNCTION__, #moduleName, #procName, #detour, #patchType, #detourLen)

#define DETOUR_MOD_R(moduleName, procName, detour, patchType, detourLen, reason) \
	detour_create_and_track_mod_r((moduleName), (procName), (void*)(detour), (int)(patchType), (int)(detourLen), (const char*)(reason), \
		__FILE__, __LINE__, __FUNCTION__, #moduleName, #procName, #detour, #patchType, #detourLen)

#define UNDETOUR(ptrAddr) detour_remove_and_track((void**)(ptrAddr), __FILE__, __LINE__, __FUNCTION__)

#define DETOUR_DUMP() detour_tracker_dump()
#define DETOUR_ACTIVE_COUNT() detour_tracker_active_count()

// Optional global overrides: define DETOUR_TRACKER_AUTOWRAP before including this header
#if !defined(DETOUR_TRACKER_NO_MACROS) && defined(DETOUR_TRACKER_AUTOWRAP)
  // Macro overloading helper to choose among 4/5/6-arg DetourCreate forms.
  // 4 args: pointer form (orig, detour, type, len)
  // 5 args: module form (moduleName, procName, detour, type, len)
  // 6 args: module form with reason (moduleName, procName, detour, type, len, reason)
  #define __DT_GET_MACRO6(_1,_2,_3,_4,_5,_6, NAME, ...) NAME
  #define DetourCreate(...) __DT_GET_MACRO6(__VA_ARGS__, DetourCreate6_TRK, DetourCreate5_TRK, DetourCreate4_TRK)(__VA_ARGS__)
#define DetourCreate4_TRK(orig, detour, patchType, detourLen) \
      detour_create_and_track_ptr((void*)(orig), (void*)(detour), (int)(patchType), (int)(detourLen), \
          __FILE__, __LINE__, __FUNCTION__, #orig, #detour, #patchType, #detourLen)
  #define DetourCreate5_TRK(moduleName, procName, detour, patchType, detourLen) \
      detour_create_and_track_mod((moduleName), (procName), (void*)(detour), (int)(patchType), (int)(detourLen), \
          __FILE__, __LINE__, __FUNCTION__, #moduleName, #procName, #detour, #patchType, #detourLen)
  #define DetourCreate6_TRK(moduleName, procName, detour, patchType, detourLen, reason) \
      detour_create_and_track_mod_r((moduleName), (procName), (void*)(detour), (int)(patchType), (int)(detourLen), (const char*)(reason), \
          __FILE__, __LINE__, __FUNCTION__, #moduleName, #procName, #detour, #patchType, #detourLen)
  #define DetourRemove(ptrAddr) detour_remove_and_track((void**)(ptrAddr), __FILE__, __LINE__, __FUNCTION__)

  // Optional: pointer-form with reason via DetourCreateR to avoid ambiguity with the 5-arg module form
  #define __DT_GET_MACRO_R(_1,_2,_3,_4,_5,_6, NAME, ...) NAME
  #define DetourCreateR(...) __DT_GET_MACRO_R(__VA_ARGS__, DetourCreateModR_TRK, DetourCreatePtrR_TRK)(__VA_ARGS__)
  #define DetourCreatePtrR_TRK(orig, detour, patchType, detourLen, reason) \
      detour_create_and_track_ptr_r((void*)(orig), (void*)(detour), (int)(patchType), (int)(detourLen), (const char*)(reason), \
          __FILE__, __LINE__, __FUNCTION__, #orig, #detour, #patchType, #detourLen)
  #define DetourCreateModR_TRK(moduleName, procName, detour, patchType, detourLen, reason) \
      detour_create_and_track_mod_r((moduleName), (procName), (void*)(detour), (int)(patchType), (int)(detourLen), (const char*)(reason), \
          __FILE__, __LINE__, __FUNCTION__, #moduleName, #procName, #detour, #patchType, #detourLen)

  // Pointer/module forms that also accept a trailing feature/context string.
  // The extra argument is intentionally ignored at runtime; it exists for static reporting context.
  #define DetourCreateRF(orig, detour, patchType, detourLen, reason, feature) \
      detour_create_and_track_ptr_r((void*)(orig), (void*)(detour), (int)(patchType), (int)(detourLen), (const char*)(reason), \
          __FILE__, __LINE__, __FUNCTION__, #orig, #detour, #patchType, #detourLen)
  #define DetourCreateModRF(moduleName, procName, detour, patchType, detourLen, reason, feature) \
      detour_create_and_track_mod_r((moduleName), (procName), (void*)(detour), (int)(patchType), (int)(detourLen), (const char*)(reason), \
          __FILE__, __LINE__, __FUNCTION__, #moduleName, #procName, #detour, #patchType, #detourLen)
#endif

#endif

