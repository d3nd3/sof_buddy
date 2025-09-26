#ifndef REF_GL_HOOKS_H
#define REF_GL_HOOKS_H

#ifdef __cplusplus
extern "C" {
#endif

void refgl_VID_CheckChanges(void);
int  refgl_VID_LoadRefresh(char *name);
void refgl_GL_BuildPolygonFromSurface(void *msurface_s);
void refgl_R_BlendLightmaps(void);
void refgl_drawTeamIcons(void *a, void *b, void *c, void *d);

#ifdef __cplusplus
}
#endif

#endif


