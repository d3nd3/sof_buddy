#pragma once

#include "feature_config.h"

#if FEATURE_HTTP_MAPS

#include "sof_compat.h"
#include "generated_detours.h"

extern cvar_t* _sofbuddy_http_maps;
extern cvar_t* _sofbuddy_http_maps_dl_1;
extern cvar_t* _sofbuddy_http_maps_dl_2;
extern cvar_t* _sofbuddy_http_maps_dl_3;
extern cvar_t* _sofbuddy_http_maps_crc_1;
extern cvar_t* _sofbuddy_http_maps_crc_2;
extern cvar_t* _sofbuddy_http_maps_crc_3;

void create_http_maps_cvars(void);
void http_maps_on_parse_configstring_post(void);
void http_maps_try_begin_precache(detour_CL_Precache_f::tCL_Precache_f original);
void http_maps_pump(void);
void http_maps_run_deferred_continue_if_pending(void);

#endif // FEATURE_HTTP_MAPS
