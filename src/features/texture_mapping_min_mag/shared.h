#pragma once

#include "feature_config.h"

#if FEATURE_TEXTURE_MAPPING_MIN_MAG

#include "sof_compat.h"

using glTexParameterf_t = void(__stdcall*)(int target_tex, int param_name, float value);

extern cvar_t* _sofbuddy_minfilter_unmipped;
extern cvar_t* _sofbuddy_magfilter_unmipped;
extern cvar_t* _sofbuddy_minfilter_mipped;
extern cvar_t* _sofbuddy_magfilter_mipped;
extern cvar_t* _sofbuddy_minfilter_ui;
extern cvar_t* _sofbuddy_magfilter_ui;

extern cvar_t* _gl_texturemode;
extern glTexParameterf_t orig_glTexParameterf;

void create_texturemapping_cvars(void);
void minfilter_change(cvar_t *cvar);
void magfilter_change(cvar_t *cvar);
void texturemapping_PostCvarInit();
void setup_minmag_filters(char const* name);

void __stdcall orig_glTexParameterf_min_mipped(int target_tex, int param_name, float value);
void __stdcall orig_glTexParameterf_mag_mipped(int target_tex, int param_name, float value);
void __stdcall orig_glTexParameterf_min_unmipped(int target_tex, int param_name, float value);
void __stdcall orig_glTexParameterf_mag_unmipped(int target_tex, int param_name, float value);
void __stdcall orig_glTexParameterf_min_ui(int target_tex, int param_name, float value);
void __stdcall orig_glTexParameterf_mag_ui(int target_tex, int param_name, float value);

#endif // FEATURE_TEXTURE_MAPPING_MIN_MAG


