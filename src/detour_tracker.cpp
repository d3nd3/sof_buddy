#include <windows.h>

#include <vector>
#include <string>

#include "util.h"
#include "DetourXS/detourxs.h"
#define DETOUR_TRACKER_NO_MACROS
#include "detour_tracker.h"
// Avoid macro recursion inside this implementation unit
// No macro redefinitions due to DETOUR_TRACKER_NO_MACROS

struct DetourRecord {
	void * trampoline;
	void * origPtr;      // for pointer-form detours; nullptr for module-form
	void * detourPtr;    // for pointer-form detours; nullptr for module-form
	std::string origExpr;
	std::string detourExpr;
	std::string patchExpr;
	std::string lenExpr;
	std::string file;
	std::string function;
	int line;
	std::vector<std::string> reasons;
	bool active;
};

static std::vector<DetourRecord> g_detours;
static CRITICAL_SECTION g_detours_cs;
static LONG g_detours_cs_init = 0;

static inline void detours_lock_init_once(void)
{
    if (InterlockedCompareExchange(&g_detours_cs_init, 1, 0) == 0) {
        InitializeCriticalSectionAndSpinCount(&g_detours_cs, 4000);
    }
}

static inline void detours_lock(void)
{
    if (!g_detours_cs_init) detours_lock_init_once();
    EnterCriticalSection(&g_detours_cs);
}

static inline void detours_unlock(void)
{
    LeaveCriticalSection(&g_detours_cs);
}

static void track_add(void * trampoline,
		const char * file, int line, const char * function,
		const char * origExpr, const char * detourExpr,
		const char * patchExpr, const char * lenExpr,
		const char * reason)
{
    detours_lock();
	DetourRecord rec;
	rec.trampoline = trampoline;
	rec.origPtr = NULL;
	rec.detourPtr = NULL;
	rec.origExpr = origExpr ? origExpr : "";
	rec.detourExpr = detourExpr ? detourExpr : "";
	rec.patchExpr = patchExpr ? patchExpr : "";
	rec.lenExpr = lenExpr ? lenExpr : "";
	rec.file = file ? file : "";
	rec.function = function ? function : "";
	rec.line = line;
	if (reason && *reason) rec.reasons.push_back(reason);
	rec.active = (trampoline != NULL);
    g_detours.push_back(rec);

	if (trampoline) {
		PrintOut(PRINT_LOG, "DetourCreate OK: %s -> %s (%s:%d)\n",
			rec.origExpr.c_str(), rec.detourExpr.c_str(), rec.file.c_str(), rec.line);
	} else {
		PrintOut(PRINT_BAD, "DetourCreate FAILED: %s -> %s (%s:%d)\n",
			rec.origExpr.c_str(), rec.detourExpr.c_str(), rec.file.c_str(), rec.line);
	}
    detours_unlock();
}

static void track_remove(void ** detourCreatePtrAddr,
		const char * file, int line, const char * function)
{
    detours_lock();
	void * tramp = detourCreatePtrAddr ? *detourCreatePtrAddr : NULL;
	for (size_t i = 0; i < g_detours.size(); ++i) {
		if (g_detours[i].trampoline == tramp && g_detours[i].active) {
			g_detours[i].active = false;
			PrintOut(PRINT_LOG, "DetourRemove: %s -> %s (%s:%d)\n",
				g_detours[i].origExpr.c_str(), g_detours[i].detourExpr.c_str(), file ? file : "", line);
			break;
		}
	}
    detours_unlock();
}

extern "C" {

void * detour_create_and_track_ptr(void * orig, void * detour, int patchType, int detourLen,
	const char * file, int line, const char * function,
	const char * origExpr, const char * detourExpr, const char * patchExpr, const char * lenExpr)
{
    // Gracefully avoid reapplying an existing identical detour
    detours_lock();
    for (size_t i = 0; i < g_detours.size(); ++i) {
        DetourRecord &r = g_detours[i];
        if (r.active && r.origPtr == orig && r.detourPtr == detour) {
            void * tramp = r.trampoline;
            detours_unlock();
            return tramp;
        }
    }
    detours_unlock();
	void * trampoline = DetourCreate(orig, detour, patchType, detourLen);
	track_add(trampoline, file, line, function, origExpr, detourExpr, patchExpr, lenExpr, NULL);
    // Store pointer identity for duplicate detection
    detours_lock();
    if (trampoline) {
        for (size_t i = g_detours.size(); i-- > 0;) {
            if (g_detours[i].trampoline == trampoline) {
                g_detours[i].origPtr = orig;
                g_detours[i].detourPtr = detour;
                break;
            }
        }
    }
    detours_unlock();
	return trampoline;
}

void * detour_create_and_track_mod(const char * moduleName, const char * procName, void * detour, int patchType, int detourLen,
	const char * file, int line, const char * function,
	const char * moduleExpr, const char * procExpr, const char * detourExpr, const char * patchExpr, const char * lenExpr)
{
	// Check existing by composed orig expression and detour pointer string expr match
	detours_lock();
	std::string origExprKey = std::string(moduleExpr ? moduleExpr : "module") + "!" + (procExpr ? procExpr : "proc");
	for (size_t i = 0; i < g_detours.size(); ++i) {
		DetourRecord &r = g_detours[i];
		if (r.active && r.origExpr == origExprKey && r.detourExpr == (detourExpr ? detourExpr : "")) {
			void * tramp = r.trampoline;
			detours_unlock();
			return tramp;
		}
	}
	detours_unlock();

	void * trampoline = DetourCreate(moduleName, procName, detour, patchType, detourLen);
	// Compose orig expression as Module!Proc for readability
	std::string origExpr = origExprKey;
	track_add(trampoline, file, line, function, origExpr.c_str(), detourExpr, patchExpr, lenExpr, NULL);
	return trampoline;
}

void * detour_create_and_track_ptr_r(void * orig, void * detour, int patchType, int detourLen, const char * reason,
	const char * file, int line, const char * function,
	const char * origExpr, const char * detourExpr, const char * patchExpr, const char * lenExpr)
{
	// If an equivalent detour is already active, append reason and return existing trampoline
	detours_lock();
	for (size_t i = 0; i < g_detours.size(); ++i) {
		if (g_detours[i].active && g_detours[i].origExpr == (origExpr ? origExpr : "") && g_detours[i].detourExpr == (detourExpr ? detourExpr : "")) {
			if (reason && *reason) g_detours[i].reasons.push_back(reason);
			void * tramp = g_detours[i].trampoline;
			detours_unlock();
			return tramp;
		}
	}
	detours_unlock();

	void * trampoline = DetourCreate(orig, detour, patchType, detourLen);
	track_add(trampoline, file, line, function, origExpr, detourExpr, patchExpr, lenExpr, reason);
    // Store pointer identity for duplicate detection
    detours_lock();
    if (trampoline) {
        for (size_t i = g_detours.size(); i-- > 0;) {
            if (g_detours[i].trampoline == trampoline) {
                g_detours[i].origPtr = orig;
                g_detours[i].detourPtr = detour;
                break;
            }
        }
    }
    detours_unlock();
	return trampoline;
}

void * detour_create_and_track_mod_r(const char * moduleName, const char * procName, void * detour, int patchType, int detourLen, const char * reason,
	const char * file, int line, const char * function,
	const char * moduleExpr, const char * procExpr, const char * detourExpr, const char * patchExpr, const char * lenExpr)
{
	// Compose key and check existing active detour
	std::string origKey = std::string(moduleExpr ? moduleExpr : "module") + "!" + (procExpr ? procExpr : "proc");
	detours_lock();
	for (size_t i = 0; i < g_detours.size(); ++i) {
		if (g_detours[i].active && g_detours[i].origExpr == origKey && g_detours[i].detourExpr == (detourExpr ? detourExpr : "")) {
			if (reason && *reason) g_detours[i].reasons.push_back(reason);
			void * tramp = g_detours[i].trampoline;
			detours_unlock();
			return tramp;
		}
	}
	detours_unlock();

	void * trampoline = DetourCreate(moduleName, procName, detour, patchType, detourLen);
	track_add(trampoline, file, line, function, origKey.c_str(), detourExpr, patchExpr, lenExpr, reason);
	return trampoline;
}

int detour_remove_and_track(void ** detourCreatePtrAddr, const char * file, int line, const char * function)
{
	track_remove(detourCreatePtrAddr, file, line, function);
	return DetourRemove(detourCreatePtrAddr);
}

void detour_tracker_dump(void)
{
    detours_lock();
	int active = 0;
	for (size_t i = 0; i < g_detours.size(); ++i) {
		if (g_detours[i].active) active++;
	}
	PrintOut(PRINT_LOG, "Detours: %d active, %d total\n", active, (int)g_detours.size());
	for (size_t i = 0; i < g_detours.size(); ++i) {
		const DetourRecord &r = g_detours[i];
		PrintOut(r.active ? PRINT_LOG : PRINT_LOG, "  [%s] %s -> %s at %s:%d\n",
			r.active ? "ACTIVE" : "REMOVED",
			r.origExpr.c_str(), r.detourExpr.c_str(), r.file.c_str(), r.line);
		if (!r.reasons.empty()) {
			for (size_t k = 0; k < r.reasons.size(); ++k) {
				PrintOut(PRINT_LOG, "      reason[%d]: %s\n", (int)k, r.reasons[k].c_str());
			}
		}
	}
    detours_unlock();
}

int detour_tracker_active_count(void)
{
    detours_lock();
	int active = 0;
	for (size_t i = 0; i < g_detours.size(); ++i) if (g_detours[i].active) active++;
    detours_unlock();
    return active;
}

} // extern "C"


