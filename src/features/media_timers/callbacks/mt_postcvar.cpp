#include "feature_config.h"

#if FEATURE_MEDIA_TIMERS

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"
#include "generated_detours.h"

void mediaTimers_PostCvarInit(void)
{
	PrintOut(PRINT_LOG, "Media timers: PostCvarInit\n");
	cl_maxfps = detour_Cvar_Get::oCvar_Get("cl_maxfps","30",0,&cl_maxfps_change);
	if (cl_maxfps) cl_maxfps_change(cl_maxfps);

	// Persistence owner for cl_maxfps (this feature owns the cvar + callback):
	// - With SoFPlus: spm_settings.cfg owns it (saved on change and shutdown, see
	//   sofbuddy_cfg_save_sofplus_settings); keep it unarchived to avoid a
	//   config.cfg vs spm_settings.cfg split.
	// - Vanilla (no SoFPlus): archive so config.cfg persists the cap.
	if (cl_maxfps) {
		const bool sofplus_present =
			findCvar(const_cast<char*>("_sp_cl_master_gamespy")) != nullptr ||
			findCvar(const_cast<char*>("_sp_cl_cpu_cool")) != nullptr;
		if (sofplus_present) cl_maxfps->flags &= ~CVAR_ARCHIVE;
		else cl_maxfps->flags |= CVAR_ARCHIVE;
	}
	
	create_mediatimers_cvars();
	
	PrintOut(PRINT_LOG, "Media timers: Applying memory patches...\n");
}

#endif // FEATURE_MEDIA_TIMERS
