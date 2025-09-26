#ifndef FEATURES_REF_FIXES_HOOKS_H
#define FEATURES_REF_FIXES_HOOKS_H

#ifdef __cplusplus
extern "C" {
#endif

void refgl_on_VID_CheckChanges(void);
int refgl_on_VID_LoadRefresh(char *name);
void refgl_on_GL_BuildPolygonFromSurface(void *msurface_s);
void refgl_on_R_BlendLightmaps(void);
void refgl_on_drawTeamIcons(float * targetPlayerOrigin,char * playerName,char * imageNameTeamIcon,int redOrBlue);

#ifdef __cplusplus
}
#endif

#endif
