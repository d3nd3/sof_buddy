#include "sof_buddy.h"
#include "util.h"
#include "features/ref_fixes/hooks.h"

// Forward declarations to existing implementations in ref_fixes.cpp
extern void my_VID_CheckChanges(void);
extern qboolean my_VID_LoadRefresh(char *name);
extern void my_GL_BuildPolygonFromSurface(void *msurface_s);
extern void my_R_BlendLightmaps(void);
extern void my_drawTeamIcons(float * targetPlayerOrigin,char * playerName,char * imageNameTeamIcon,int redOrBlue);

void refgl_on_VID_CheckChanges(void) { my_VID_CheckChanges(); }
int refgl_on_VID_LoadRefresh(char *name) { return my_VID_LoadRefresh(name); }
void refgl_on_GL_BuildPolygonFromSurface(void *msurface_s) { my_GL_BuildPolygonFromSurface(msurface_s); }
void refgl_on_R_BlendLightmaps(void) { my_R_BlendLightmaps(); }
void refgl_on_drawTeamIcons(float * targetPlayerOrigin,char * playerName,char * imageNameTeamIcon,int redOrBlue) { my_drawTeamIcons(targetPlayerOrigin, playerName, imageNameTeamIcon, redOrBlue); }
