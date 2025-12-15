#pragma once

#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "sof_compat.h"

extern int lightblend_src;
extern int lightblend_dst;

extern int *lightblend_target_src;
extern int *lightblend_target_dst;
extern float *lighting_cutoff_target;
extern double *water_size_double_target;
extern float *water_size_float_target;
extern unsigned char *shiny_spherical_target1;
extern unsigned char *shiny_spherical_target3;
extern float *cam_vforward;
extern float *cam_vup;
extern float *cam_vright;

extern void (__stdcall *real_glBlendFunc)(unsigned int, unsigned int);
extern void (__cdecl *oAdjustTexCoords)(float*, float*, float*, float*, float*, float*);
extern void (__stdcall *glTexCoord2f)(float, float);
extern void (__stdcall *glVertex3f)(float, float, float);
extern void (__stdcall *glBegin)(unsigned int);
extern void (__stdcall *glEnd)(void);
extern void (__stdcall *glColor3f)(float, float, float);

extern cvar_t * _sofbuddy_lighting_overbright;
extern cvar_t * _sofbuddy_lighting_cutoff;
extern cvar_t * _sofbuddy_water_size;
extern cvar_t * _sofbuddy_lightblend_src;
extern cvar_t * _sofbuddy_lightblend_dst;
extern cvar_t * _sofbuddy_shiny_spherical;
extern cvar_t * gl_ext_multitexture;

extern bool is_blending;

void create_lightingblend_cvars(void);
void lightblend_change(cvar_t * cvar);
void lighting_overbright_change(cvar_t * cvar);
void lighting_cutoff_change(cvar_t * cvar);
void water_size_change(cvar_t * cvar);
void shiny_spherical_change(cvar_t * cvar);
void __stdcall glBlendFunc_R_BlendLightmaps(unsigned int sfactor, unsigned int dfactor);
void __cdecl hkAdjustTexCoords(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t);

#endif

