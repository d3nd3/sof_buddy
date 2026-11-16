#include "feature_config.h"

#if FEATURE_SOUND_ALT_TAB_FIX

#include "util.h"

#include <windows.h>
#include <cstdint>

namespace {

using SndActivateFn = void(__cdecl*)(int);
using SndStopAllSoundsFn = void(__cdecl*)();

// SoF.exe 1.06a stores the sound DLL exports in these globals (LoadSoundModule).
constexpr uintptr_t kSndActivateSlotRva = 0x00390D6C;
constexpr uintptr_t kSndStopAllSoundsSlotRva = 0x00390D4C;

SndActivateFn g_original_activate = nullptr;
SndStopAllSoundsFn g_stop_all_sounds = nullptr;

void __cdecl sofbuddy_snd_activate(int active) {
    PrintOut(PRINT_LOG, "sound_alt_tab_fix: S_Activate(%d)\n", active);

    SndActivateFn original = g_original_activate;
    if (original) {
        original(active);
    }

    // Defsnd releases its DirectSound buffers while the window is inactive. S_Update still
    // queues every sound started in the background (up to 128 pending starts), and the first
    // active frame would issue all of them at once. Drop the held sounds on reactivation.
    if (active && g_stop_all_sounds) {
        g_stop_all_sounds();
    }
}

} // namespace

void sound_alt_tab_fix_load_sound_module_post() {
    auto activate_slot = reinterpret_cast<SndActivateFn*>(
        rvaToAbsExe(reinterpret_cast<void*>(kSndActivateSlotRva)));
    auto stop_all_slot = reinterpret_cast<SndStopAllSoundsFn*>(
        rvaToAbsExe(reinterpret_cast<void*>(kSndStopAllSoundsSlotRva)));
    if (!activate_slot || !stop_all_slot || !*stop_all_slot) {
        return;
    }

    g_stop_all_sounds = *stop_all_slot;

    SndActivateFn loaded_activate = *activate_slot;
    if (!loaded_activate || loaded_activate == &sofbuddy_snd_activate) {
        return;
    }

    // EAXSnd's S_Activate is a no-op and its effects keep mixing while inactive, so
    // there is no pending queue to flush (and stopping would just cut live sounds).
    HMODULE eaxsnd = GetModuleHandleA("eaxsnd.dll");
    void* eaxsnd_activate = eaxsnd ? reinterpret_cast<void*>(GetProcAddress(eaxsnd, "S_Activate"))
                                   : nullptr;
    if (eaxsnd_activate && eaxsnd_activate == reinterpret_cast<void*>(loaded_activate)) {
        PrintOut(PRINT_LOG, "sound_alt_tab_fix: EAXSnd does not pause effects; not wrapping\n");
        return;
    }

    g_original_activate = loaded_activate;
    *activate_slot = &sofbuddy_snd_activate;

    PrintOut(PRINT_LOG, "sound_alt_tab_fix: wrapped S_Activate at %p\n",
             reinterpret_cast<void*>(loaded_activate));
}

#endif // FEATURE_SOUND_ALT_TAB_FIX
