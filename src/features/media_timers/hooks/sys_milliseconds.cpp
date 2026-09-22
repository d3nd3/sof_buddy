#include "feature_config.h"

#if FEATURE_MEDIA_TIMERS

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../shared.h"

#include <windows.h>
#include <mmsystem.h>
#include <cstdint>

// Ensure compatibility with old Windows XP SDK headers
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

namespace {

struct EngineClock {
    bool          initialized = false;
    bool          is_win_xp = false;
    HANDLE        main_thread = nullptr;
    DWORD_PTR     process_affinity_mask = 0;

    int64_t       qpc_freq = 0;
    int64_t       qpc_base = 0;
    std::uint32_t origin_ms = 0;
    std::uint32_t last_ms = 0;

    volatile std::uint32_t* curtime_ptr = nullptr;

    volatile std::uint32_t* Curtime() {
        if (!curtime_ptr) {
            curtime_ptr = static_cast<volatile std::uint32_t*>(
                rvaToAbsExe(reinterpret_cast<void*>(0x390D38)));
        }
        return curtime_ptr;
    }

    bool CheckIfWindowsXP() {
        OSVERSIONINFOEXA osvi = { sizeof(OSVERSIONINFOEXA) };
        if (GetVersionExA(reinterpret_cast<OSVERSIONINFOA*>(&osvi))) {
            // Windows XP is NT version 5.1 (or 5.2 for XP 64-bit / Server 2003)
            return (osvi.dwMajorVersion == 5);
        }
        return false;
    }

    void QueryCounterSafe(LARGE_INTEGER* out_cur) {
        // Microsoft recommendation: on Windows XP only, query QPC on a single 
        // fixed core to prevent cross-core TSC jitter. On Vista+, do not touch affinity.
        if (is_win_xp && process_affinity_mask != 0) {
            DWORD_PTR thread_mask = 1; // Pin temporarily to Core 0
            DWORD_PTR old_mask = SetThreadAffinityMask(main_thread, thread_mask);
            QueryPerformanceCounter(out_cur);
            SetThreadAffinityMask(main_thread, old_mask);
        } else {
            QueryPerformanceCounter(out_cur);
        }
    }

    void Init(std::uint32_t stock_origin) {
        LARGE_INTEGER f;
        if (!QueryPerformanceFrequency(&f) || f.QuadPart <= 0) {
            // Fallback to stock timeGetTime if hardware lacks QPC
            qpc_freq = 0;
            return;
        }
        qpc_freq = f.QuadPart;

        is_win_xp = CheckIfWindowsXP();
        main_thread = GetCurrentThread();

        DWORD_PTR sys_mask = 0;
        GetProcessAffinityMask(GetCurrentProcess(), &process_affinity_mask, &sys_mask);

        LARGE_INTEGER cur;
        QueryCounterSafe(&cur);

        qpc_base = cur.QuadPart;
        origin_ms = stock_origin;
        last_ms = stock_origin;

        if (volatile std::uint32_t* ct = Curtime())
            *ct = stock_origin;

        initialized = true;
    }

    std::uint32_t GetMilliseconds(detour_Sys_Milliseconds::tSys_Milliseconds original) {
        if (!initialized) {
            std::uint32_t stock = original ? static_cast<std::uint32_t>(original()) 
                                           : static_cast<std::uint32_t>(timeGetTime());
            Init(stock);
            if (qpc_freq <= 0)
                return stock;
        }

        LARGE_INTEGER cur;
        QueryCounterSafe(&cur);

        // Compute elapsed ticks from the static base (never shift base backwards)
        int64_t elapsed_ticks = cur.QuadPart - qpc_base;
        if (elapsed_ticks < 0)
            elapsed_ticks = 0;

        std::uint32_t elapsed_ms = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(elapsed_ticks) * 1000ull) /
            static_cast<std::uint64_t>(qpc_freq));

        std::uint32_t ms = origin_ms + elapsed_ms;

        // Strictly monotonic clamp (32-bit wrap safe):
        // Prevents any negative time steps without modifying the origin
        if (static_cast<std::int32_t>(ms - last_ms) < 0)
            ms = last_ms;

        if (ms != last_ms) {
            last_ms = ms;
            if (volatile std::uint32_t* ct = Curtime())
                *ct = ms;
        }

        return ms;
    }
};

EngineClock g_clock;

} // namespace

int my_Sys_Milliseconds(void)
{
    // If called without original trampoline, fallback to timeGetTime
    return static_cast<int>(g_clock.GetMilliseconds(nullptr));
}

int my_TimeGetTime(void)
{
    return my_Sys_Milliseconds();
}

int sys_milliseconds_override_callback(detour_Sys_Milliseconds::tSys_Milliseconds original)
{
    return static_cast<int>(g_clock.GetMilliseconds(original));
}

#endif // FEATURE_MEDIA_TIMERS