#include "sof_buddy.h"
#include "util.h"

#include "features/ref_fixes/hooks.h"
#include "ref_gl_hooks.h"

// Centralized ref_gl wrappers that call feature-owned hooks.

extern void (*orig_VID_CheckChanges)(void);
extern qboolean (*orig_VID_LoadRefresh)(char*);
extern void (*orig_GL_BuildPolygonFromSurface)(void*);
extern void (*orig_R_BlendLightmaps)(void);
extern void (*orig_drawTeamIcons)(void*,void*,void*,void*);

void refgl_VID_CheckChanges(void) { refgl_on_VID_CheckChanges(); }
int refgl_VID_LoadRefresh(char *name) { return refgl_on_VID_LoadRefresh(name); }
void refgl_GL_BuildPolygonFromSurface(void *msurface_s) { refgl_on_GL_BuildPolygonFromSurface(msurface_s); }
void refgl_R_BlendLightmaps(void) { refgl_on_R_BlendLightmaps(); }
void refgl_drawTeamIcons(void *a, void *b, void *c, void *d) {
	// Casts held in feature implementation
	refgl_on_drawTeamIcons((float*)a, (char*)b, (char*)c, (int)(intptr_t)d);
}


