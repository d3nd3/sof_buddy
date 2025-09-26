#include "sof_buddy.h"

#include <unordered_map>
#include <string>
#include <math.h>
#include <iostream>

#include "sof_compat.h"
#include "features.h"

#include "../DetourXS/detourxs.h"


#include "util.h"

typedef struct {
	unsigned short w;
	unsigned short h;
} m32size;
std::unordered_map<std::string,m32size> default_textures;


cvar_t * _gl_texturemode = NULL;
cvar_t * gl_swapinterval = NULL;
cvar_t * vid_ref = NULL;

qboolean (*orig_VID_LoadRefresh)( char *name ) = NULL;
void (*orig_VID_CheckChanges)(void) = NULL;

void (*orig_GL_BuildPolygonFromSurface)(void *fa) = NULL;
int (*orig_R_Init)( void *hinstance, void *hWnd, void * unknown ) = NULL;
void (*orig_drawTeamIcons)(void * param1,void * param2,void * param3,void * param4) = NULL;
void (*orig_R_BlendLightmaps)(void) = NULL;
void (*orig_GL_TextureMode)(char * mode) = NULL;
void (__stdcall *orig_glTexParameterf)(int target_tex, int param_name, float value) = NULL;
int (*orig_R_SetMode)(void * deviceMode) = NULL;


extern void scaledFont_init(void);

HMODULE __stdcall RefInMemory(LPCSTR lpLibFileName);
void on_ref_init(void);
void initDefaultTexSizes(void);

void my_VID_CheckChanges(void);
qboolean my_VID_LoadRefresh( char *name );
int my_R_Init(void *hinstance, void *hWnd, void * unknown );
int my_R_SetMode(void * deviceMode);

void __cdecl my_GL_RenderLightmappedPoly_intercept(void * surf,void * surf2);
void my_R_BlendLightmaps(void);
void my_GL_BuildPolygonFromSurface(void *msurface_s);
void my_drawTeamIcons(float * targetPlayerOrigin,char * playerName,char * imageNameTeamIcon,int redOrBlue);
int TeamIconInterceptFix(void);

void __stdcall glBlendFunc_R_BlendLightmaps(unsigned int sfactor,unsigned int dfactor);

#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307
#define GL_SRC_ALPHA_SATURATE             0x0308

#define GL_CONSTANT_COLOR                 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR       0x8002
#define GL_CONSTANT_ALPHA                 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA       0x8004


#define GL_BLEND_COLOR                    0x8005
#define GL_BLEND_EQUATION                 0x8009


#ifdef FEATURE_ALT_LIGHTING
//SRC = Incoming Data(Light)
//DST = Existing Data(Texture)
//defaults.
int lightblend_src = GL_ZERO;
int lightblend_dst = GL_SRC_COLOR;
int *lightblend_target_src = 0x300A4610;
int *lightblend_target_dst = 0x300A43FC;

//15
void lightblend_change(cvar_t * cvar) {
	int value = cvar->string;
	if (!strcmp(cvar->string,"GL_ZERO")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_ZERO;
		}
		else
			lightblend_dst = GL_ZERO;
	} else if (!strcmp(cvar->string,"GL_ONE")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_ONE;
		}
		else
			lightblend_dst = GL_ONE;
	} else if (!strcmp(cvar->string,"GL_SRC_COLOR")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_SRC_COLOR;
		}
		else
			lightblend_dst = GL_SRC_COLOR;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_SRC_COLOR")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_ONE_MINUS_SRC_COLOR;
		}
		else
			lightblend_dst = GL_ONE_MINUS_SRC_COLOR;
	} else if (!strcmp(cvar->string,"GL_DST_COLOR")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_DST_COLOR;
		}
		else
			lightblend_dst = GL_DST_COLOR;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_DST_COLOR")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_ONE_MINUS_DST_COLOR;
		}
		else
			lightblend_dst = GL_ONE_MINUS_DST_COLOR;
	} else if (!strcmp(cvar->string,"GL_SRC_ALPHA")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_SRC_ALPHA;
		}
		else
			lightblend_dst = GL_SRC_ALPHA;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_SRC_ALPHA")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_ONE_MINUS_SRC_ALPHA;
		}
		else
			lightblend_dst = GL_ONE_MINUS_SRC_ALPHA;
	} else if (!strcmp(cvar->string,"GL_DST_ALPHA")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_DST_ALPHA;
		}
		else
			lightblend_dst = GL_DST_ALPHA;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_DST_ALPHA")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_ONE_MINUS_DST_ALPHA;
		}
		else
			lightblend_dst = GL_ONE_MINUS_DST_ALPHA;
	} else if (!strcmp(cvar->string,"GL_CONSTANT_COLOR")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_CONSTANT_COLOR;
		}
		else
			lightblend_dst = GL_CONSTANT_COLOR;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_CONSTANT_COLOR")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_ONE_MINUS_CONSTANT_COLOR;
		}
		else
			lightblend_dst = GL_ONE_MINUS_CONSTANT_COLOR;
	} else if (!strcmp(cvar->string,"GL_CONSTANT_ALPHA")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_CONSTANT_ALPHA;
		}
		else
			lightblend_dst = GL_CONSTANT_ALPHA;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_CONSTANT_ALPHA")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_ONE_MINUS_CONSTANT_ALPHA;
		}
		else
			lightblend_dst = GL_ONE_MINUS_CONSTANT_ALPHA;
	} else if (!strcmp(cvar->string,"GL_SRC_ALPHA_SATURATE")) {
		if (!strcmp(cvar->name,"_sofbuddy_lightblend_src")) {
			lightblend_src = GL_SRC_ALPHA_SATURATE;
		}
		else {
			lightblend_dst = GL_SRC_ALPHA_SATURATE;
		}
	} else {
		PrintOut(PRINT_BAD,"Bad lightblend_src value. See glBlendFunc() docs.\nGL_ZERO,GL_ONE,GL_SRC_COLOR,"
						   "GL_ONE_MINUS_SRC_COLOR,GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA,"
						   "GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_CONSTANT_COLOR,"
						   "GL_ONE_MINUS_CONSTANT_COLOR,GL_CONSTANT_ALPHA,GL_ONE_MINUS_CONSTANT_ALPHA,"
						   "GL_SRC_ALPHA_SATURATE"
		);
	}
}
#endif

typedef struct
{
	char *name;
	int gl_code;
} filter_mapping;
filter_mapping min_filter_modes[] = {
	{"GL_NEAREST",0x2600},
	{"GL_LINEAR",0x2601},
	{"GL_NEAREST_MIPMAP_NEAREST",0x2700},
	{"GL_LINEAR_MIPMAP_NEAREST",0x2701},
	{"GL_NEAREST_MIPMAP_LINEAR",0x2702},
	{"GL_LINEAR_MIPMAP_LINEAR",0x2703}
};
filter_mapping mag_filter_modes[] = {
	{"GL_NEAREST",0x2600},
	{"GL_LINEAR",0x2601}
};
int minfilter_unmipped = 0x2600;
int magfilter_unmipped = 0x2600;
int minfilter_mipped = 0x2600;
int magfilter_mipped = 0x2600;
int minfilter_ui = 0x2600;
int magfilter_ui = 0x2600;


/*
	A cvar change function should be careful to use the cvar its changing global variable, not set yet.
*/
void minfilter_change(cvar_t * cvar) {
	// MessageBox(NULL, cvar->string , cvar->name, MB_ICONERROR | MB_OK);
	for ( int i=0;i<6;i++) {
		if (!strcmp(min_filter_modes[i].name,cvar->string)) {
			// MessageBox(NULL, "Matched" , "Error", MB_ICONERROR | MB_OK);
			if ( !strcmp(cvar->name,"_sofbuddy_minfilter_unmipped") ) {
				minfilter_unmipped = min_filter_modes[i].gl_code;
			} else if ( !strcmp(cvar->name,"_sofbuddy_minfilter_mipped") ) {
				minfilter_mipped = min_filter_modes[i].gl_code;
			} else if ( !strcmp(cvar->name,"_sofbuddy_minfilter_ui") ) {
				minfilter_ui = min_filter_modes[i].gl_code;
			}
			if (_gl_texturemode)
				_gl_texturemode->modified = true;
			return;
		}
	}
	orig_Com_Printf("Invalid filter\n");
}

void magfilter_change(cvar_t * cvar) {
	for ( int i=0;i<2;i++) {
		if (!strcmp(mag_filter_modes[i].name,cvar->string)) {
			if ( !strcmp(cvar->name,"_sofbuddy_magfilter_unmipped") ) {
				magfilter_unmipped = mag_filter_modes[i].gl_code;
				orig_Com_Printf("Magfilter_unmipped set to : %s\n",mag_filter_modes[i].name);
			} else if ( !strcmp(cvar->name,"_sofbuddy_magfilter_mipped") ) {
				magfilter_mipped = mag_filter_modes[i].gl_code;
				orig_Com_Printf("Magfilter_mipped set to : %s\n",mag_filter_modes[i].name);
			} else if ( !strcmp(cvar->name,"_sofbuddy_magfilter_ui") ) {
				magfilter_ui = mag_filter_modes[i].gl_code;
				orig_Com_Printf("Magfilter_ui set to : %s\n",mag_filter_modes[i].name);
			}
			if (_gl_texturemode)
				_gl_texturemode->modified = true;
			return;
		}
	}
	orig_Com_Printf("Invalid filter\n");
}

void __stdcall orig_glTexParameterf_min_mipped(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,minfilter_mipped);
}
void __stdcall orig_glTexParameterf_mag_mipped(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,magfilter_mipped);
}
void __stdcall orig_glTexParameterf_min_unmipped(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,minfilter_unmipped);
}
void __stdcall orig_glTexParameterf_mag_unmipped(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,magfilter_unmipped);
}
void __stdcall orig_glTexParameterf_min_ui(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,minfilter_ui);
}
void __stdcall orig_glTexParameterf_mag_ui(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,magfilter_ui);
}

/*
	RefInMemory is a LoadLibrary() in-place Detour, for ref_gl.dll
*/
void refFixes_early(void) {
	//Direct LoadLibrary replacement.
	WriteE8Call(0x20066E75,&RefInMemory);
	WriteByte(0x20066E7A,0x90);

	orig_VID_LoadRefresh = DetourCreate((void*)0x20066E10,(void*)&my_VID_LoadRefresh,DETOUR_TYPE_JMP,5);

	//Fix gl_swapinterval on VID_CheckChanges
	orig_VID_CheckChanges = DetourCreate((void*)0x200670C0 ,(void*)&my_VID_CheckChanges,DETOUR_TYPE_JMP,5);


}
/*
	When the ref_gl.dll library is reloaded, detours are lost.
	So we use VID_LoadRefresh as entry point to reapply.

	Called at the end of QCommon_Init().

	Before 'exec config.cfg'

	IMPORTANT:
	  A cvar's modified function cannot use the global pointer because it hasnt' returned yet...
*/
void refFixes_cvars_init(void)
{
	//MessageBox(NULL, "refFixes_apply", "MessageBox Example", MB_OK);
	//std::cout << "refFixes_apply";
	
	#ifdef FEATURE_HD_TEX
	initDefaultTexSizes();
	#endif

	#ifdef FEATURE_ALT_LIGHTING
	create_reffixes_cvars();
	#endif
	
}


int current_vid_w;
int current_vid_h;
int * viddef_width = 0x2040365C;
int * viddef_height = 0x20403660;
void my_VID_CheckChanges(void)
{
	//trigger gl_swapinterval->modified for R_Init() -> GL_SetDefaultState()
	if ( vid_ref && vid_ref->modified ) {
		gl_swapinterval->modified = true;
	}
	orig_VID_CheckChanges();
	current_vid_w = *viddef_width;
	current_vid_h = *viddef_height;
}



/*
re.init() exported by ref_lib AND...
re.init() is called inside VID_LoadRefresh
'was previously using r_Init as entry, but could not hook it as never had signal that
ref_gl.dll is loaded, until too late and r_init has already been called'
*/
qboolean my_VID_LoadRefresh( char *name )
{	

	//vid_ref loaded.
	qboolean ret = orig_VID_LoadRefresh(name);
	
	// gl function pointers have been set.
	on_ref_init();

	return ret;
}

double fovfix_x = 78.6;
double fovfix_y = 78.6;
float teamviewFovAngle = 95;
//unsigned int orig_fovAdjustBytes = 0;

void (__stdcall *real_glBlendFunc)(int sfactor,int factor);

/*
	No way to hook this. Because inside VID_LoadRefresh().
*/
int my_R_Init(void *hinstance, void *hWnd, void * unknown )
{	
	/*
	DetourRemove(&orig_R_SetMode);
	orig_R_SetMode = DetourCreate((void*)0x3001BBD0,&my_R_SetMode,DETOUR_TYPE_JMP,6);
	*/
	int retval = orig_R_Init(hinstance,hWnd,unknown);

	return retval;
}

/*
void (__stdcall *orig_glMTexCoord2fSGIS)(int target, float x, float y) = NULL;
void (__stdcall *orig_glMTexCoord2fvSGIS)(int target, float *x) = NULL;
void (__stdcall * orig_glVertex3fv)(float * v) = NULL;
void (__stdcall * orig_glBegin)(int mode) = NULL;
*/

void hd_fix_init(void) {
	//texture uv coordinates scaling correctly.
	DetourRemove(&orig_GL_BuildPolygonFromSurface);
	orig_GL_BuildPolygonFromSurface = DetourCreate((void*)0x30016390,(void*)&my_GL_BuildPolygonFromSurface,DETOUR_TYPE_JMP,6);
}

void teamicon_fix_init(void) {
	/*
		Fov Teamicon Widescreen Fix
	*/

	DetourRemove(&orig_drawTeamIcons);
	orig_drawTeamIcons = DetourCreate((void*)0x30003040,(void*)&my_drawTeamIcons,DETOUR_TYPE_JMP,6);


	writeUnsignedIntegerAt(0x3000313F,(unsigned int)&fovfix_x);
	writeUnsignedIntegerAt(0x30003157,(unsigned int)&fovfix_y);

	// 0x30003187 + 5 - something = TeamIconInterceptFix
	writeIntegerAt(0x30003187,(int)&TeamIconInterceptFix - 0x30003187 - 4);

	//orig_fovAdjustBytes = *(unsigned int*)(0x200157A8);
	// fov adjust display
	writeUnsignedIntegerAt(0x200157A8,&teamviewFovAngle);
}
#ifdef FEATURE_ALT_LIGHTING
void lighting_fix_init(void) {
	/*
		Lighting Improrvement gl_ext_multitexture 1
	*/
	/*
	WriteE8Call(0x30015C20,&my_GL_RenderLightmappedPoly_intercept);
	//Jmp to glEnd() afterwards.
	WriteE9Jmp(0x30015C25,0x30015D18);
	//Make push surface_t, instead of image_t (push EAX -> push EBP instead)
	WriteByte(0x30015C1F,0x50);

	WriteByte(0x30015D18,0x90);
	WriteByte(0x30015D19,0x90);
	WriteByte(0x30015D1A,0x90);
	WriteByte(0x30015D1B,0x90);
	WriteByte(0x30015D1C,0x90);
	WriteByte(0x30015D1D,0x90);
	*/

	/*
	glMTexCoord2fSGIS = *(int*)0x300A45EC;
	glMTexCoord2fvSGIS = *(int*)0x300A431C;
	glVertex3fv = *(int*)0x300A4520;
	glBegin = *(int*)0x300A4710;
*/

	/*
		For multiply blending /w gl_ext_multitexture 0.
	*/
	DetourRemove(&orig_R_BlendLightmaps);
	orig_R_BlendLightmaps = DetourCreate((void*)0x30015440,(void*)&my_R_BlendLightmaps,DETOUR_TYPE_JMP,6);
	
	lightblend_change(_sofbuddy_lightblend_src);
	lightblend_change(_sofbuddy_lightblend_dst);

	real_glBlendFunc = *(int*)0x300A426C;
	//Hook glBlendFuncs
	WriteE8Call(0x3001B9A4,&glBlendFunc_R_BlendLightmaps);
	WriteByte(0x3001B9A9,0x90);
	WriteE8Call(0x3001B690,&glBlendFunc_R_BlendLightmaps);
	WriteByte(0x3001B695,0x90);


	//Disable setting blend_target in R_BlendLightmaps, we do this now.
	WriteByte(0x30015584,0x90);
	WriteByte(0x30015585,0x90);
	WriteByte(0x30015586,0x90);
	WriteByte(0x30015587,0x90);
	WriteByte(0x30015588,0x90);
	WriteByte(0x30015589,0x90);
	WriteByte(0x3001558A,0x90);
	WriteByte(0x3001558B,0x90);
	WriteByte(0x3001558C,0x90);
	WriteByte(0x3001558D,0x90);
	WriteByte(0x3001558E,0x90);
}

/*
	The cvars have to map to unique features.
	If they are shortcuts for each other, then order matters, and then they can't sit in config.cfg together.
*/

void whiteraven_change(cvar_t* cvar) {
// deprecated implementation.
#if 0
	if (cvar->value) {
		//on
		orig_Cvar_Set2("_sofbuddy_lightblend_src","GL_DST_COLOR",false);
		orig_Cvar_Set2("_sofbuddy_lightblend_dst","GL_SRC_COLOR",false);
	} else {
		//off - default sof
		orig_Cvar_Set2("_sofbuddy_lightblend_src","GL_ZERO",false);
		orig_Cvar_Set2("_sofbuddy_lightblend_dst","GL_SRC_COLOR",false);
	}
#endif
}
#endif

void my_GL_TextureMode(char * mode) {
	orig_Com_Printf("CALLLING GL_TEXTUREMODE %i\n",minfilter_mipped);
	orig_GL_TextureMode(mode);
}

/*
	R_Init->GL_SetDefaultState() is handling texturemode for us, yet we are after that.
	So this patch is late.
	Thus we want to call it again, from R_beginFrame()

	Before, gl_texturemode would only affect:
	  sky/detailtextures(unmipped) using MAG_FILTER(gl_texturemode) for both min and mag
	  normal in-game mipped textures using MIN_FILTER(gl_texturemode) and MAG_FILTER(gl_texturemode)

	UI was untouched, hardcode to NEAREST/NEAREST.
	Considering defaulting the UI to GL_LINEAR for MAG. For re-scale HUD aesthetics.

	gl_texturemode values:
	==GL_NEAREST==
	  MIN: GL_NEAREST
	  MAG: GL_NEAREST
	==GL_LINEAR==
	  MIN: GL_LINEAR
	  MAG: GL_LINEAR
	==GL_NEAREST_MIPMAP_NEAREST==
	  MIN: GL_NEAREST_MIPMAP_NEAREST
	  MAG: GL_NEAREST
	==GL_LINEAR_MIPMAP_NEAREST==
	  MIN: GL_LINEAR_MIPMAP_NEAREST 
	  MAG: GL_LINEAR
	==GL_NEAREST_MIPMAP_LINEAR==
	  MIN: GL_NEAREST_MIPMAP_LINEAR
	  MAG: GL_NEAREST
	==GL_LINEAR_MIPMAP_LINEAR==
	  MIN: GL_LINEAR_MIPMAP_LINEAR
	  MAG: GL_LINEAR


  	So:
  		MAG can be:
  			GL_NEAREST
  			GL_LINEAR
  			( can't use mipmaps ).
  		MIN can be:
  			GL_NEAREST_MIPMAP_NEAREST
  				Chooses the mipmap that most closely matches the size of the pixel being textured and uses the GL_NEAREST criterion
  				(the texture element nearest to the center of the pixel) to produce a texture value.
  			GL_NEAREST_MIPMAP_LINEAR
  				Chooses the two mipmaps that most closely match the size of the pixel being textured and uses the GL_NEAREST criterion
  				(the texture element nearest to the center of the pixel) to produce a texture value from each mipmap.
  				The final texture value is a weighted average of those two values.
  			GL_LINEAR_MIPMAP_NEAREST
  				Chooses the mipmap that most closely matches the size of the pixel being textured and uses the GL_LINEAR criterion
  				(a weighted average of the four texture elements that are closest to the center of the pixel) to produce a texture value.
  			GL_LINEAR_MIPMAP_LINEAR
				Chooses the two mipmaps that most closely match the size of the pixel being textured and uses the GL_LINEAR criterion
				(a weighted average of the four texture elements that are closest to the center of the pixel) to produce a texture value from each mipmap.
				The final texture value is a weighted average of those two values.

	//how many mipmaps to sample
	GL_SOMETHING_MIPMAP_LINEAR = pick 2 mipmap that most closely matches size. and take the weighted average between them.
	GL_SOMETHING_MIPMAP_NEAREST = pick 1 mipmap that most closely matches size.

	//texel selection mechanism
	GL_NEAREST_MIPMAP_SOMETHING = the texel nearest to the center of the pixel
	GL_LINEAR_MIPMAP_SOMETHING = a weighted average of the four texels that are closest to the center of the pixel


	MIN == DISTANT ( zoomed out )
	MAG == SUPER CLOSE ( zoomed in )

	There are six defined minifying functions. 
	Two of them use the nearest one or nearest four texture elements to compute the texture value.
	The other four use mipmaps.

*/
void setup_minmag_filters(void) {
	static bool first_run = true;

	// orig_GL_TextureMode = DetourCreate(0x300066D0,&my_GL_TextureMode,DETOUR_TYPE_JMP,5);

	//Not necessary to specify flags, because they get stacked, and can't be removed.
	//This is not used anymore, just need it to trigger.
	_gl_texturemode = orig_Cvar_Get("gl_texturemode","GL_LINEAR_MIPMAP_LINEAR",CVAR_ARCHIVE,NULL); 

	// This overpowers sofplus _sp_cl_vid_gl_texture_mag_filter.
	// Could later allow his cvar to be used for mag_ui, maybe.
	orig_glTexParameterf = *(int*)0x300A457C;

	//Setup the glParameterf hooks for each individual one.
	WriteE8Call(0x30006636 ,&orig_glTexParameterf_min_mipped);
	WriteByte(0x3000663B,0x90);

	WriteE8Call(0x30006660 ,&orig_glTexParameterf_mag_mipped);
	WriteByte(0x30006665,0x90);

	WriteE8Call(0x300065DF ,&orig_glTexParameterf_min_unmipped);
	WriteByte(0x300065E4,0x90);

	WriteE8Call(0x30006609 ,&orig_glTexParameterf_mag_unmipped);
	WriteByte(0x3000660E,0x90);

	WriteE8Call(0x3000659C ,&orig_glTexParameterf_min_ui);
	WriteByte(0x300065A1,0x90);

	WriteE8Call(0x300065B1 ,&orig_glTexParameterf_mag_ui);
	WriteByte(0x300065B6,0x90);

	
	// orig_Com_Printf("Triggering glTextureMOde!!\n");
	//Trigger re-apply of texturemode
	if (_gl_texturemode) _gl_texturemode->modified = true;


	//==init== code for gl_swapinterval fix
	gl_swapinterval = orig_Cvar_Get("gl_swapinterval","0",NULL,NULL);
	vid_ref = orig_Cvar_Get("vid_ref","gl",NULL,NULL);

}

int my_R_SetMode(void * deviceMode) {
	// orig_Com_Printf("R_SETMODE_R_SETMODE\n");
	int ret = orig_R_SetMode(deviceMode);

	#ifdef FEATURE_FONT_SCALING
	if (ret) {
		my_Con_CheckResize();
		// orig_Com_Printf("fontscale is : %i %i\n",*(int*)0x2024AF98,*(int*)0x2040365C);
	}
	#endif
	
	return ret;
}
HMODULE (__stdcall *orig_LoadLibraryA)(LPCSTR lpLibFileName) = *(unsigned int*)0x20111178;

/*
	Direct LoadLibrary replacement.
	
	RefInMemory is a LoadLibrary() in-place Detour, for ref_gl.dll

	Allows to modify ref_gl.dll at before R_Init() returns.
*/
HMODULE __stdcall RefInMemory(LPCSTR lpLibFileName)
{
	HMODULE ret = orig_LoadLibraryA(lpLibFileName);
	if (ret) {
		/*
			Get the exact moment that hardware-changed new setup files are exec'ed (cpu_ vid_card different to config.cfg)
		*/
		WriteE8Call(0x3000FA26,&InitDefaults);
		WriteByte(0x3000FA2B,0x90);	
	}
	
	return ret;
}

/*
	After R_Init()
*/
void on_ref_init(void)
{

	setup_minmag_filters();

#ifdef FEATURE_HD_TEX
	hd_fix_init();
#endif
#ifdef FEATURE_TEAMICON_FIX
	teamicon_fix_init();
#endif

#ifdef FEATURE_ALT_LIGHTING
	lighting_fix_init();
#endif

#ifdef FEATURE_FONT_SCALING
	scaledFont_init();
#endif
}

/*
alias defaultlight "_sofbuddy_lightblend_src GL_ZERO;_sofbuddy_lightblend_dst GL_SRC_COLOR"
alias ravenlight "_sofbuddy_lightblend_src GL_DST_COLOR;_sofbuddy_lightblend_dst GL_SRC_COLOR"

*/

//void (*orig_CheckComplexStates)(void) = 0x3001B650;
//void (*orig_HandleFlowing)(void * surf,int * scroll_x, int * scroll_y) = 0x300127D0;
//void (*orig_GL_MBind2)(int target,int texnum) = 0x30006350;

/*
	SoF uses glBlendFunc(GL_ZERO,GL_SRC_COLOR) in R_BlendLightMaps().
	Result = S(Light) x (0,0,0) + D(Texture) x (Sr,Sg,Sb)
		== Texture x Light.

	Experiments showing the max values it creates which relates to clamping.

	SoF uses glBlendFunc(GL_ZERO,GL_SRC_COLOR) in R_BlendLightMaps().
	Result = S(Light) x (0,0,0) + D(Texture) x (Sr,Sg,Sb)

	Light=0.5, Tex=0.5
	GL_ZERO,GL_SRC_COLOR == GL_DST_COLOR,GL_ZERO == Tex*Light (SOF DEFAULT) == 0->1
	GL_ONE,GL_SRC_COLOR == Light+(Tex*Light) == 0->2 (Favours Light)
	GL_DST_COLOR,GL_ONE == (Tex*Light)+Tex==0->2 (Favours Tex)
	GL_SRC_COLOR,GL_DST_COLOR == Light^2+Tex^2 ==0->2 (DARKENS)

	TLDR:Raven's solution doubles saturation of every pixel ( Lightmap AND texture ).
	==chose this one: multiplies by 2, then clamp to 1.: so any combination of texture/lighting that has satutratiton greater > 0.5 when multiplied together, gets clamped to full saturation. So range of saturation becomes 0-0.5.. Max saturation is applied at 0.5, anything higher becomes 0.5 too. Mapping from 0-0.5 to 0-1, everything is doubled in saturation.====
	GL_DST_COLOR,GL_SRC_COLOR == 2*Light*Tex==0->2 (LIGHTENS)


	GL_ZERO,GL_ZERO == 0

	==This one looks washed out, why?, ah because after clamp it removes the texture, and not the light==
	GL_ONE,GL_ONE == Light+Tex == 0->2 (LIGHTENS)

	GL_ONE,GL_ZERO == Light == 0->1
	GL_ZERO,GL_ONE == Tex == 0->1
	GL_DST_COLOR,GL_DST_COLOR == Light*Tex+Tex^2 = 0->2 (Favours Tex)
	GL_SRC_COLOR,GL_SRC_COLOR == Light^2+Light*Tex = 0->2 (Favours Light)
	GL_SRC_ALPHA,GL_SRC_COLOR == Light*1+Light*Tex = 0->2 (Favours Light, darker when transparent,)
	GL_SRC_COLOR,GL_SRC_ALPHA == Light^2+Light*1= 0->2 (Favours Light)
	GL_DST_ALPHA,GL_DST_COLOR == Light*1+Tex^2 = 0->2 ( Favours Light)
	GL_DST_COLOR,GL_DST_ALPHA == Light*Tex+1*Tex = 0->2 (Favours Tex)
	GL_DST_ALPHA,GL_SRC_COLOR == Light*1+Tex*Light = 0->2 (Favours Light)

	==============
	gl_modulate explodes the lightmaps, but this high value just gets clamped to 255. 
	Thus making the max light strength lower, since lower light strengths become the max. 
	aka:
	highlight clipping
	Blown Highlights
	Overexposure
	Burnout
	Washed-Out Highlights
	Overbrights
	White-Out
	==============

//default
glBlendFunc (GL_ZERO, GL_SRC_COLOR );
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

//magicraven
glBlendFunc (GL_DST_COLOR, GL_SRC_COLOR );
	// First texture unit (handles destination color)
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);  // Destination color
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);  // Source color
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);


*/
void my_GL_RenderLightmappedPoly_intercept(void * msurface_t, void * dummy) {
	// asm volatile(
    //     "push %ebp\n"
    // );

	orig_Com_Printf("Huh??");
	// glBegin(0x06);
	/*
	void * image_t = *(int*)(msurface_t+0x2C);
	int texnum = *(int*)(image_t+0x50);
	int lightmaptexturenum = *(int*)(msurface_t+0x4C);
	int lightmaptextures = *(int*)0x3008FFD0;
	void * mtexinfo_t = *(int*)(msurface_t+0x34);
	int tex_flags = *(int*)(mtexinfo_t+0x20);
	void * glpoly_t = *(int*)(msurface_t+0x28);
	int num_verts = *(int*)(glpoly_t+0x4);
	float * verts = *(float**)(glpoly_t+0x8);

	GL_MBind2(0x84C0,texnum);
	GL_MBind2(0x84C1,lightmaptextures+lightmaptexturenum);
	CheckComplexStates();
	glBegin(0x06); //GL_TRIANGLE_FAN
	int scroll_x = 0;
	int scroll_y = 0;
	// SURF_FLOWING
	if (tex_flags & 0x40) {
		HandleFlowing(msurface_t,&scroll_x,&scroll_y);
	}
	
	float * v = verts;
	for ( int i=0; i<num_verts;i++,v+=7 ) {
		glMTexCoord2fSGIS(0x84C0,v[3]-scroll_x,v[4]-scroll_y);
		glMTexCoord2fvSGIS(0x84C1,&v[5]);
		glVertex3fv(v);
	}
*/
	// asm volatile(
	//     "pop %ebp\n"    // Restore EAX
	// );
}

/*
ref_gl/gl_local.h
typedef struct image_s
{
	char	name[MAX_QPATH];			// game path, including extension
	imagetype_t	type;
	int		width, height;				// source image
	int		upload_width, upload_height;	// after power of two and picmip
	int		registration_sequence;		// 0 = free
	struct msurface_s	*texturechain;	// for sort-by-texture world drawing
	int		texnum;						// gl texture binding
	float	sl, tl, sh, th;				// 0,0 - 1,1 unless part of the scrap
	qboolean	scrap;
	qboolean	has_alpha;

	qboolean paletted;
} image_t;
*/
void my_GL_BuildPolygonFromSurface(void *msurface_s) {
	//orig_Com_Printf("my_GL_BuildPolygonFromSurface\n");
	unsigned int *mtexinfo_t = (unsigned int*)(msurface_s+0x34);
	unsigned int *image_s = *mtexinfo_t+0x2C;
	unsigned short * img_w = *image_s+0x44;
	unsigned short * img_h = *image_s+0x46;

	char* name = *image_s;

	//surface texture name to lowercase
	std::string texName = name;
	for(auto& elem : texName)
		elem = std::tolower(elem);

	//texName exists in default_textures unordered_map
	if (default_textures.count(texName) > 0) {
		*img_w = default_textures[texName].w;
		*img_h = default_textures[texName].h;
	}

	orig_GL_BuildPolygonFromSurface(msurface_s);
}


#ifdef FEATURE_ALT_LIGHTING

// _sofbuddy_whiteraven_lighting


bool is_blending = false;
//Maybe blend is already active before calling R_BlendLightmaps (Nope, it is not.)
void my_R_BlendLightmaps(void) {
	// orig_Com_Printf("ComplexState is %i\n",((*(int*)0x300A46E0) & 0x01));

	//required to trigger change mb?
	if ( _sofbuddy_whiteraven_lighting->value == 1.0f ) {
		*lightblend_target_src = GL_DST_COLOR;
		*lightblend_target_dst = GL_SRC_COLOR;
	} else {
		*lightblend_target_src = lightblend_src;
		*lightblend_target_dst = lightblend_dst;	
	}
	

	is_blending = true;
	// Calls CheckComplexStates, which calls glBlendFunc
	orig_R_BlendLightmaps();
	is_blending = false;
}



// src = Incoming pixel Data (Light Data)
// dest = Pixel Data in color Buffer (Texture Data)
void __stdcall glBlendFunc_R_BlendLightmaps(unsigned int sfactor,unsigned int dfactor)
{

	// Multiply Blend Mode.
	// real_glBlendFunc(GL_DST_COLOR,GL_ONE_MINUS_SRC_ALPHA);
	// Default
	// real_glBlendFunc(GL_ZERO,GL_SRC_COLOR);
	if ( is_blending ) { 
		if ( _sofbuddy_whiteraven_lighting->value == 1.0f ) {
			real_glBlendFunc(GL_DST_COLOR,GL_SRC_COLOR);
		} else {
			real_glBlendFunc(lightblend_src,lightblend_dst);
		}
	}
	else {
		real_glBlendFunc(sfactor,dfactor);
	}
}
#endif
/*

q2 : 
	fov =  atan( height / ( width/tan(fov_x/360*M_PI))) * 360/M_PI

1/tan(degToRad(fovX)*0.5) * fovX / fovX

multiplier  = fovX/tan(fovX*0.5) /fovX / fovX

multplierx = fixfovx/fovx

multiplioer = fovx/ratio/fovx
multiplier = fovx/(ratio*fovx)

fovx = multipler * (ratio*fovx)

eg. 1920/(distance to screen)

1/tan(degToRad(fovY)*0.5) * fovY / fovY
*/
void my_drawTeamIcons(float * targetPlayerOrigin,char * playerName,char * imageNameTeamIcon,int redOrBlue)
{

	float fovY = *(float*)0x3008FD00;
	float fovX = *(float*)0x3008FCFC;

	fovfix_x = fovX/tan(degToRad(fovX*0.5));
	fovfix_y = fovY/tan(degToRad(fovY*0.5));

	float diagonalFov = fovX*1.4142f;

	teamviewFovAngle = diagonalFov;

	orig_drawTeamIcons(targetPlayerOrigin,playerName,imageNameTeamIcon,redOrBlue);
}

int TeamIconInterceptFix(void)
{

	// @0x30003186
	// ST0 contains HalfHeight - Lots of Shit
	// Lets Add the value to make it work
	// virtualHeight to RealHeight
	// (realHeight - virtualHeight) * 0.5
	float thedata;
	asm volatile(	"fstp %0;"
					::"m"(thedata):);
	unsigned int realHeight = *(unsigned int*)0x3008FFC8;
	unsigned int virtualHeight = *(unsigned int*)0x3008FCF8;

	float HalfHeight = thedata + (realHeight - virtualHeight) * 0.5;

	if ( HalfHeight < *(int*)(0x20225ED4) ) return -20;
	if ( HalfHeight > (*(int*)(0x20225ED4) + *(int*)(0x20225EDC) ) ) return -20;

	return HalfHeight;
}

/*
the code takes the middle of screen
uses it as a starting point for math
then adds an offset to it
to get the location of the teamicon
the offset value is a byproduct of the fov, so it needs the (viewport height)
the starting point is a byproduct of the entire screen somehow, so it needs the window height
the default code uses viewport height for both
i had to seperate it

it takes half the height, as a starting location
it tries to get center of the window
but .. if you take half of the viewport, when its squahsed
and add that to edge of window
it doesnt take you to center of window
thats the bug
get it ?

@ctrl the TeamIcon are drawn 2d, so can draw outside the 3d viewport.  The positioning of the teamicon is using variable "halfHeight" to get use center of screen as origin to the relative positioning of teamicon.  When viewport is smaller vertically than window in gl_mode 3 @ fov111 , halfHeight is no longer center of screen.  So made the code use Half of Window Height properly.  Since the 2d teamicon is free to draw outside the viewport in gaps above and below.. had to put in a valid check if its off viewport co or dinates..  Also found the default draw only within fov == 95 hard coded ( it uses cl.frame sent from server fov of 95 ) .. modified specific location where its using this to calculate if teamicon within fov and adjusted to 1.4x fovX.  The code by default uses 95 in all directions, thats why its fine Vertically usually but teamcion not show on wide fov.. So changing thsi to 1.4x fovX meant its always visible. (1.4 is just number to ensure it reaches diagonal distance of screen to all corners).

@ctrl *if its off viewport co-ordinates i just set its position to be -20 .. so its not seen (drawn completley offscreen)
@ctrl to fix the showing top and bottom of screen in gl_mode 3 111 fov issue
*/
void initDefaultTexSizes(void)
{
default_textures["textures/armory/1_2_subfloor1"] = {64,64};
default_textures["textures/armory/1_bathroom_men"] = {64,64};
default_textures["textures/armory/1_bath_towels"] = {64,128};
default_textures["textures/armory/1_beam1"] = {64,32};
default_textures["textures/armory/1_beam3"] = {64,32};
default_textures["textures/armory/1_beam4"] = {64,32};
default_textures["textures/armory/1_black"] = {16,16};
default_textures["textures/armory/1_build_win"] = {64,64};
default_textures["textures/armory/1_clock2"] = {64,64};
default_textures["textures/armory/1_concrete_floor"] = {128,128};
default_textures["textures/armory/1_concrete_floor2"] = {128,128};
default_textures["textures/armory/1_duct2"] = {64,64};
default_textures["textures/armory/1_dumpbck"] = {128,64};
default_textures["textures/armory/1_grating"] = {64,64};
default_textures["textures/armory/1_lobby3"] = {64,64};
default_textures["textures/armory/1_mtrim1"] = {64,32};
default_textures["textures/armory/1_poster2"] = {64,64};
default_textures["textures/armory/1_ramp_wall1_top"] = {64,32};
default_textures["textures/armory/1_sidewalk3"] = {64,64};
default_textures["textures/armory/1_soda_front_a"] = {64,64};
default_textures["textures/armory/1_soda_front_a3"] = {64,64};
default_textures["textures/armory/1_soda_front_b"] = {64,32};
default_textures["textures/armory/1_soda_front_b3"] = {64,32};
default_textures["textures/armory/1_soda_side_a"] = {64,64};
default_textures["textures/armory/1_soda_side_ad"] = {64,64};
default_textures["textures/armory/1_soda_side_b"] = {64,32};
default_textures["textures/armory/1_soda_side_bd"] = {64,32};
default_textures["textures/armory/1_soda_top"] = {64,64};
default_textures["textures/armory/1_tile"] = {64,64};
default_textures["textures/armory/1_tile_big_blue"] = {32,32};
default_textures["textures/armory/1_tile_big_green"] = {32,32};
default_textures["textures/armory/1_tile_bottom"] = {32,32};
default_textures["textures/armory/1_tunnelfloor_1"] = {32,32};
default_textures["textures/armory/1_tunnelfloor_2"] = {32,32};
default_textures["textures/armory/1_tunneltrack_plain"] = {64,64};
default_textures["textures/armory/1_tunnel_ceiling"] = {64,64};
default_textures["textures/armory/2_boiler_side"] = {64,64};
default_textures["textures/armory/2_brick5"] = {64,64};
default_textures["textures/armory/2_brick5bot"] = {64,64};
default_textures["textures/armory/2_cinderblock_bottom"] = {64,64};
default_textures["textures/armory/2_conwall1bot"] = {64,32};
default_textures["textures/armory/2_elevcar_roof"] = {64,64};
default_textures["textures/armory/2_mortar"] = {32,32};
default_textures["textures/armory/2_oldceiling1"] = {64,64};
default_textures["textures/armory/2_oldceiling1a"] = {64,64};
default_textures["textures/armory/2_oldceiling1ab"] = {64,64};
default_textures["textures/armory/2_oldceiling2"] = {64,32};
default_textures["textures/armory/2_oldwall2"] = {64,64};
default_textures["textures/armory/2_oldwall2b"] = {64,64};
default_textures["textures/armory/2_oldwall2c"] = {64,64};
default_textures["textures/armory/2_open_box"] = {64,64};
default_textures["textures/armory/2_open_box2"] = {64,64};
default_textures["textures/armory/2_open_box2d"] = {64,64};
default_textures["textures/armory/2_open_box3"] = {64,64};
default_textures["textures/armory/2_pipe_int2"] = {64,64};
default_textures["textures/armory/2_scumconcrete"] = {64,64};
default_textures["textures/armory/2_stair_side1"] = {16,16};
default_textures["textures/armory/2_subwood"] = {64,64};
default_textures["textures/armory/3_apt"] = {64,64};
default_textures["textures/armory/3_apt_bottom1"] = {64,64};
default_textures["textures/armory/3_apt_bottom3"] = {64,64};
default_textures["textures/armory/3_apt_window"] = {64,64};
default_textures["textures/armory/3_bar"] = {32,32};
default_textures["textures/armory/3_bar2"] = {64,64};
default_textures["textures/armory/3_bar_bottom1"] = {32,64};
default_textures["textures/armory/3_bar_window"] = {64,64};
default_textures["textures/armory/3_books3"] = {32,128};
default_textures["textures/armory/3_boxplain"] = {32,32};
default_textures["textures/armory/3_building1a"] = {128,32};
default_textures["textures/armory/3_building1b"] = {128,32};
default_textures["textures/armory/3_cement"] = {64,64};
default_textures["textures/armory/3_cobble"] = {64,64};
default_textures["textures/armory/3_door3"] = {64,128};
default_textures["textures/armory/3_door4"] = {64,128};
default_textures["textures/armory/3_door5"] = {64,128};
default_textures["textures/armory/3_door_blocked3"] = {64,128};
default_textures["textures/armory/3_flatwood"] = {64,64};
default_textures["textures/armory/3_grocery1"] = {64,64};
default_textures["textures/armory/3_grocery1_window"] = {64,64};
default_textures["textures/armory/3_grocery1_window_wide"] = {128,64};
default_textures["textures/armory/3_grocery1_window_wide_l"] = {128,64};
default_textures["textures/armory/3_grocery2"] = {64,64};
default_textures["textures/armory/3_grocery2_bottom"] = {64,64};
default_textures["textures/armory/3_grocery2_bottom_g1"] = {64,64};
default_textures["textures/armory/3_grocery2_bottom_g2"] = {128,64};
default_textures["textures/armory/3_market1"] = {64,64};
default_textures["textures/armory/3_market1_bottom"] = {64,64};
default_textures["textures/armory/3_market1_top"] = {64,64};
default_textures["textures/armory/3_market_sign1"] = {64,64};
default_textures["textures/armory/3_market_sign2"] = {64,64};
default_textures["textures/armory/3_market_sign3"] = {64,64};
default_textures["textures/armory/3_market_window1"] = {64,64};
default_textures["textures/armory/3_medcrate1"] = {64,64};
default_textures["textures/armory/3_medcrate2"] = {64,64};
default_textures["textures/armory/3_metal1"] = {64,64};
default_textures["textures/armory/3_mrust3"] = {64,64};
default_textures["textures/armory/3_oldbrick"] = {64,64};
default_textures["textures/armory/3_orange2"] = {64,64};
default_textures["textures/armory/3_orange_bottom1"] = {64,64};
default_textures["textures/armory/3_redbrick"] = {64,64};
default_textures["textures/armory/3_redbrick_bottom1"] = {64,64};
default_textures["textures/armory/3_redbrick_trim"] = {16,16};
default_textures["textures/armory/3_redbrick_window_a"] = {64,64};
default_textures["textures/armory/3_redbrick_window_ad1"] = {64,64};
default_textures["textures/armory/3_redbrick_window_ad2"] = {64,64};
default_textures["textures/armory/3_redbrick_window_b"] = {64,64};
default_textures["textures/armory/3_store_white"] = {64,64};
default_textures["textures/armory/3_store_white_bottom"] = {64,64};
default_textures["textures/armory/3_street"] = {64,64};
default_textures["textures/armory/3_street_trans"] = {64,64};
default_textures["textures/armory/3_sweatfloor"] = {64,64};
default_textures["textures/armory/3_sweatwall"] = {64,64};
default_textures["textures/armory/3_sweatwall_bottom"] = {64,64};
default_textures["textures/armory/3_sweatwall_top"] = {64,64};
default_textures["textures/armory/3_sweatwall_top2"] = {64,64};
default_textures["textures/armory/3_sweat_ceil1"] = {64,64};
default_textures["textures/armory/3_tarpaper"] = {64,64};
default_textures["textures/armory/3_tarpaper_horiz"] = {64,64};
default_textures["textures/armory/3_wall2"] = {128,128};
default_textures["textures/armory/3_wall2_bottom"] = {128,128};
default_textures["textures/armory/3_wareceiling1"] = {128,128};
default_textures["textures/armory/3_woodfloor2"] = {128,128};
default_textures["textures/armory/alleybrick"] = {64,64};
default_textures["textures/armory/alleybrick_btm"] = {64,32};
default_textures["textures/armory/alleybrick_btm2"] = {64,32};
default_textures["textures/armory/alleybrick_g1"] = {64,64};
default_textures["textures/armory/alleybrick_g2"] = {64,64};
default_textures["textures/armory/alleybrick_g3"] = {64,64};
default_textures["textures/armory/alleybrick_g4"] = {64,64};
default_textures["textures/armory/alleybrick_g5"] = {64,64};
default_textures["textures/armory/alleybrick_g6"] = {64,64};
default_textures["textures/armory/alleybrick_sign1"] = {64,64};
default_textures["textures/armory/alleybrick_sign1a"] = {64,64};
default_textures["textures/armory/alleybrick_sign1b"] = {64,64};
default_textures["textures/armory/alleybrick_top"] = {64,32};
default_textures["textures/armory/a_bkstack"] = {32,64};
default_textures["textures/armory/a_bkstack_frt"] = {32,64};
default_textures["textures/armory/a_bkstack_side"] = {32,64};
default_textures["textures/armory/a_comwall3"] = {128,128};
default_textures["textures/armory/a_gunrack1"] = {64,128};
default_textures["textures/armory/a_gunrack2"] = {64,128};
default_textures["textures/armory/bars2"] = {64,64};
default_textures["textures/armory/big_pipe1"] = {64,64};
default_textures["textures/armory/bookcase1"] = {64,128};
default_textures["textures/armory/bookcase2"] = {64,128};
default_textures["textures/armory/bookshelf"] = {64,64};
default_textures["textures/armory/bookshelf2"] = {64,64};
default_textures["textures/armory/bullseye"] = {64,64};
default_textures["textures/armory/button2b"] = {32,32};
default_textures["textures/armory/button_1"] = {16,32};
default_textures["textures/armory/button_2"] = {16,32};
default_textures["textures/armory/calendar"] = {32,64};
default_textures["textures/armory/calendar2"] = {32,64};
default_textures["textures/armory/catwalk"] = {64,64};
default_textures["textures/armory/chain1"] = {16,64};
default_textures["textures/armory/chainlink"] = {32,32};
default_textures["textures/armory/clip"] = {64,64};
default_textures["textures/armory/comwall1"] = {128,128};
default_textures["textures/armory/comwall1a"] = {128,128};
default_textures["textures/armory/comwall2"] = {64,128};
default_textures["textures/armory/comwall2a"] = {64,128};
default_textures["textures/armory/con1"] = {64,64};
default_textures["textures/armory/con13"] = {128,128};
default_textures["textures/armory/con13finish"] = {128,128};
default_textures["textures/armory/con13start"] = {128,128};
default_textures["textures/armory/con14"] = {64,64};
default_textures["textures/armory/con15"] = {64,64};
default_textures["textures/armory/con1b"] = {32,64};
default_textures["textures/armory/con1_drain"] = {64,64};
default_textures["textures/armory/con2"] = {64,64};
default_textures["textures/armory/con3"] = {64,64};
default_textures["textures/armory/con4"] = {64,64};
default_textures["textures/armory/con5"] = {64,64};
default_textures["textures/armory/conceil2"] = {64,64};
default_textures["textures/armory/contrim1"] = {128,64};
default_textures["textures/armory/deskback1"] = {64,64};
default_textures["textures/armory/deskback2"] = {64,64};
default_textures["textures/armory/deskside"] = {64,64};
default_textures["textures/armory/desktop1"] = {64,64};
default_textures["textures/armory/desktop2"] = {64,64};
default_textures["textures/armory/desktop3"] = {64,64};
default_textures["textures/armory/door1"] = {64,128};
default_textures["textures/armory/door1a"] = {64,128};
default_textures["textures/armory/door_ext"] = {64,128};
default_textures["textures/armory/dryerfrtd"] = {64,64};
default_textures["textures/armory/dryerside"] = {64,64};
default_textures["textures/armory/dryertop"] = {64,64};
default_textures["textures/armory/dumpbck"] = {128,64};
default_textures["textures/armory/dumpfrt"] = {128,32};
default_textures["textures/armory/dumplid"] = {64,64};
default_textures["textures/armory/d_genericsmooth"] = {64,64};
default_textures["textures/armory/d_metalrough"] = {64,64};
default_textures["textures/armory/d_metalsmooth"] = {64,64};
default_textures["textures/armory/d_stoneholey"] = {64,64};
default_textures["textures/armory/d_stonerough"] = {128,128};
default_textures["textures/armory/d_stonesmooth"] = {64,64};
default_textures["textures/armory/d_woodrough"] = {64,64};
default_textures["textures/armory/d_woodrough2"] = {64,64};
default_textures["textures/armory/d_woodsmooth"] = {64,64};
default_textures["textures/armory/env1a"] = {256,256};
default_textures["textures/armory/env1b"] = {256,256};
default_textures["textures/armory/env1c"] = {256,256};
default_textures["textures/armory/foodstores1"] = {64,128};
default_textures["textures/armory/foodstores2"] = {64,128};
default_textures["textures/armory/girdwall2"] = {128,128};
default_textures["textures/armory/glass"] = {32,32};
default_textures["textures/armory/glass_new2d"] = {128,128};
default_textures["textures/armory/glass_new2damage"] = {128,128};
default_textures["textures/armory/glass_wire"] = {64,64};
default_textures["textures/armory/gunrack1"] = {64,128};
default_textures["textures/armory/hint"] = {64,64};
default_textures["textures/armory/hud_1"] = {128,128};
default_textures["textures/armory/hud_2"] = {128,128};
default_textures["textures/armory/hud_3"] = {128,128};
default_textures["textures/armory/hud_4"] = {128,128};
default_textures["textures/armory/hud_5"] = {128,128};
default_textures["textures/armory/hud_6"] = {128,128};
default_textures["textures/armory/hud_7"] = {128,128};
default_textures["textures/armory/hud_8"] = {128,128};
default_textures["textures/armory/h_cretewall_dirty"] = {64,64};
default_textures["textures/armory/h_cretewall_plain"] = {64,64};
default_textures["textures/armory/h_heli_land1"] = {64,64};
default_textures["textures/armory/light_bluey"] = {32,32};
default_textures["textures/armory/light_florescent"] = {16,16};
default_textures["textures/armory/light_square"] = {32,32};
default_textures["textures/armory/light_tube2"] = {64,16};
default_textures["textures/armory/light_yellow"] = {32,32};
default_textures["textures/armory/magshelf"] = {64,64};
default_textures["textures/armory/magshelf2"] = {64,64};
default_textures["textures/armory/magshelf3"] = {64,64};
default_textures["textures/armory/mattress"] = {64,64};
default_textures["textures/armory/matt_side"] = {32,32};
default_textures["textures/armory/metal2"] = {64,64};
default_textures["textures/armory/metalfloor1"] = {64,64};
default_textures["textures/armory/metalrib1"] = {32,32};
default_textures["textures/armory/metal_clean"] = {64,64};
default_textures["textures/armory/mirror"] = {64,64};
default_textures["textures/armory/newpipe1"] = {64,32};
default_textures["textures/armory/newpipe1hilight"] = {64,32};
default_textures["textures/armory/no_draw"] = {64,64};
default_textures["textures/armory/origin"] = {64,64};
default_textures["textures/armory/pawngoods"] = {64,64};
default_textures["textures/armory/pipe_end"] = {64,64};
default_textures["textures/armory/pipe_paint1a"] = {64,64};
default_textures["textures/armory/pop_bad1"] = {64,128};
default_textures["textures/armory/pop_bad2"] = {64,128};
default_textures["textures/armory/pop_bad3"] = {64,128};
default_textures["textures/armory/pop_bad4"] = {64,128};
default_textures["textures/armory/pop_hostage"] = {64,128};
default_textures["textures/armory/pop_npc1"] = {64,128};
default_textures["textures/armory/pop_npc2"] = {64,128};
default_textures["textures/armory/pop_npc5"] = {64,128};
default_textures["textures/armory/radiator"] = {64,32};
default_textures["textures/armory/rail"] = {16,16};
default_textures["textures/armory/rustyplain"] = {32,32};
default_textures["textures/armory/shop_wndw"] = {128,64};
default_textures["textures/armory/sidewalk"] = {64,64};
default_textures["textures/armory/sign13"] = {128,128};
default_textures["textures/armory/sign17"] = {64,128};
default_textures["textures/armory/sign5"] = {64,64};
default_textures["textures/armory/sign9"] = {64,64};
default_textures["textures/armory/skip"] = {64,64};
default_textures["textures/armory/sky"] = {64,64};
default_textures["textures/armory/sky2"] = {64,64};
default_textures["textures/armory/sofmag1"] = {32,32};
default_textures["textures/armory/sofmag2"] = {32,32};
default_textures["textures/armory/stair_side"] = {16,16};
default_textures["textures/armory/s_asphalt"] = {64,64};
default_textures["textures/armory/s_asphalt_hole"] = {64,64};
default_textures["textures/armory/s_curb"] = {16,128};
default_textures["textures/armory/s_sidewalk"] = {128,128};
default_textures["textures/armory/tire1"] = {64,32};
default_textures["textures/armory/tire2"] = {64,32};
default_textures["textures/armory/trigger"] = {64,64};
default_textures["textures/armory/vaultbeam"] = {32,32};
default_textures["textures/armory/vaultbeam2"] = {32,64};
default_textures["textures/armory/vaultbeam2a"] = {32,64};
default_textures["textures/armory/vaultbeam3"] = {32,64};
default_textures["textures/armory/vaultbeam_side"] = {32,32};
default_textures["textures/armory/vaultceiling2"] = {64,64};
default_textures["textures/armory/vaultdoor"] = {64,128};
default_textures["textures/armory/vaultfloor1"] = {64,64};
default_textures["textures/armory/vaultfloor2"] = {64,64};
default_textures["textures/armory/vaultmetal"] = {32,32};
default_textures["textures/armory/vaultwall1"] = {64,128};
default_textures["textures/armory/water"] = {128,128};
default_textures["textures/armory/window2"] = {64,64};
default_textures["textures/armory/window2_dirt"] = {64,64};
default_textures["textures/armory/wood1"] = {64,64};
default_textures["textures/armory/w_boxside"] = {64,64};
default_textures["textures/armory/w_boxtop"] = {64,64};
default_textures["textures/armory/w_gardoor3"] = {128,128};
default_textures["textures/armory/w_gardoor3_g1"] = {128,128};
default_textures["textures/armory/w_palette1"] = {64,64};
default_textures["textures/armory/w_palette2"] = {64,64};
default_textures["textures/armory/w_pallette3"] = {64,64};
default_textures["textures/armory/w_rustwall6"] = {64,64};
default_textures["textures/armory/w_semiback2"] = {64,128};
default_textures["textures/armory/w_semiside2"] = {64,128};
default_textures["textures/armory/w_shtmetal1"] = {64,64};
default_textures["textures/armory/w_win1_noalpha"] = {64,64};
default_textures["textures/bosnia/1_3_brickdark1"] = {128,128};
default_textures["textures/bosnia/1_3_brickgrey"] = {128,128};
default_textures["textures/bosnia/1_3_brickgreyb"] = {128,128};
default_textures["textures/bosnia/1_3_brickgreyc"] = {128,128};
default_textures["textures/bosnia/1_3_brickgrey_bot"] = {128,128};
default_textures["textures/bosnia/1_3_brickgrey_bota"] = {128,128};
default_textures["textures/bosnia/1_3_brickmed1"] = {128,128};
default_textures["textures/bosnia/1_3_brickred"] = {128,128};
default_textures["textures/bosnia/1_3_brickred2"] = {128,128};
default_textures["textures/bosnia/1_3_brickred3"] = {128,128};
default_textures["textures/bosnia/1_3_brickred_btm"] = {128,128};
default_textures["textures/bosnia/1_3_brickstreet"] = {128,128};
default_textures["textures/bosnia/1_3_bricktan1"] = {128,128};
default_textures["textures/bosnia/1_3_bricktan1b"] = {128,128};
default_textures["textures/bosnia/1_3_bricktan1d"] = {128,128};
default_textures["textures/bosnia/1_3_bricktan1_btm"] = {128,128};
default_textures["textures/bosnia/1_3_bricktan1_hole"] = {128,128};
default_textures["textures/bosnia/1_3_bricktan2"] = {128,128};
default_textures["textures/bosnia/1_3_bricktan3"] = {128,128};
default_textures["textures/bosnia/1_3_build1"] = {128,128};
default_textures["textures/bosnia/1_3_build1a"] = {128,128};
default_textures["textures/bosnia/1_3_build1_bot"] = {128,128};
default_textures["textures/bosnia/1_3_build1_bot2"] = {128,128};
default_textures["textures/bosnia/1_3_build1_bot3"] = {128,128};
default_textures["textures/bosnia/1_3_build1_win"] = {128,128};
default_textures["textures/bosnia/1_3_build2a"] = {128,128};
default_textures["textures/bosnia/1_3_build_win"] = {128,128};
default_textures["textures/bosnia/1_3_ceiling1"] = {64,64};
default_textures["textures/bosnia/1_3_ceiling2"] = {128,128};
default_textures["textures/bosnia/1_3_clock"] = {64,64};
default_textures["textures/bosnia/1_3_clockd"] = {64,64};
default_textures["textures/bosnia/1_3_cobble"] = {64,64};
default_textures["textures/bosnia/1_3_cobbledark"] = {128,128};
default_textures["textures/bosnia/1_3_cobblegrey"] = {128,128};
default_textures["textures/bosnia/1_3_cobble_edge1"] = {64,64};
default_textures["textures/bosnia/1_3_cobsmall"] = {64,64};
default_textures["textures/bosnia/1_3_column"] = {64,128};
default_textures["textures/bosnia/1_3_con2"] = {64,64};
default_textures["textures/bosnia/1_3_con3"] = {64,64};
default_textures["textures/bosnia/1_3_con4"] = {64,64};
default_textures["textures/bosnia/1_3_con5"] = {64,64};
default_textures["textures/bosnia/1_3_con5b"] = {32,64};
default_textures["textures/bosnia/1_3_con6"] = {128,128};
default_textures["textures/bosnia/1_3_confloor"] = {64,64};
default_textures["textures/bosnia/1_3_confloor_ceil"] = {64,64};
default_textures["textures/bosnia/1_3_confloor_crack"] = {64,64};
default_textures["textures/bosnia/1_3_crete1"] = {64,64};
default_textures["textures/bosnia/1_3_crete1_trans"] = {64,32};
default_textures["textures/bosnia/1_3_crete1_trans2"] = {64,32};
default_textures["textures/bosnia/1_3_crete2"] = {64,64};
default_textures["textures/bosnia/1_3_dirt"] = {128,128};
default_textures["textures/bosnia/1_3_door1"] = {64,128};
default_textures["textures/bosnia/1_3_door2"] = {64,128};
default_textures["textures/bosnia/1_3_door3"] = {64,128};
default_textures["textures/bosnia/1_3_door_blocked1"] = {64,128};
default_textures["textures/bosnia/1_3_flagstn"] = {128,128};
default_textures["textures/bosnia/1_3_flagstn3"] = {128,128};
default_textures["textures/bosnia/1_3_newbuild2a_btm"] = {128,64};
default_textures["textures/bosnia/1_3_newbuild2b"] = {128,128};
default_textures["textures/bosnia/1_3_newbuild2c"] = {128,128};
default_textures["textures/bosnia/1_3_newbuild2d"] = {128,128};
default_textures["textures/bosnia/1_3_oldbrick"] = {64,64};
default_textures["textures/bosnia/1_3_oldbrick_btm"] = {64,64};
default_textures["textures/bosnia/1_3_outlet"] = {16,32};
default_textures["textures/bosnia/1_3_outlet_d"] = {16,32};
default_textures["textures/bosnia/1_3_plast1"] = {128,128};
default_textures["textures/bosnia/1_3_plaster1"] = {128,128};
default_textures["textures/bosnia/1_3_plaster1b"] = {128,128};
default_textures["textures/bosnia/1_3_plaster1c"] = {128,128};
default_textures["textures/bosnia/1_3_plaster1d"] = {128,128};
default_textures["textures/bosnia/1_3_plaster1_blast"] = {128,128};
default_textures["textures/bosnia/1_3_plaster1_btm2"] = {128,128};
default_textures["textures/bosnia/1_3_plaster1_btm3"] = {128,128};
default_textures["textures/bosnia/1_3_plaster2"] = {128,128};
default_textures["textures/bosnia/1_3_plaster3"] = {128,128};
default_textures["textures/bosnia/1_3_plaster4"] = {128,128};
default_textures["textures/bosnia/1_3_plasty1"] = {128,128};
default_textures["textures/bosnia/1_3_plasty2"] = {128,128};
default_textures["textures/bosnia/1_3_plastya"] = {128,128};
default_textures["textures/bosnia/1_3_plastyb"] = {128,128};
default_textures["textures/bosnia/1_3_shingle"] = {64,64};
default_textures["textures/bosnia/1_3_shingle3a"] = {128,128};
default_textures["textures/bosnia/1_3_shingle4"] = {64,64};
default_textures["textures/bosnia/1_3_shingle5"] = {64,64};
default_textures["textures/bosnia/1_3_shingle6"] = {64,64};
default_textures["textures/bosnia/1_3_sidewalk"] = {128,128};
default_textures["textures/bosnia/1_3_stairside_wood"] = {32,16};
default_textures["textures/bosnia/1_3_stairslab"] = {64,64};
default_textures["textures/bosnia/1_3_stairtop_wood"] = {32,64};
default_textures["textures/bosnia/1_3_store_brick1"] = {64,64};
default_textures["textures/bosnia/1_3_store_brick1_bottom"] = {64,64};
default_textures["textures/bosnia/1_3_store_brick2"] = {64,64};
default_textures["textures/bosnia/1_3_store_brick2_bottom"] = {64,64};
default_textures["textures/bosnia/1_3_store_brick3"] = {64,64};
default_textures["textures/bosnia/1_3_store_brick3_bottom"] = {64,64};
default_textures["textures/bosnia/1_3_stucc1"] = {128,128};
default_textures["textures/bosnia/1_3_stucc2"] = {128,128};
default_textures["textures/bosnia/1_3_stucco1"] = {128,128};
default_textures["textures/bosnia/1_3_stucco2"] = {128,128};
default_textures["textures/bosnia/1_3_stuccodark"] = {128,128};
default_textures["textures/bosnia/1_3_stuccostrip1"] = {64,128};
default_textures["textures/bosnia/1_3_tank1"] = {128,128};
default_textures["textures/bosnia/1_3_tank2a"] = {128,128};
default_textures["textures/bosnia/1_3_tank2b"] = {128,128};
default_textures["textures/bosnia/1_3_trim_generic"] = {64,16};
default_textures["textures/bosnia/1_3_wgate"] = {128,128};
default_textures["textures/bosnia/1_3_woodalpha1"] = {64,64};
default_textures["textures/bosnia/1_3_woodfence"] = {64,128};
default_textures["textures/bosnia/1_3_woodplank2"] = {64,32};
default_textures["textures/bosnia/1_apt"] = {64,64};
default_textures["textures/bosnia/1_apt_bottom1"] = {64,64};
default_textures["textures/bosnia/1_apt_bottom3"] = {64,64};
default_textures["textures/bosnia/1_apt_bottom3_d"] = {64,64};
default_textures["textures/bosnia/1_apt_window"] = {64,64};
default_textures["textures/bosnia/1_apt_windowd"] = {64,64};
default_textures["textures/bosnia/1_garageceil1"] = {128,128};
default_textures["textures/bosnia/1_sweatwall"] = {64,64};
default_textures["textures/bosnia/1_sweatwall2"] = {64,64};
default_textures["textures/bosnia/1_sweatwall_bottom"] = {64,64};
default_textures["textures/bosnia/1_sweatwall_top"] = {64,64};
default_textures["textures/bosnia/1_sweat_ceil1"] = {64,64};
default_textures["textures/bosnia/1_trim_wood"] = {64,32};
default_textures["textures/bosnia/1_villa1"] = {64,64};
default_textures["textures/bosnia/1_woodfloor"] = {128,128};
default_textures["textures/bosnia/1_wtrim1"] = {64,32};
default_textures["textures/bosnia/2_bigstone"] = {128,128};
default_textures["textures/bosnia/2_brick2"] = {64,64};
default_textures["textures/bosnia/2_church_a2"] = {64,64};
default_textures["textures/bosnia/2_church_a3"] = {64,64};
default_textures["textures/bosnia/2_church_a5"] = {64,64};
default_textures["textures/bosnia/2_church_a6"] = {32,64};
default_textures["textures/bosnia/2_church_b2"] = {32,16};
default_textures["textures/bosnia/2_church_c"] = {32,32};
default_textures["textures/bosnia/2_church_d"] = {128,128};
default_textures["textures/bosnia/2_church_door"] = {64,128};
default_textures["textures/bosnia/2_church_f"] = {64,128};
default_textures["textures/bosnia/2_church_h"] = {64,64};
default_textures["textures/bosnia/2_concrete1"] = {64,64};
default_textures["textures/bosnia/2_concrete1b"] = {64,64};
default_textures["textures/bosnia/2_concrete2"] = {64,64};
default_textures["textures/bosnia/2_concrete3"] = {64,64};
default_textures["textures/bosnia/2_concrete3b"] = {64,64};
default_textures["textures/bosnia/2_conwall1"] = {64,64};
default_textures["textures/bosnia/2_conwall2"] = {64,64};
default_textures["textures/bosnia/2_conwall_pipe"] = {64,64};
default_textures["textures/bosnia/2_conwall_pipetop"] = {64,32};
default_textures["textures/bosnia/2_conwall_seam"] = {64,64};
default_textures["textures/bosnia/2_conwall_seambot"] = {64,32};
default_textures["textures/bosnia/2_conwall_seamtop"] = {64,32};
default_textures["textures/bosnia/2_crete_grna"] = {64,64};
default_textures["textures/bosnia/2_crete_grnb"] = {64,64};
default_textures["textures/bosnia/2_darkrock"] = {128,128};
default_textures["textures/bosnia/2_dirt_floor2"] = {64,64};
default_textures["textures/bosnia/2_dirt_floor3"] = {64,64};
default_textures["textures/bosnia/2_dirt_floor4"] = {128,128};
default_textures["textures/bosnia/2_floor"] = {128,128};
default_textures["textures/bosnia/2_floor_lip1"] = {64,64};
default_textures["textures/bosnia/2_floor_lip2"] = {64,64};
default_textures["textures/bosnia/2_gutter1"] = {32,64};
default_textures["textures/bosnia/2_gutter2"] = {32,64};
default_textures["textures/bosnia/2_gutter3"] = {32,64};
default_textures["textures/bosnia/2_light_red"] = {32,32};
default_textures["textures/bosnia/2_metpipe1"] = {128,128};
default_textures["textures/bosnia/2_metpipe2"] = {128,128};
default_textures["textures/bosnia/2_mildewwall2"] = {128,128};
default_textures["textures/bosnia/2_mrust1"] = {64,64};
default_textures["textures/bosnia/2_mtrim2"] = {64,32};
default_textures["textures/bosnia/2_pipe_rusty"] = {64,32};
default_textures["textures/bosnia/2_rockfloor"] = {128,128};
default_textures["textures/bosnia/2_rockfloorb"] = {128,128};
default_textures["textures/bosnia/2_rockwall1"] = {128,128};
default_textures["textures/bosnia/2_rockwall1b"] = {128,32};
default_textures["textures/bosnia/2_sewcon"] = {64,64};
default_textures["textures/bosnia/2_sewerbot"] = {128,128};
default_textures["textures/bosnia/2_sewerbrick"] = {128,128};
default_textures["textures/bosnia/2_sewerbrick3"] = {64,64};
default_textures["textures/bosnia/2_sewerbrick4"] = {64,64};
default_textures["textures/bosnia/2_sewerbrick4_lip"] = {64,64};
default_textures["textures/bosnia/2_sewermid"] = {128,128};
default_textures["textures/bosnia/2_sewerrock1"] = {128,128};
default_textures["textures/bosnia/2_sewerrock2"] = {128,128};
default_textures["textures/bosnia/2_sewerrock3"] = {128,128};
default_textures["textures/bosnia/2_sewerrock4"] = {128,128};
default_textures["textures/bosnia/2_sewertop"] = {128,128};
default_textures["textures/bosnia/2_stonewall"] = {64,64};
default_textures["textures/bosnia/2_stonewall2"] = {64,64};
default_textures["textures/bosnia/2_stonewall_bot"] = {64,32};
default_textures["textures/bosnia/2_support1"] = {64,128};
default_textures["textures/bosnia/2_support2"] = {64,32};
default_textures["textures/bosnia/2_swr_blk_md"] = {128,128};
default_textures["textures/bosnia/2_swr_blk_top"] = {128,128};
default_textures["textures/bosnia/2_trim1"] = {128,64};
default_textures["textures/bosnia/2_wall_lip"] = {64,64};
default_textures["textures/bosnia/3_bgunbar2"] = {256,64};
default_textures["textures/bosnia/3_bgunbar3"] = {256,64};
default_textures["textures/bosnia/3_bgunbar4"] = {256,32};
default_textures["textures/bosnia/3_bgunbar6"] = {128,128};
default_textures["textures/bosnia/3_bgunbar7"] = {128,128};
default_textures["textures/bosnia/3_bgunbar8"] = {128,128};
default_textures["textures/bosnia/3_bgunbarl1"] = {128,128};
default_textures["textures/bosnia/3_bgunmetal"] = {64,64};
default_textures["textures/bosnia/3_bgunmetal3"] = {64,64};
default_textures["textures/bosnia/3_bgunmetalr"] = {64,64};
default_textures["textures/bosnia/3_bguns1"] = {128,128};
default_textures["textures/bosnia/3_bguns2"] = {64,64};
default_textures["textures/bosnia/3_bguns3"] = {64,64};
default_textures["textures/bosnia/3_bguns3b"] = {64,64};
default_textures["textures/bosnia/3_bguns3d"] = {64,64};
default_textures["textures/bosnia/3_bguns4"] = {128,128};
default_textures["textures/bosnia/3_bguns5"] = {64,64};
default_textures["textures/bosnia/3_catwalk"] = {64,64};
default_textures["textures/bosnia/3_metalside"] = {64,64};
default_textures["textures/bosnia/3_metalside1"] = {64,64};
default_textures["textures/bosnia/3_metalside2"] = {16,16};
default_textures["textures/bosnia/3_mfloor"] = {64,64};
default_textures["textures/bosnia/3_paint_orange"] = {16,16};
default_textures["textures/bosnia/3_pipeopening"] = {64,64};
default_textures["textures/bosnia/3_stair_side"] = {16,16};
default_textures["textures/bosnia/3_stair_top"] = {32,64};
default_textures["textures/bosnia/3_storewall_burn"] = {128,128};
default_textures["textures/bosnia/3_storewall_burn_btm"] = {128,128};
default_textures["textures/bosnia/3_store_flr_burn2"] = {128,128};
default_textures["textures/bosnia/3_trim"] = {32,32};
default_textures["textures/bosnia/3_woodfence_alpha"] = {64,128};
default_textures["textures/bosnia/ammobox_end"] = {32,32};
default_textures["textures/bosnia/ammobox_side"] = {64,32};
default_textures["textures/bosnia/ammobox_top"] = {64,32};
default_textures["textures/bosnia/ammobox_topd"] = {64,32};
default_textures["textures/bosnia/arch1"] = {128,128};
default_textures["textures/bosnia/arch2b"] = {128,128};
default_textures["textures/bosnia/arch3"] = {128,128};
default_textures["textures/bosnia/arch4"] = {128,128};
default_textures["textures/bosnia/barbwire"] = {128,64};
default_textures["textures/bosnia/bars1"] = {64,64};
default_textures["textures/bosnia/black"] = {16,16};
default_textures["textures/bosnia/camonet1"] = {64,64};
default_textures["textures/bosnia/chainlink"] = {32,32};
default_textures["textures/bosnia/clip"] = {64,64};
default_textures["textures/bosnia/crate1"] = {64,64};
default_textures["textures/bosnia/crate2"] = {64,64};
default_textures["textures/bosnia/crate3"] = {64,64};
default_textures["textures/bosnia/crate4"] = {64,64};
default_textures["textures/bosnia/crate_logo"] = {64,64};
default_textures["textures/bosnia/crate_long"] = {64,32};
default_textures["textures/bosnia/crate_rough_logo"] = {64,64};
default_textures["textures/bosnia/crate_rough_mtl"] = {64,64};
default_textures["textures/bosnia/crate_rough_type"] = {64,64};
default_textures["textures/bosnia/ctfr_base"] = {64,64};
default_textures["textures/bosnia/dirt_brown"] = {128,128};
default_textures["textures/bosnia/dirt_grey"] = {128,128};
default_textures["textures/bosnia/d_genericsmooth"] = {64,64};
default_textures["textures/bosnia/d_metalrough"] = {64,64};
default_textures["textures/bosnia/d_metalsmooth"] = {64,64};
default_textures["textures/bosnia/d_sand"] = {64,64};
default_textures["textures/bosnia/d_stonecracked"] = {64,64};
default_textures["textures/bosnia/d_stoneholey"] = {64,64};
default_textures["textures/bosnia/d_stoneholey2"] = {64,64};
default_textures["textures/bosnia/d_stonerough"] = {128,128};
default_textures["textures/bosnia/d_stonesmooth"] = {64,64};
default_textures["textures/bosnia/d_stucco"] = {64,64};
default_textures["textures/bosnia/d_woodrough"] = {64,64};
default_textures["textures/bosnia/d_woodsmooth"] = {64,64};
default_textures["textures/bosnia/env1a"] = {256,256};
default_textures["textures/bosnia/env1b"] = {256,256};
default_textures["textures/bosnia/grate1"] = {64,64};
default_textures["textures/bosnia/gravel1"] = {128,128};
default_textures["textures/bosnia/hint"] = {64,64};
default_textures["textures/bosnia/ladder_rung1"] = {32,16};
default_textures["textures/bosnia/ladder_rung2"] = {32,16};
default_textures["textures/bosnia/light_tiny_green"] = {16,16};
default_textures["textures/bosnia/light_tiny_red"] = {16,16};
default_textures["textures/bosnia/light_tiny_tan"] = {16,16};
default_textures["textures/bosnia/light_tiny_white"] = {16,16};
default_textures["textures/bosnia/light_tiny_yellow"] = {16,16};
default_textures["textures/bosnia/light_tungsten"] = {32,32};
default_textures["textures/bosnia/light_yellow1"] = {32,32};
default_textures["textures/bosnia/light_yellow2"] = {32,32};
default_textures["textures/bosnia/metal_clean"] = {64,64};
default_textures["textures/bosnia/metal_cool"] = {32,32};
default_textures["textures/bosnia/metal_dark"] = {32,32};
default_textures["textures/bosnia/metal_rust"] = {64,64};
default_textures["textures/bosnia/meter1"] = {64,64};
default_textures["textures/bosnia/meter1d"] = {64,64};
default_textures["textures/bosnia/newbuild1d_btm"] = {128,64};
default_textures["textures/bosnia/newbuilda"] = {128,128};
default_textures["textures/bosnia/no_draw"] = {64,64};
default_textures["textures/bosnia/oldbrick"] = {64,64};
default_textures["textures/bosnia/oldwood2"] = {128,128};
default_textures["textures/bosnia/origin"] = {64,64};
default_textures["textures/bosnia/roada"] = {128,128};
default_textures["textures/bosnia/roadb"] = {128,128};
default_textures["textures/bosnia/roadc"] = {128,128};
default_textures["textures/bosnia/roadd"] = {128,128};
default_textures["textures/bosnia/roade"] = {128,128};
default_textures["textures/bosnia/roadf"] = {128,128};
default_textures["textures/bosnia/roadg"] = {128,128};
default_textures["textures/bosnia/roadh"] = {128,128};
default_textures["textures/bosnia/roadi"] = {128,128};
default_textures["textures/bosnia/roadj"] = {128,128};
default_textures["textures/bosnia/roadk"] = {128,128};
default_textures["textures/bosnia/roadm"] = {128,128};
default_textures["textures/bosnia/rockceiling"] = {128,128};
default_textures["textures/bosnia/sandbag128"] = {128,128};
default_textures["textures/bosnia/sandbag64"] = {64,64};
default_textures["textures/bosnia/sewerstn1a"] = {128,128};
default_textures["textures/bosnia/sewerstn1c"] = {128,128};
default_textures["textures/bosnia/sign-ckpoint"] = {128,64};
default_textures["textures/bosnia/sign-minefield"] = {64,64};
default_textures["textures/bosnia/sign-sniper"] = {64,64};
default_textures["textures/bosnia/sign2"] = {128,64};
default_textures["textures/bosnia/sign3"] = {128,128};
default_textures["textures/bosnia/sign_bank"] = {128,64};
default_textures["textures/bosnia/sign_busstation"] = {64,64};
default_textures["textures/bosnia/sign_pharmacy"] = {128,64};
default_textures["textures/bosnia/sign_policestation"] = {128,64};
default_textures["textures/bosnia/sign_postoffice"] = {128,64};
default_textures["textures/bosnia/sign_sniper2"] = {128,64};
default_textures["textures/bosnia/sign_square"] = {64,32};
default_textures["textures/bosnia/skip"] = {64,64};
default_textures["textures/bosnia/sky"] = {64,64};
default_textures["textures/bosnia/trigger"] = {64,64};
default_textures["textures/bosnia/water2"] = {128,128};
default_textures["textures/bosnia/water3"] = {128,128};
default_textures["textures/bosnia/water4"] = {128,128};
default_textures["textures/bosnia/water4b"] = {128,128};
default_textures["textures/bosnia/water4c"] = {128,128};
default_textures["textures/bosnia/window1_dirt"] = {64,64};
default_textures["textures/bosnia/window1_dirt_d"] = {64,64};
default_textures["textures/bosnia/window2_dirt"] = {64,64};
default_textures["textures/bosnia/window2_dirt_d"] = {64,64};
default_textures["textures/castle/1_black"] = {16,16};
default_textures["textures/castle/1_duct2"] = {64,64};
default_textures["textures/castle/1_grating"] = {64,64};
default_textures["textures/castle/1_hayfront"] = {64,64};
default_textures["textures/castle/1_hayside"] = {128,64};
default_textures["textures/castle/1_sidewalk3"] = {64,64};
default_textures["textures/castle/1_tunnelfloor_1"] = {32,32};
default_textures["textures/castle/2_elevcar_roof"] = {64,64};
default_textures["textures/castle/2_greenlight_off"] = {64,16};
default_textures["textures/castle/2_greenlight_on"] = {64,16};
default_textures["textures/castle/2_oldwall2"] = {64,64};
default_textures["textures/castle/2_redlight_off"] = {64,16};
default_textures["textures/castle/2_redlight_on"] = {64,16};
default_textures["textures/castle/3_ammo_end"] = {32,32};
default_textures["textures/castle/3_ammo_side"] = {64,32};
default_textures["textures/castle/3_ammo_top"] = {64,32};
default_textures["textures/castle/3_ammo_topd"] = {64,32};
default_textures["textures/castle/3_amocrate"] = {64,32};
default_textures["textures/castle/3_amocrate2"] = {32,32};
default_textures["textures/castle/3_blastdoor2"] = {128,128};
default_textures["textures/castle/3_blastdoorc"] = {64,64};
default_textures["textures/castle/3_blastdoord"] = {64,64};
default_textures["textures/castle/3_blastdoor_frame"] = {64,128};
default_textures["textures/castle/3_blastdoor_frame1"] = {16,16};
default_textures["textures/castle/3_boxplain"] = {32,32};
default_textures["textures/castle/3_button1_off"] = {32,32};
default_textures["textures/castle/3_button1_on"] = {32,32};
default_textures["textures/castle/3_camera1"] = {64,64};
default_textures["textures/castle/3_camera2"] = {64,64};
default_textures["textures/castle/3_camera3"] = {64,64};
default_textures["textures/castle/3_camera_d"] = {64,64};
default_textures["textures/castle/3_carpet"] = {64,64};
default_textures["textures/castle/3_carpet_2"] = {64,64};
default_textures["textures/castle/3_carpet_3"] = {64,64};
default_textures["textures/castle/3_carpet_4"] = {64,64};
default_textures["textures/castle/3_clamp"] = {64,64};
default_textures["textures/castle/3_clamp_1"] = {64,64};
default_textures["textures/castle/3_clamp_ext1"] = {32,16};
default_textures["textures/castle/3_clamp_ext2"] = {32,32};
default_textures["textures/castle/3_clamp_housing3"] = {64,64};
default_textures["textures/castle/3_clamp_int"] = {128,64};
default_textures["textures/castle/3_computer_1"] = {64,128};
default_textures["textures/castle/3_computer_1_d"] = {64,128};
default_textures["textures/castle/3_computer_2"] = {64,128};
default_textures["textures/castle/3_computer_2_d"] = {64,128};
default_textures["textures/castle/3_computer_3"] = {64,128};
default_textures["textures/castle/3_computer_3_d"] = {64,128};
default_textures["textures/castle/3_computer_side"] = {32,128};
default_textures["textures/castle/3_computer_top"] = {64,32};
default_textures["textures/castle/3_comp_door"] = {64,128};
default_textures["textures/castle/3_comp_door_d"] = {64,128};
default_textures["textures/castle/3_comp_side_dr"] = {32,128};
default_textures["textures/castle/3_comp_top_dr"] = {64,32};
default_textures["textures/castle/3_console1"] = {64,64};
default_textures["textures/castle/3_console1_d"] = {64,64};
default_textures["textures/castle/3_console2"] = {64,64};
default_textures["textures/castle/3_console2_d"] = {64,64};
default_textures["textures/castle/3_console3"] = {64,32};
default_textures["textures/castle/3_console3_d"] = {64,32};
default_textures["textures/castle/3_console4"] = {64,32};
default_textures["textures/castle/3_console4_d"] = {64,32};
default_textures["textures/castle/3_console5"] = {32,64};
default_textures["textures/castle/3_console5_d"] = {32,64};
default_textures["textures/castle/3_console6"] = {64,32};
default_textures["textures/castle/3_console6d"] = {64,32};
default_textures["textures/castle/3_console7"] = {64,64};
default_textures["textures/castle/3_console7_d"] = {64,64};
default_textures["textures/castle/3_crane_8"] = {64,64};
default_textures["textures/castle/3_crane_housing1"] = {64,64};
default_textures["textures/castle/3_crane_housing3"] = {64,64};
default_textures["textures/castle/3_crane_housing5"] = {64,64};
default_textures["textures/castle/3_crane_housing6"] = {64,64};
default_textures["textures/castle/3_crane_housing7"] = {64,64};
default_textures["textures/castle/3_crate1"] = {64,64};
default_textures["textures/castle/3_crate1a"] = {64,64};
default_textures["textures/castle/3_crate1_front"] = {128,64};
default_textures["textures/castle/3_crate2"] = {64,64};
default_textures["textures/castle/3_crate2a"] = {64,64};
default_textures["textures/castle/3_crate2_back"] = {128,64};
default_textures["textures/castle/3_crate2_front"] = {128,64};
default_textures["textures/castle/3_desk_end"] = {64,64};
default_textures["textures/castle/3_desk_middle"] = {64,64};
default_textures["textures/castle/3_desk_middle_1"] = {64,64};
default_textures["textures/castle/3_desk_middle_1_d"] = {64,64};
default_textures["textures/castle/3_desk_middle_2"] = {64,64};
default_textures["textures/castle/3_desk_middle_2d"] = {64,64};
default_textures["textures/castle/3_desk_middle_3"] = {64,64};
default_textures["textures/castle/3_desk_middle_3_d"] = {64,64};
default_textures["textures/castle/3_desk_middle_4"] = {64,64};
default_textures["textures/castle/3_door2"] = {64,128};
default_textures["textures/castle/3_door5"] = {64,128};
default_textures["textures/castle/3_door_alpha"] = {64,128};
default_textures["textures/castle/3_elevator_button"] = {32,64};
default_textures["textures/castle/3_elevator_button2"] = {32,64};
default_textures["textures/castle/3_elevator_button_down"] = {32,64};
default_textures["textures/castle/3_elevator_ceil"] = {64,64};
default_textures["textures/castle/3_elevator_door"] = {32,128};
default_textures["textures/castle/3_elevator_floor"] = {64,64};
default_textures["textures/castle/3_elevator_top"] = {64,64};
default_textures["textures/castle/3_elevator_wall"] = {64,64};
default_textures["textures/castle/3_gen_panel2"] = {64,64};
default_textures["textures/castle/3_gen_panel3"] = {64,64};
default_textures["textures/castle/3_gen_panel4"] = {64,64};
default_textures["textures/castle/3_gen_panel6"] = {64,64};
default_textures["textures/castle/3_gen_trim1"] = {64,32};
default_textures["textures/castle/3_gen_trim2"] = {64,32};
default_textures["textures/castle/3_glass1"] = {64,64};
default_textures["textures/castle/3_glass2"] = {64,64};
default_textures["textures/castle/3_glass3"] = {64,64};
default_textures["textures/castle/3_glass4"] = {64,64};
default_textures["textures/castle/3_grateflr"] = {64,64};
default_textures["textures/castle/3_hallarch"] = {128,128};
default_textures["textures/castle/3_hallarchtrim"] = {16,16};
default_textures["textures/castle/3_hall_blockwall"] = {64,64};
default_textures["textures/castle/3_hall_blockwall_b"] = {64,64};
default_textures["textures/castle/3_hall_blockwall_c"] = {64,64};
default_textures["textures/castle/3_hall_ceil1"] = {64,64};
default_textures["textures/castle/3_hall_floor_ceil"] = {64,64};
default_textures["textures/castle/3_hall_floor_ceil2"] = {64,64};
default_textures["textures/castle/3_handbutton"] = {64,64};
default_textures["textures/castle/3_ibeam"] = {64,32};
default_textures["textures/castle/3_ibeam_top"] = {64,32};
default_textures["textures/castle/3_machines_1"] = {64,64};
default_textures["textures/castle/3_machine_10"] = {64,64};
default_textures["textures/castle/3_machine_11"] = {64,64};
default_textures["textures/castle/3_machine_2"] = {64,64};
default_textures["textures/castle/3_machine_3"] = {64,64};
default_textures["textures/castle/3_machine_4"] = {64,64};
default_textures["textures/castle/3_machine_5"] = {64,64};
default_textures["textures/castle/3_machine_6"] = {64,64};
default_textures["textures/castle/3_machine_7"] = {64,64};
default_textures["textures/castle/3_machine_8"] = {64,64};
default_textures["textures/castle/3_machine_9"] = {64,64};
default_textures["textures/castle/3_missile_1"] = {64,64};
default_textures["textures/castle/3_missile_2"] = {64,64};
default_textures["textures/castle/3_missile_3"] = {64,64};
default_textures["textures/castle/3_missile_3_paint"] = {64,64};
default_textures["textures/castle/3_missile_4"] = {64,64};
default_textures["textures/castle/3_missile_4_paint"] = {64,64};
default_textures["textures/castle/3_missile_5"] = {64,64};
default_textures["textures/castle/3_missile_6"] = {64,256};
default_textures["textures/castle/3_monitor_1"] = {64,64};
default_textures["textures/castle/3_monitor_1a"] = {64,64};
default_textures["textures/castle/3_monitor_1a_d"] = {64,64};
default_textures["textures/castle/3_monitor_1_d"] = {64,64};
default_textures["textures/castle/3_monitor_2"] = {64,64};
default_textures["textures/castle/3_monitor_lg"] = {128,128};
default_textures["textures/castle/3_monitor_lg_d"] = {128,128};
default_textures["textures/castle/3_monitor_med_d"] = {64,64};
default_textures["textures/castle/3_monitor_side"] = {16,16};
default_textures["textures/castle/3_railing"] = {64,64};
default_textures["textures/castle/3_railingtop"] = {16,16};
default_textures["textures/castle/3_sheetmtl_vent"] = {64,64};
default_textures["textures/castle/3_silocover"] = {64,64};
default_textures["textures/castle/3_silo_wall"] = {128,128};
default_textures["textures/castle/3_smallbuildings_1"] = {32,32};
default_textures["textures/castle/3_smallbuildings_1_top"] = {32,32};
default_textures["textures/castle/3_smallbuildings_2"] = {32,32};
default_textures["textures/castle/3_smallbuildings_2_top"] = {32,32};
default_textures["textures/castle/3_soda_frnt_a"] = {64,64};
default_textures["textures/castle/3_soda_frnt_a_d"] = {64,64};
default_textures["textures/castle/3_soda_frnt_b"] = {64,32};
default_textures["textures/castle/3_soda_frnt_b_d"] = {64,32};
default_textures["textures/castle/3_soda_side_a"] = {64,64};
default_textures["textures/castle/3_soda_side_a_d"] = {64,64};
default_textures["textures/castle/3_soda_side_b"] = {64,32};
default_textures["textures/castle/3_switch_d"] = {64,32};
default_textures["textures/castle/3_switch_green_off"] = {64,32};
default_textures["textures/castle/3_switch_green_on"] = {64,32};
default_textures["textures/castle/3_switch_red_off"] = {64,32};
default_textures["textures/castle/3_target"] = {64,64};
default_textures["textures/castle/3_transmap"] = {128,128};
default_textures["textures/castle/3_wall2"] = {128,128};
default_textures["textures/castle/3_wall2_bottom"] = {128,128};
default_textures["textures/castle/3_wallgrid_long2"] = {64,128};
default_textures["textures/castle/3_wall_grid_long"] = {64,128};
default_textures["textures/castle/3_wall_panel"] = {64,64};
default_textures["textures/castle/3_wall_panela"] = {64,64};
default_textures["textures/castle/4_body1"] = {64,128};
default_textures["textures/castle/4_body1a"] = {64,128};
default_textures["textures/castle/4_body1c"] = {64,128};
default_textures["textures/castle/4_body1d_back"] = {256,64};
default_textures["textures/castle/4_body_sidetrim"] = {16,16};
default_textures["textures/castle/4_body_top1"] = {64,64};
default_textures["textures/castle/4_body_top1a_silos"] = {64,64};
default_textures["textures/castle/4_body_top1b"] = {64,64};
default_textures["textures/castle/4_contower_frt_bck"] = {32,64};
default_textures["textures/castle/4_contower_side1"] = {64,64};
default_textures["textures/castle/4_contower_side1a"] = {64,64};
default_textures["textures/castle/4_contower_top_end"] = {64,64};
default_textures["textures/castle/4_contower_top_mid"] = {64,64};
default_textures["textures/castle/4_periscope_etc"] = {16,16};
default_textures["textures/castle/archtrim"] = {32,16};
default_textures["textures/castle/arch_base"] = {128,128};
default_textures["textures/castle/arch_inside"] = {64,64};
default_textures["textures/castle/arch_top"] = {128,128};
default_textures["textures/castle/a_comwall3"] = {128,128};
default_textures["textures/castle/banner03"] = {32,64};
default_textures["textures/castle/banner03_alpha"] = {32,64};
default_textures["textures/castle/banner04"] = {32,64};
default_textures["textures/castle/banner_01"] = {64,128};
default_textures["textures/castle/banner_02"] = {64,64};
default_textures["textures/castle/bars1"] = {64,64};
default_textures["textures/castle/bars2"] = {64,64};
default_textures["textures/castle/bathroom2_tile"] = {64,64};
default_textures["textures/castle/bathroom2_tile_walla"] = {64,64};
default_textures["textures/castle/bathroom2_tile_wallb"] = {64,64};
default_textures["textures/castle/bathroom_tile"] = {64,64};
default_textures["textures/castle/bathroom_tile_walla"] = {64,64};
default_textures["textures/castle/bathroom_tile_wallb"] = {64,64};
default_textures["textures/castle/bed1"] = {64,128};
default_textures["textures/castle/bed2"] = {64,128};
default_textures["textures/castle/bed3"] = {128,128};
default_textures["textures/castle/bedwalla"] = {64,64};
default_textures["textures/castle/bedwallb"] = {64,64};
default_textures["textures/castle/bedwallc"] = {64,64};
default_textures["textures/castle/bedwalld"] = {64,64};
default_textures["textures/castle/bed_canopy"] = {32,16};
default_textures["textures/castle/bed_hall_wall2"] = {64,64};
default_textures["textures/castle/bed_hall_wall2a"] = {64,64};
default_textures["textures/castle/bed_hall_wallc"] = {64,64};
default_textures["textures/castle/black_raider_logob"] = {128,128};
default_textures["textures/castle/bookcase01"] = {64,64};
default_textures["textures/castle/bookcase02"] = {64,64};
default_textures["textures/castle/bookcase03"] = {64,64};
default_textures["textures/castle/bookcase06"] = {64,64};
default_textures["textures/castle/bookcase_trim"] = {32,32};
default_textures["textures/castle/boxside"] = {64,64};
default_textures["textures/castle/boxtop"] = {64,64};
default_textures["textures/castle/bulletinboard"] = {128,64};
default_textures["textures/castle/ceilingstone"] = {128,128};
default_textures["textures/castle/ceiling_roundhall"] = {128,128};
default_textures["textures/castle/cementwall1"] = {64,64};
default_textures["textures/castle/clip"] = {64,64};
default_textures["textures/castle/clock1"] = {32,32};
default_textures["textures/castle/clock2"] = {32,32};
default_textures["textures/castle/closet_hanger"] = {32,16};
default_textures["textures/castle/closet_hangertop"] = {16,8};
default_textures["textures/castle/closet_pants"] = {16,32};
default_textures["textures/castle/closet_pants_side"] = {16,32};
default_textures["textures/castle/closet_suit_front"] = {32,64};
default_textures["textures/castle/closet_suit_side"] = {32,32};
default_textures["textures/castle/cobble1"] = {64,64};
default_textures["textures/castle/cobblestone"] = {64,64};
default_textures["textures/castle/comwall1"] = {128,128};
default_textures["textures/castle/comwall1a"] = {128,128};
default_textures["textures/castle/comwall2"] = {64,128};
default_textures["textures/castle/con13"] = {128,128};
default_textures["textures/castle/con15"] = {64,64};
default_textures["textures/castle/con3"] = {64,64};
default_textures["textures/castle/ctfb_2_oldwall2"] = {64,64};
default_textures["textures/castle/ctfb_3_carpet"] = {64,64};
default_textures["textures/castle/ctfb_base"] = {64,64};
default_textures["textures/castle/ctfb_con13"] = {128,128};
default_textures["textures/castle/ctfb_stnwall9"] = {128,128};
default_textures["textures/castle/ctfb_stnwall9b_slmy"] = {128,128};
default_textures["textures/castle/ctfb_stnwall9c"] = {128,128};
default_textures["textures/castle/ctfb_stnwall9c_lit"] = {128,128};
default_textures["textures/castle/ctfb_stnwall9d"] = {128,128};
default_textures["textures/castle/ctfb_stnwall9e"] = {128,128};
default_textures["textures/castle/ctfr_2_oldwall2"] = {64,64};
default_textures["textures/castle/ctfr_base"] = {64,64};
default_textures["textures/castle/ctfr_con13"] = {128,128};
default_textures["textures/castle/ctfr_stnwall9"] = {128,128};
default_textures["textures/castle/ctfr_stnwall9b"] = {128,128};
default_textures["textures/castle/ctfr_stnwall9c"] = {128,128};
default_textures["textures/castle/ctfr_stnwall9c_lit"] = {128,128};
default_textures["textures/castle/ctfr_stnwall9d"] = {128,128};
default_textures["textures/castle/ctfr_stnwall9e"] = {128,128};
default_textures["textures/castle/curtain03"] = {64,64};
default_textures["textures/castle/curtain03a"] = {64,64};
default_textures["textures/castle/curtain04"] = {64,64};
default_textures["textures/castle/curtain04a"] = {64,64};
default_textures["textures/castle/decfloor"] = {64,64};
default_textures["textures/castle/deskwood"] = {64,64};
default_textures["textures/castle/deskwood_side"] = {64,64};
default_textures["textures/castle/diningceil"] = {64,64};
default_textures["textures/castle/diningceil2"] = {64,64};
default_textures["textures/castle/diningceil_beam1"] = {64,64};
default_textures["textures/castle/diningceil_beam2"] = {64,64};
default_textures["textures/castle/diningwall_bot"] = {64,64};
default_textures["textures/castle/diningwall_top"] = {64,64};
default_textures["textures/castle/diningwall_top1"] = {64,64};
default_textures["textures/castle/diningwall_top2"] = {64,64};
default_textures["textures/castle/diningwall_top3"] = {64,64};
default_textures["textures/castle/diningwall_top4"] = {64,64};
default_textures["textures/castle/dining_carpet"] = {64,64};
default_textures["textures/castle/dining_table"] = {64,64};
default_textures["textures/castle/door1a"] = {64,128};
default_textures["textures/castle/door3"] = {64,128};
default_textures["textures/castle/door4"] = {64,128};
default_textures["textures/castle/doorarch"] = {32,32};
default_textures["textures/castle/doorarch1"] = {32,16};
default_textures["textures/castle/doorarch1a"] = {32,32};
default_textures["textures/castle/doorarch2"] = {32,16};
default_textures["textures/castle/doorframe_deco"] = {32,64};
default_textures["textures/castle/doorframe_plain"] = {32,64};
default_textures["textures/castle/doortop"] = {64,64};
default_textures["textures/castle/door_front"] = {64,128};
default_textures["textures/castle/door_front_ext"] = {64,128};
default_textures["textures/castle/dungeonwall_128"] = {128,128};
default_textures["textures/castle/dungeonwall_64"] = {64,64};
default_textures["textures/castle/dungeonwall_slimy"] = {64,64};
default_textures["textures/castle/dungeonwall_slimya"] = {64,64};
default_textures["textures/castle/dungeonwall_slimyc"] = {64,64};
default_textures["textures/castle/dungeon_ceil1"] = {64,64};
default_textures["textures/castle/dungeon_ceil2"] = {64,64};
default_textures["textures/castle/dungeon_door"] = {64,128};
default_textures["textures/castle/dungeon_floor"] = {64,64};
default_textures["textures/castle/dungeon_wall"] = {64,64};
default_textures["textures/castle/dungeon_wall_b"] = {64,64};
default_textures["textures/castle/dungeon_wall_base"] = {64,64};
default_textures["textures/castle/dungeon_wall_c"] = {64,64};
default_textures["textures/castle/d_genericsmooth"] = {64,64};
default_textures["textures/castle/d_gravel"] = {64,64};
default_textures["textures/castle/d_metalribbed"] = {64,64};
default_textures["textures/castle/d_metalrough"] = {64,64};
default_textures["textures/castle/d_metalsmooth"] = {64,64};
default_textures["textures/castle/d_sand"] = {64,64};
default_textures["textures/castle/d_stonerough"] = {128,128};
default_textures["textures/castle/d_stonesmooth"] = {64,64};
default_textures["textures/castle/d_stucco"] = {64,64};
default_textures["textures/castle/d_woodrough"] = {64,64};
default_textures["textures/castle/d_woodsmooth"] = {64,64};
default_textures["textures/castle/env1a"] = {256,256};
default_textures["textures/castle/env1b"] = {256,256};
default_textures["textures/castle/env1c"] = {256,256};
default_textures["textures/castle/file_front"] = {32,64};
default_textures["textures/castle/file_side"] = {32,64};
default_textures["textures/castle/file_top"] = {16,16};
default_textures["textures/castle/fireplace_deco"] = {16,32};
default_textures["textures/castle/fireplace_iron"] = {32,32};
default_textures["textures/castle/floordesign"] = {64,128};
default_textures["textures/castle/floordesign2"] = {64,64};
default_textures["textures/castle/fresco2"] = {128,128};
default_textures["textures/castle/fresco_new"] = {128,128};
default_textures["textures/castle/fuse1"] = {32,64};
default_textures["textures/castle/fuse2"] = {32,64};
default_textures["textures/castle/glass_rosette"] = {128,128};
default_textures["textures/castle/glass_rosette3"] = {128,128};
default_textures["textures/castle/grass1"] = {64,64};
default_textures["textures/castle/grass2"] = {64,64};
default_textures["textures/castle/grate"] = {64,64};
default_textures["textures/castle/grate3"] = {64,64};
default_textures["textures/castle/grating1"] = {64,64};
default_textures["textures/castle/grating2"] = {64,64};
default_textures["textures/castle/greenbox"] = {64,64};
default_textures["textures/castle/greenbox_front1"] = {64,64};
default_textures["textures/castle/greenbox_front2"] = {64,64};
default_textures["textures/castle/greenbox_front3"] = {64,64};
default_textures["textures/castle/greenbox_front3d"] = {64,64};
default_textures["textures/castle/greenbox_panel"] = {64,64};
default_textures["textures/castle/greenbox_panel2"] = {32,32};
default_textures["textures/castle/greenbox_panel_d"] = {64,64};
default_textures["textures/castle/hallcarpet"] = {64,128};
default_textures["textures/castle/hallfloor_cut1"] = {64,64};
default_textures["textures/castle/hallfloor_cut2"] = {64,64};
default_textures["textures/castle/hallfloor_cut3"] = {64,64};
default_textures["textures/castle/hall_base"] = {64,64};
default_textures["textures/castle/hall_beamtall"] = {32,64};
default_textures["textures/castle/hall_ceil1"] = {64,64};
default_textures["textures/castle/hall_trim"] = {16,32};
default_textures["textures/castle/hall_trim2"] = {32,16};
default_textures["textures/castle/hay"] = {64,64};
default_textures["textures/castle/hint"] = {64,64};
default_textures["textures/castle/h_cretewall_dirty"] = {64,64};
default_textures["textures/castle/h_cretewall_plain"] = {64,64};
default_textures["textures/castle/h_heli_land1"] = {64,64};
default_textures["textures/castle/iron_rail"] = {64,64};
default_textures["textures/castle/kit_botcab"] = {64,32};
default_textures["textures/castle/kit_cabside"] = {32,32};
default_textures["textures/castle/kit_countertop"] = {64,32};
default_textures["textures/castle/kit_dripsa"] = {64,64};
default_textures["textures/castle/kit_dripsb"] = {64,64};
default_textures["textures/castle/kit_refrig"] = {64,128};
default_textures["textures/castle/kit_refrigside"] = {32,128};
default_textures["textures/castle/kit_refrigtop"] = {64,32};
default_textures["textures/castle/kit_steel_sink"] = {32,32};
default_textures["textures/castle/kit_stovefront"] = {64,32};
default_textures["textures/castle/kit_stovehood"] = {64,64};
default_textures["textures/castle/kit_stovetop"] = {64,32};
default_textures["textures/castle/kit_topcab"] = {64,64};
default_textures["textures/castle/kit_wall"] = {64,64};
default_textures["textures/castle/kit_wall3"] = {64,64};
default_textures["textures/castle/kit_wall_stain"] = {64,64};
default_textures["textures/castle/lib_wall01"] = {64,64};
default_textures["textures/castle/lightb"] = {64,32};
default_textures["textures/castle/lightc"] = {64,32};
default_textures["textures/castle/lighte"] = {32,32};
default_textures["textures/castle/lightg"] = {64,32};
default_textures["textures/castle/light_blue"] = {32,32};
default_textures["textures/castle/light_bluey"] = {32,32};
default_textures["textures/castle/light_orange4"] = {32,32};
default_textures["textures/castle/light_orange6"] = {32,32};
default_textures["textures/castle/light_red"] = {32,32};
default_textures["textures/castle/light_yellow"] = {32,32};
default_textures["textures/castle/map1"] = {64,64};
default_textures["textures/castle/map2"] = {64,64};
default_textures["textures/castle/marbfloor1"] = {128,128};
default_textures["textures/castle/marbfloor2"] = {64,64};
default_textures["textures/castle/marbfloor3"] = {128,128};
default_textures["textures/castle/marbfloor4"] = {128,128};
default_textures["textures/castle/marbfloor5"] = {128,128};
default_textures["textures/castle/marbfloor6"] = {64,64};
default_textures["textures/castle/mastbed_door"] = {64,128};
default_textures["textures/castle/metalrib2"] = {64,64};
default_textures["textures/castle/metal_clean"] = {64,64};
default_textures["textures/castle/metal_dark2"] = {32,32};
default_textures["textures/castle/metal_panel"] = {64,64};
default_textures["textures/castle/metal_rust"] = {64,64};
default_textures["textures/castle/metal_rust2"] = {64,64};
default_textures["textures/castle/metal_rust3"] = {64,64};
default_textures["textures/castle/metal_rust4"] = {64,64};
default_textures["textures/castle/metal_rust5"] = {64,64};
default_textures["textures/castle/metal_rust6"] = {64,64};
default_textures["textures/castle/metal_rust8"] = {64,64};
default_textures["textures/castle/mirror1b"] = {64,64};
default_textures["textures/castle/mirror1bd"] = {64,64};
default_textures["textures/castle/no_draw"] = {64,64};
default_textures["textures/castle/office_window"] = {64,64};
default_textures["textures/castle/office_window_d"] = {64,64};
default_textures["textures/castle/origin"] = {64,64};
default_textures["textures/castle/painting1"] = {32,64};
default_textures["textures/castle/painting2"] = {32,32};
default_textures["textures/castle/painting3"] = {32,64};
default_textures["textures/castle/painting4"] = {64,64};
default_textures["textures/castle/painting5"] = {64,128};
default_textures["textures/castle/pebbles"] = {128,128};
default_textures["textures/castle/pillar1"] = {64,64};
default_textures["textures/castle/pillar2"] = {64,64};
default_textures["textures/castle/pillar3"] = {64,64};
default_textures["textures/castle/pillar3b"] = {64,64};
default_textures["textures/castle/pipe2"] = {64,32};
default_textures["textures/castle/pipey1"] = {32,16};
default_textures["textures/castle/plywood_shelf"] = {64,64};
default_textures["textures/castle/railing1"] = {64,64};
default_textures["textures/castle/railing1_top"] = {32,16};
default_textures["textures/castle/railing3"] = {64,64};
default_textures["textures/castle/railtrim1_bot1"] = {64,32};
default_textures["textures/castle/rug01"] = {128,128};
default_textures["textures/castle/rug02"] = {128,128};
default_textures["textures/castle/shelf"] = {16,16};
default_textures["textures/castle/shingle01"] = {64,64};
default_textures["textures/castle/shower2_curtain"] = {64,64};
default_textures["textures/castle/shower_curtain_alpha"] = {64,64};
default_textures["textures/castle/skip"] = {64,64};
default_textures["textures/castle/sky"] = {64,64};
default_textures["textures/castle/stable_door"] = {64,128};
default_textures["textures/castle/stable_fence"] = {64,64};
default_textures["textures/castle/stairlip1"] = {32,16};
default_textures["textures/castle/stairlip2"] = {32,16};
default_textures["textures/castle/stonefloor1"] = {64,64};
default_textures["textures/castle/stonefloor2"] = {64,64};
default_textures["textures/castle/stonefloor3"] = {64,64};
default_textures["textures/castle/stonefloor4"] = {64,64};
default_textures["textures/castle/stonefloor5"] = {64,64};
default_textures["textures/castle/stonewall12"] = {64,64};
default_textures["textures/castle/stonewall13"] = {128,128};
default_textures["textures/castle/stonewall14"] = {128,128};
default_textures["textures/castle/stonewall15"] = {128,128};
default_textures["textures/castle/stonewall15b"] = {128,64};
default_textures["textures/castle/stonewall15c"] = {128,128};
default_textures["textures/castle/stonewall16"] = {128,128};
default_textures["textures/castle/stonewall16b"] = {128,128};
default_textures["textures/castle/stonewall16c"] = {64,64};
default_textures["textures/castle/stonewall16c_d"] = {64,64};
default_textures["textures/castle/stonewall16_wires"] = {128,128};
default_textures["textures/castle/stonewall17a"] = {128,128};
default_textures["textures/castle/stonewall17b"] = {64,32};
default_textures["textures/castle/stonewall17c"] = {64,32};
default_textures["textures/castle/stonewall2"] = {128,128};
default_textures["textures/castle/stonewall7"] = {64,64};
default_textures["textures/castle/stonewall8"] = {64,64};
default_textures["textures/castle/stonewall9"] = {128,128};
default_textures["textures/castle/stonewall9b"] = {128,128};
default_textures["textures/castle/stonewall9b_slimy"] = {128,128};
default_textures["textures/castle/stonewall9c"] = {128,128};
default_textures["textures/castle/stonewall9c_lit"] = {128,128};
default_textures["textures/castle/stonewall9d"] = {128,128};
default_textures["textures/castle/stonewall9d_lit"] = {128,128};
default_textures["textures/castle/stonewall9e"] = {128,128};
default_textures["textures/castle/stonewall9e_lit"] = {128,128};
default_textures["textures/castle/stuc_ceil1"] = {64,64};
default_textures["textures/castle/stuc_ceil1b"] = {64,64};
default_textures["textures/castle/stuc_ceil1_trim"] = {16,32};
default_textures["textures/castle/stuc_ceil2"] = {64,64};
default_textures["textures/castle/stuc_wall1"] = {64,64};
default_textures["textures/castle/stuc_wall1b"] = {64,64};
default_textures["textures/castle/stuc_walla"] = {64,64};
default_textures["textures/castle/stuc_wallb"] = {64,64};
default_textures["textures/castle/tablewood1"] = {64,64};
default_textures["textures/castle/tablewood2"] = {64,64};
default_textures["textures/castle/tablewood3"] = {64,64};
default_textures["textures/castle/tilefloor"] = {64,64};
default_textures["textures/castle/trigger"] = {64,64};
default_textures["textures/castle/trim_ceiling1"] = {64,32};
default_textures["textures/castle/vaultbeam"] = {32,32};
default_textures["textures/castle/vaultbeam2a"] = {32,64};
default_textures["textures/castle/vaultbeam_side"] = {32,32};
default_textures["textures/castle/vaultdoor"] = {64,128};
default_textures["textures/castle/vaultfloor1"] = {64,64};
default_textures["textures/castle/vaultfloor2"] = {64,64};
default_textures["textures/castle/vaultwall1"] = {64,128};
default_textures["textures/castle/wardrobe_front"] = {64,128};
default_textures["textures/castle/wardrobe_side"] = {32,128};
default_textures["textures/castle/wardrobe_top"] = {64,32};
default_textures["textures/castle/water"] = {128,128};
default_textures["textures/castle/water2"] = {128,128};
default_textures["textures/castle/wood1"] = {64,64};
default_textures["textures/castle/woodfloor1"] = {64,64};
default_textures["textures/castle/woodfloor2"] = {128,128};
default_textures["textures/castle/woodfloor4"] = {64,64};
default_textures["textures/castle/w_gardoor3"] = {128,128};
default_textures["textures/castle/w_palette1"] = {64,64};
default_textures["textures/castle/w_pallette3"] = {64,64};
default_textures["textures/general/clip"] = {64,64};
default_textures["textures/general/hint"] = {64,64};
default_textures["textures/general/skip"] = {64,64};
default_textures["textures/iraq/1_2_floor1"] = {32,32};
default_textures["textures/iraq/1_2_floor2"] = {32,32};
default_textures["textures/iraq/1_2_floor3"] = {64,64};
default_textures["textures/iraq/1_2_window"] = {32,64};
default_textures["textures/iraq/1_2_window2"] = {32,64};
default_textures["textures/iraq/1_arch_jim"] = {32,128};
default_textures["textures/iraq/1_bigarch"] = {128,128};
default_textures["textures/iraq/1_bigarch2"] = {64,128};
default_textures["textures/iraq/1_bigpiece_a"] = {128,128};
default_textures["textures/iraq/1_bigpiece_b"] = {128,128};
default_textures["textures/iraq/1_button_off"] = {32,32};
default_textures["textures/iraq/1_button_on"] = {32,32};
default_textures["textures/iraq/1_carpetile"] = {32,32};
default_textures["textures/iraq/1_carpetile2"] = {32,32};
default_textures["textures/iraq/1_designwalla"] = {128,32};
default_textures["textures/iraq/1_designwallb"] = {128,128};
default_textures["textures/iraq/1_designwallc"] = {128,64};
default_textures["textures/iraq/1_hotelwall"] = {128,128};
default_textures["textures/iraq/1_jimdoors"] = {128,128};
default_textures["textures/iraq/1_jimpanel_a"] = {64,64};
default_textures["textures/iraq/1_jimpanel_b"] = {64,64};
default_textures["textures/iraq/1_medarch"] = {64,128};
default_textures["textures/iraq/1_medarch2"] = {32,64};
default_textures["textures/iraq/1_smallarch"] = {64,128};
default_textures["textures/iraq/1_townarch1"] = {64,64};
default_textures["textures/iraq/1_townarch2"] = {64,128};
default_textures["textures/iraq/1_townarch3"] = {64,32};
default_textures["textures/iraq/1_townarch4"] = {64,128};
default_textures["textures/iraq/1_townbrick"] = {128,128};
default_textures["textures/iraq/1_townpillar_a"] = {32,32};
default_textures["textures/iraq/1_townpillar_b"] = {32,32};
default_textures["textures/iraq/1_townwall"] = {128,128};
default_textures["textures/iraq/1_townwall_a"] = {64,128};
default_textures["textures/iraq/1_townwall_b"] = {64,64};
default_textures["textures/iraq/1_townwall_market"] = {128,128};
default_textures["textures/iraq/1_townwall_plain"] = {128,128};
default_textures["textures/iraq/1_wall_jim1"] = {64,64};
default_textures["textures/iraq/1_wall_jim2"] = {64,64};
default_textures["textures/iraq/1_wall_jim3"] = {64,64};
default_textures["textures/iraq/1_wall_plaster1"] = {32,32};
default_textures["textures/iraq/1_wall_plaster1c"] = {32,16};
default_textures["textures/iraq/1_windowjim_a"] = {64,64};
default_textures["textures/iraq/1_windowjim_b"] = {64,32};
default_textures["textures/iraq/1_worktop"] = {128,64};
default_textures["textures/iraq/1_wsection"] = {128,128};
default_textures["textures/iraq/2_3_liftdoor"] = {64,128};
default_textures["textures/iraq/2_3_metalside1"] = {64,64};
default_textures["textures/iraq/2_3_mfloor"] = {64,64};
default_textures["textures/iraq/2_3_mfloor2"] = {64,64};
default_textures["textures/iraq/2_3_mfloordirty"] = {64,64};
default_textures["textures/iraq/2_3_vent1"] = {64,64};
default_textures["textures/iraq/2_bigdoor1"] = {64,64};
default_textures["textures/iraq/2_bigdoor1_kos3"] = {64,64};
default_textures["textures/iraq/2_bigdoor2"] = {64,64};
default_textures["textures/iraq/2_bigdoor2_kos3"] = {64,64};
default_textures["textures/iraq/2_bigdoor3"] = {64,64};
default_textures["textures/iraq/2_bigdoor4"] = {64,64};
default_textures["textures/iraq/2_bigdoor5"] = {32,32};
default_textures["textures/iraq/2_blockstack"] = {64,64};
default_textures["textures/iraq/2_blockstack2"] = {64,64};
default_textures["textures/iraq/2_blockwall"] = {64,64};
default_textures["textures/iraq/2_blockwall2"] = {64,64};
default_textures["textures/iraq/2_con2"] = {64,64};
default_textures["textures/iraq/2_conwall1"] = {128,128};
default_textures["textures/iraq/2_conwall1b"] = {128,128};
default_textures["textures/iraq/2_conwall1c"] = {128,128};
default_textures["textures/iraq/2_conwall3"] = {32,32};
default_textures["textures/iraq/2_conwall4"] = {64,64};
default_textures["textures/iraq/2_conwalla"] = {64,64};
default_textures["textures/iraq/2_conwallb2"] = {64,64};
default_textures["textures/iraq/2_greenlight_off"] = {64,16};
default_textures["textures/iraq/2_greenlight_on"] = {64,16};
default_textures["textures/iraq/2_metal"] = {64,64};
default_textures["textures/iraq/2_metalb"] = {64,64};
default_textures["textures/iraq/2_metalwall1a"] = {64,64};
default_textures["textures/iraq/2_redlight_off"] = {64,16};
default_textures["textures/iraq/2_redlight_on"] = {64,16};
default_textures["textures/iraq/2_steelceila"] = {64,64};
default_textures["textures/iraq/2_steelceilb"] = {64,64};
default_textures["textures/iraq/2_steelwall1"] = {64,128};
default_textures["textures/iraq/2_steelwall2"] = {64,128};
default_textures["textures/iraq/2_steelwalla"] = {64,128};
default_textures["textures/iraq/2_steelwallb"] = {64,128};
default_textures["textures/iraq/2_steelwallb2"] = {64,128};
default_textures["textures/iraq/2_steelwallc"] = {64,64};
default_textures["textures/iraq/2_steelwallc3"] = {64,64};
default_textures["textures/iraq/2_steelwallc4"] = {64,64};
default_textures["textures/iraq/2_steelwallc5"] = {64,64};
default_textures["textures/iraq/2_steelwalld"] = {64,64};
default_textures["textures/iraq/2_steelwalle"] = {64,64};
default_textures["textures/iraq/2_steelwallf"] = {64,64};
default_textures["textures/iraq/2_steelwallf2"] = {64,64};
default_textures["textures/iraq/2_steelwallg"] = {64,128};
default_textures["textures/iraq/2_steelwallg2"] = {64,128};
default_textures["textures/iraq/2_steelwallh"] = {64,128};
default_textures["textures/iraq/2_steelwallred"] = {128,128};
default_textures["textures/iraq/2_steelwallredb"] = {128,128};
default_textures["textures/iraq/3_beamangle"] = {32,32};
default_textures["textures/iraq/3_beamfront"] = {16,16};
default_textures["textures/iraq/3_beamfront2"] = {16,16};
default_textures["textures/iraq/3_beamplain"] = {16,16};
default_textures["textures/iraq/3_beamside"] = {16,16};
default_textures["textures/iraq/3_beamtop1"] = {64,32};
default_textures["textures/iraq/3_beamtop2"] = {32,32};
default_textures["textures/iraq/3_bigtop"] = {128,128};
default_textures["textures/iraq/3_box1frnt"] = {64,32};
default_textures["textures/iraq/3_box1frntd"] = {64,32};
default_textures["textures/iraq/3_box1frntx"] = {64,32};
default_textures["textures/iraq/3_box1frntx_d"] = {64,32};
default_textures["textures/iraq/3_box1side"] = {32,64};
default_textures["textures/iraq/3_box1sided"] = {32,64};
default_textures["textures/iraq/3_box1top1"] = {64,32};
default_textures["textures/iraq/3_box1top1x"] = {64,32};
default_textures["textures/iraq/3_box1top1x_b"] = {64,32};
default_textures["textures/iraq/3_box1top1x_d"] = {64,32};
default_textures["textures/iraq/3_box1top2"] = {64,32};
default_textures["textures/iraq/3_box1topd"] = {64,32};
default_textures["textures/iraq/3_box2frnt"] = {32,128};
default_textures["textures/iraq/3_box2frntd"] = {32,128};
default_textures["textures/iraq/3_box2frntx"] = {32,128};
default_textures["textures/iraq/3_box2frntx_b"] = {32,128};
default_textures["textures/iraq/3_box2frntx_d"] = {32,128};
default_textures["textures/iraq/3_box2side1"] = {64,128};
default_textures["textures/iraq/3_box2side1x"] = {64,128};
default_textures["textures/iraq/3_box2side1x_d"] = {64,128};
default_textures["textures/iraq/3_box2side2"] = {64,16};
default_textures["textures/iraq/3_box2top"] = {32,64};
default_textures["textures/iraq/3_box2topx"] = {32,64};
default_textures["textures/iraq/3_box2topx_d"] = {32,64};
default_textures["textures/iraq/3_box3front1"] = {64,64};
default_textures["textures/iraq/3_box3front1x"] = {64,64};
default_textures["textures/iraq/3_box3front1x_d"] = {64,64};
default_textures["textures/iraq/3_box3front2"] = {64,64};
default_textures["textures/iraq/3_box3frontd"] = {64,64};
default_textures["textures/iraq/3_box3side1a"] = {16,64};
default_textures["textures/iraq/3_box4front"] = {64,32};
default_textures["textures/iraq/3_box4frontd"] = {64,32};
default_textures["textures/iraq/3_box4frontx"] = {64,32};
default_textures["textures/iraq/3_box4frontx_d"] = {64,32};
default_textures["textures/iraq/3_box4side"] = {16,32};
default_textures["textures/iraq/3_box4sided"] = {16,32};
default_textures["textures/iraq/3_box4top1"] = {64,16};
default_textures["textures/iraq/3_box4top1x"] = {64,32};
default_textures["textures/iraq/3_box4top1x_b"] = {64,32};
default_textures["textures/iraq/3_box4top1x_d"] = {64,32};
default_textures["textures/iraq/3_box4top2"] = {64,16};
default_textures["textures/iraq/3_box4topd"] = {64,16};
default_textures["textures/iraq/3_box5front1"] = {64,32};
default_textures["textures/iraq/3_box5front2"] = {64,32};
default_textures["textures/iraq/3_box5frontd"] = {64,32};
default_textures["textures/iraq/3_box5side"] = {32,32};
default_textures["textures/iraq/3_box5top"] = {64,16};
default_textures["textures/iraq/3_box6bigside"] = {128,128};
default_textures["textures/iraq/3_box6bottomfront"] = {64,32};
default_textures["textures/iraq/3_box6bottomfrontd"] = {64,32};
default_textures["textures/iraq/3_box6bottomtop1"] = {64,16};
default_textures["textures/iraq/3_box6bottomtop2"] = {64,16};
default_textures["textures/iraq/3_box6bottomtopd"] = {64,16};
default_textures["textures/iraq/3_box6frntfront"] = {32,64};
default_textures["textures/iraq/3_box6frntside"] = {16,64};
default_textures["textures/iraq/3_box6frnttop"] = {32,16};
default_textures["textures/iraq/3_box6front"] = {32,64};
default_textures["textures/iraq/3_box6lidfront"] = {128,16};
default_textures["textures/iraq/3_box6lidtop"] = {128,16};
default_textures["textures/iraq/3_box72frnt23top"] = {64,32};
default_textures["textures/iraq/3_box72frnt23topx"] = {64,32};
default_textures["textures/iraq/3_box72frnt23topx_d"] = {64,32};
default_textures["textures/iraq/3_box72side"] = {32,64};
default_textures["textures/iraq/3_box72sided"] = {32,64};
default_textures["textures/iraq/3_box72sidex"] = {32,64};
default_textures["textures/iraq/3_box72sidex_d"] = {32,64};
default_textures["textures/iraq/3_box73frnt"] = {64,64};
default_textures["textures/iraq/3_box74front"] = {64,32};
default_textures["textures/iraq/3_box74frontx"] = {64,32};
default_textures["textures/iraq/3_box74frontx_d"] = {64,32};
default_textures["textures/iraq/3_box74top"] = {64,32};
default_textures["textures/iraq/3_box74top2"] = {64,32};
default_textures["textures/iraq/3_box74topd"] = {64,32};
default_textures["textures/iraq/3_box74topx"] = {64,32};
default_textures["textures/iraq/3_box74topx_b"] = {64,32};
default_textures["textures/iraq/3_box74topx_d"] = {64,32};
default_textures["textures/iraq/3_box7bfront"] = {64,128};
default_textures["textures/iraq/3_box7bfrontd"] = {64,128};
default_textures["textures/iraq/3_box7bfrontx"] = {64,128};
default_textures["textures/iraq/3_box7bfrontx_d"] = {64,128};
default_textures["textures/iraq/3_box7bside"] = {32,128};
default_textures["textures/iraq/3_box7bsided"] = {32,128};
default_textures["textures/iraq/3_box7bsidex"] = {32,128};
default_textures["textures/iraq/3_box7bsidex_d"] = {32,128};
default_textures["textures/iraq/3_box7btop"] = {64,32};
default_textures["textures/iraq/3_box7btopx"] = {64,32};
default_textures["textures/iraq/3_box7btopx_d"] = {64,32};
default_textures["textures/iraq/3_box8front"] = {64,16};
default_textures["textures/iraq/3_box8side"] = {16,32};
default_textures["textures/iraq/3_box8top1"] = {64,16};
default_textures["textures/iraq/3_box8top2"] = {64,16};
default_textures["textures/iraq/3_box8topd"] = {64,16};
default_textures["textures/iraq/3_boxfrnta"] = {64,64};
default_textures["textures/iraq/3_boxfrntb"] = {64,64};
default_textures["textures/iraq/3_boxfrntc"] = {64,64};
default_textures["textures/iraq/3_boxplain"] = {32,32};
default_textures["textures/iraq/3_cardtable"] = {64,64};
default_textures["textures/iraq/3_catwalk"] = {64,64};
default_textures["textures/iraq/3_catwalk2"] = {64,64};
default_textures["textures/iraq/3_catwalk2b"] = {64,64};
default_textures["textures/iraq/3_catwlk_noalpha"] = {64,64};
default_textures["textures/iraq/3_ceiling1"] = {64,64};
default_textures["textures/iraq/3_cweighta"] = {64,32};
default_textures["textures/iraq/3_cweightside"] = {32,32};
default_textures["textures/iraq/3_cweighttop"] = {64,32};
default_textures["textures/iraq/3_elebutton"] = {16,32};
default_textures["textures/iraq/3_eledoor"] = {64,128};
default_textures["textures/iraq/3_flatwood"] = {64,64};
default_textures["textures/iraq/3_floorhole"] = {32,32};
default_textures["textures/iraq/3_gate1"] = {128,128};
default_textures["textures/iraq/3_mbeam"] = {16,64};
default_textures["textures/iraq/3_metal1"] = {64,64};
default_textures["textures/iraq/3_metal1b"] = {64,64};
default_textures["textures/iraq/3_metalrib1"] = {32,32};
default_textures["textures/iraq/3_metalside"] = {64,64};
default_textures["textures/iraq/3_meter1"] = {64,64};
default_textures["textures/iraq/3_meter1d"] = {64,64};
default_textures["textures/iraq/3_meter2"] = {64,64};
default_textures["textures/iraq/3_meter2d"] = {64,64};
default_textures["textures/iraq/3_meter3"] = {64,64};
default_textures["textures/iraq/3_meter3d"] = {64,64};
default_textures["textures/iraq/3_meter4"] = {64,64};
default_textures["textures/iraq/3_meter4d"] = {64,64};
default_textures["textures/iraq/3_mfloor"] = {16,16};
default_textures["textures/iraq/3_mplate"] = {128,128};
default_textures["textures/iraq/3_msupport"] = {64,128};
default_textures["textures/iraq/3_mtrim1"] = {64,32};
default_textures["textures/iraq/3_mtrim2"] = {64,32};
default_textures["textures/iraq/3_oiltank1"] = {64,64};
default_textures["textures/iraq/3_oiltank1a"] = {64,64};
default_textures["textures/iraq/3_oiltank2"] = {64,64};
default_textures["textures/iraq/3_oiltank2plain"] = {64,64};
default_textures["textures/iraq/3_oiltank3"] = {128,128};
default_textures["textures/iraq/3_paint_orange"] = {16,16};
default_textures["textures/iraq/3_paint_red"] = {16,16};
default_textures["textures/iraq/3_panel1"] = {32,64};
default_textures["textures/iraq/3_panel1d"] = {32,64};
default_textures["textures/iraq/3_panel2"] = {64,64};
default_textures["textures/iraq/3_panel2d"] = {64,64};
default_textures["textures/iraq/3_panel3"] = {64,64};
default_textures["textures/iraq/3_panel3d"] = {64,64};
default_textures["textures/iraq/3_panel4"] = {64,64};
default_textures["textures/iraq/3_panel4d"] = {64,64};
default_textures["textures/iraq/3_panel5"] = {64,32};
default_textures["textures/iraq/3_panel5d"] = {64,32};
default_textures["textures/iraq/3_panel6"] = {32,64};
default_textures["textures/iraq/3_panel6d"] = {32,64};
default_textures["textures/iraq/3_panel7"] = {128,64};
default_textures["textures/iraq/3_panel7d"] = {128,64};
default_textures["textures/iraq/3_pipe2"] = {32,32};
default_textures["textures/iraq/3_pipe2b"] = {64,32};
default_textures["textures/iraq/3_pipe3"] = {64,64};
default_textures["textures/iraq/3_pipe3b"] = {64,64};
default_textures["textures/iraq/3_pipes1"] = {64,64};
default_textures["textures/iraq/3_pipes1a"] = {64,64};
default_textures["textures/iraq/3_pipes2"] = {64,32};
default_textures["textures/iraq/3_pipes2b"] = {64,32};
default_textures["textures/iraq/3_pipes3"] = {32,32};
default_textures["textures/iraq/3_pipes3a"] = {64,32};
default_textures["textures/iraq/3_pipes5"] = {32,32};
default_textures["textures/iraq/3_pipes5b"] = {32,32};
default_textures["textures/iraq/3_pipes5bd"] = {32,32};
default_textures["textures/iraq/3_pipes6"] = {64,64};
default_textures["textures/iraq/3_pipesgrate"] = {64,64};
default_textures["textures/iraq/3_pipesoily"] = {64,64};
default_textures["textures/iraq/3_pipe_rusty"] = {64,32};
default_textures["textures/iraq/3_rail"] = {64,64};
default_textures["textures/iraq/3_rockwall"] = {128,128};
default_textures["textures/iraq/3_rockwallbot"] = {128,32};
default_textures["textures/iraq/3_rockwallseam"] = {128,128};
default_textures["textures/iraq/3_rockwallseambot"] = {128,32};
default_textures["textures/iraq/3_rustplate"] = {64,64};
default_textures["textures/iraq/3_steelwall1"] = {64,128};
default_textures["textures/iraq/3_steelwall1d"] = {64,128};
default_textures["textures/iraq/3_steelwall1dlogo"] = {64,128};
default_textures["textures/iraq/3_steelwall1dr"] = {64,128};
default_textures["textures/iraq/3_steelwall1logo"] = {64,128};
default_textures["textures/iraq/3_steelwall1r"] = {64,128};
default_textures["textures/iraq/3_steelwall1trim"] = {64,64};
default_textures["textures/iraq/3_steelwall1trimr"] = {64,64};
default_textures["textures/iraq/3_steelwall1x"] = {64,128};
default_textures["textures/iraq/3_steelwall2"] = {64,128};
default_textures["textures/iraq/3_steelwall2r"] = {64,128};
default_textures["textures/iraq/3_steelwall2x"] = {64,128};
default_textures["textures/iraq/3_steelwall3"] = {64,128};
default_textures["textures/iraq/3_steelwall3x"] = {64,128};
default_textures["textures/iraq/3_steelwallscott"] = {64,128};
default_textures["textures/iraq/3_tankdoor1"] = {64,64};
default_textures["textures/iraq/3_tankdoor2"] = {64,64};
default_textures["textures/iraq/3_tankdoor3"] = {64,64};
default_textures["textures/iraq/3_tankdoor4"] = {64,64};
default_textures["textures/iraq/3_tankdoor6"] = {16,64};
default_textures["textures/iraq/3_tankdoor7"] = {64,128};
default_textures["textures/iraq/3_tankdoor7_kos3"] = {64,128};
default_textures["textures/iraq/3_trim1"] = {32,32};
default_textures["textures/iraq/3_wire"] = {16,16};
default_textures["textures/iraq/alpha"] = {16,16};
default_textures["textures/iraq/ammobox_end"] = {32,32};
default_textures["textures/iraq/ammobox_side"] = {64,32};
default_textures["textures/iraq/ammobox_top"] = {64,32};
default_textures["textures/iraq/ammobox_topd"] = {64,32};
default_textures["textures/iraq/barbwire"] = {64,64};
default_textures["textures/iraq/barrel"] = {64,64};
default_textures["textures/iraq/barrel_end"] = {64,64};
default_textures["textures/iraq/barrel_end_b"] = {64,64};
default_textures["textures/iraq/bars1"] = {32,64};
default_textures["textures/iraq/billboard1"] = {64,64};
default_textures["textures/iraq/billboard1b"] = {64,64};
default_textures["textures/iraq/billboard2"] = {64,128};
default_textures["textures/iraq/billboard2b"] = {64,128};
default_textures["textures/iraq/black"] = {16,16};
default_textures["textures/iraq/bookcase1"] = {64,128};
default_textures["textures/iraq/bookends"] = {64,16};
default_textures["textures/iraq/books"] = {64,16};
default_textures["textures/iraq/brick1"] = {64,64};
default_textures["textures/iraq/brick2"] = {64,64};
default_textures["textures/iraq/brick2a"] = {64,32};
default_textures["textures/iraq/brick2b"] = {16,64};
default_textures["textures/iraq/brick3"] = {64,64};
default_textures["textures/iraq/brick3b"] = {64,32};
default_textures["textures/iraq/brick4"] = {64,64};
default_textures["textures/iraq/brick4b"] = {64,32};
default_textures["textures/iraq/brick5"] = {64,64};
default_textures["textures/iraq/brick5a"] = {64,32};
default_textures["textures/iraq/brick5b"] = {64,32};
default_textures["textures/iraq/brick6"] = {64,64};
default_textures["textures/iraq/brick7"] = {32,32};
default_textures["textures/iraq/brick8"] = {64,64};
default_textures["textures/iraq/brick8_trim1"] = {64,32};
default_textures["textures/iraq/brick8_trim2"] = {64,32};
default_textures["textures/iraq/brick8_win1"] = {128,128};
default_textures["textures/iraq/brickstucbot"] = {64,64};
default_textures["textures/iraq/brickstucmid"] = {64,64};
default_textures["textures/iraq/brickstuctop"] = {64,64};
default_textures["textures/iraq/brickstuctopbot"] = {64,32};
default_textures["textures/iraq/brickstuctopdirt"] = {64,64};
default_textures["textures/iraq/camera1"] = {64,64};
default_textures["textures/iraq/camera2"] = {64,64};
default_textures["textures/iraq/camera3"] = {64,64};
default_textures["textures/iraq/camounet"] = {64,64};
default_textures["textures/iraq/cargodoor_in"] = {64,128};
default_textures["textures/iraq/cargodoor_out"] = {64,128};
default_textures["textures/iraq/cargolid"] = {64,64};
default_textures["textures/iraq/cargolid2"] = {32,32};
default_textures["textures/iraq/cargonet"] = {64,64};
default_textures["textures/iraq/cargopad"] = {64,64};
default_textures["textures/iraq/cargoside_a"] = {64,64};
default_textures["textures/iraq/cargowheel_a"] = {64,64};
default_textures["textures/iraq/cargowheel_b"] = {32,32};
default_textures["textures/iraq/cargowindow_a"] = {128,64};
default_textures["textures/iraq/cargowindow_b"] = {64,64};
default_textures["textures/iraq/carpet1"] = {32,32};
default_textures["textures/iraq/carpet2"] = {32,32};
default_textures["textures/iraq/carpet3"] = {32,32};
default_textures["textures/iraq/chain1"] = {16,64};
default_textures["textures/iraq/chain2"] = {16,64};
default_textures["textures/iraq/chainlink"] = {16,16};
default_textures["textures/iraq/chainlink_pipe"] = {8,32};
default_textures["textures/iraq/clip"] = {64,64};
default_textures["textures/iraq/con1"] = {64,64};
default_textures["textures/iraq/con2"] = {32,32};
default_textures["textures/iraq/con3"] = {128,128};
default_textures["textures/iraq/con4"] = {64,64};
default_textures["textures/iraq/con5"] = {64,64};
default_textures["textures/iraq/con6"] = {64,64};
default_textures["textures/iraq/con6b"] = {64,64};
default_textures["textures/iraq/con6c"] = {64,64};
default_textures["textures/iraq/con6dirty"] = {64,64};
default_textures["textures/iraq/con7"] = {128,128};
default_textures["textures/iraq/crate3"] = {64,64};
default_textures["textures/iraq/crate4"] = {64,64};
default_textures["textures/iraq/crate5"] = {64,64};
default_textures["textures/iraq/crate5b"] = {64,64};
default_textures["textures/iraq/crate5x"] = {64,64};
default_textures["textures/iraq/crate_rough"] = {64,64};
default_textures["textures/iraq/crate_rough_mtl"] = {64,64};
default_textures["textures/iraq/ctfb_base"] = {64,64};
default_textures["textures/iraq/ctfb_crate3"] = {64,64};
default_textures["textures/iraq/ctfb_crate5"] = {64,64};
default_textures["textures/iraq/ctfb_designwalla"] = {128,32};
default_textures["textures/iraq/ctfb_door5new"] = {64,128};
default_textures["textures/iraq/ctfb_trim10"] = {32,16};
default_textures["textures/iraq/ctfb_trim5"] = {32,32};
default_textures["textures/iraq/ctfr_base"] = {64,64};
default_textures["textures/iraq/ctfr_crate3"] = {64,64};
default_textures["textures/iraq/ctfr_crate5"] = {64,64};
default_textures["textures/iraq/ctfr_designwalla"] = {128,32};
default_textures["textures/iraq/ctfr_door5new"] = {64,128};
default_textures["textures/iraq/ctfr_trim10"] = {32,16};
default_textures["textures/iraq/ctfr_trim5"] = {32,32};
default_textures["textures/iraq/c_ceiling1"] = {64,128};
default_textures["textures/iraq/c_ceiling2"] = {64,128};
default_textures["textures/iraq/c_rampdoor"] = {128,128};
default_textures["textures/iraq/c_rampfloor"] = {128,128};
default_textures["textures/iraq/c_wall1"] = {64,128};
default_textures["textures/iraq/c_wall2"] = {64,128};
default_textures["textures/iraq/c_wall3"] = {64,128};
default_textures["textures/iraq/c_wallceil"] = {64,32};
default_textures["textures/iraq/c_wallceil2"] = {64,32};
default_textures["textures/iraq/desktop"] = {128,64};
default_textures["textures/iraq/deskwood"] = {64,64};
default_textures["textures/iraq/dirt1"] = {64,64};
default_textures["textures/iraq/dirt2"] = {64,64};
default_textures["textures/iraq/dirt3"] = {64,64};
default_textures["textures/iraq/door1"] = {64,128};
default_textures["textures/iraq/door2"] = {64,128};
default_textures["textures/iraq/door2a"] = {64,128};
default_textures["textures/iraq/door3"] = {64,128};
default_textures["textures/iraq/door3a"] = {64,128};
default_textures["textures/iraq/door4"] = {64,128};
default_textures["textures/iraq/door5"] = {64,128};
default_textures["textures/iraq/door6"] = {64,128};
default_textures["textures/iraq/door7"] = {64,128};
default_textures["textures/iraq/door8"] = {64,128};
default_textures["textures/iraq/d_dirt"] = {128,128};
default_textures["textures/iraq/d_genericsmooth"] = {64,64};
default_textures["textures/iraq/d_grass"] = {128,128};
default_textures["textures/iraq/d_gravel"] = {64,64};
default_textures["textures/iraq/d_metalbump"] = {32,32};
default_textures["textures/iraq/d_metalribbed"] = {64,64};
default_textures["textures/iraq/d_metalrough"] = {64,64};
default_textures["textures/iraq/d_metalsmooth"] = {64,64};
default_textures["textures/iraq/d_sand"] = {64,64};
default_textures["textures/iraq/d_stonecracked"] = {64,64};
default_textures["textures/iraq/d_stoneholey"] = {64,64};
default_textures["textures/iraq/d_stoneholey2"] = {64,64};
default_textures["textures/iraq/d_stonerough"] = {128,128};
default_textures["textures/iraq/d_stonesmooth"] = {64,64};
default_textures["textures/iraq/d_stucco"] = {64,64};
default_textures["textures/iraq/d_woodrough"] = {64,64};
default_textures["textures/iraq/d_woodrough2"] = {64,64};
default_textures["textures/iraq/d_woodsmooth"] = {64,64};
default_textures["textures/iraq/d_woodsmooth2"] = {64,64};
default_textures["textures/iraq/elev_bar1a"] = {128,128};
default_textures["textures/iraq/elev_bar2"] = {32,64};
default_textures["textures/iraq/elev_bar_side"] = {32,32};
default_textures["textures/iraq/env1a"] = {256,256};
default_textures["textures/iraq/env1b"] = {256,256};
default_textures["textures/iraq/env1c"] = {256,256};
default_textures["textures/iraq/env_irq"] = {256,256};
default_textures["textures/iraq/fence"] = {32,64};
default_textures["textures/iraq/fence128"] = {128,128};
default_textures["textures/iraq/fence48"] = {64,64};
default_textures["textures/iraq/fence48tall"] = {64,128};
default_textures["textures/iraq/fencegate1"] = {16,16};
default_textures["textures/iraq/fencegate1b"] = {16,16};
default_textures["textures/iraq/fencegate1top"] = {16,16};
default_textures["textures/iraq/file_tops"] = {32,32};
default_textures["textures/iraq/flag1"] = {128,64};
default_textures["textures/iraq/flag1b"] = {128,64};
default_textures["textures/iraq/f_btmwall"] = {128,128};
default_textures["textures/iraq/f_btmwall_2"] = {128,128};
default_textures["textures/iraq/f_carpet_red"] = {32,32};
default_textures["textures/iraq/f_carpet_red_trim"] = {16,32};
default_textures["textures/iraq/f_carpet_rust"] = {32,32};
default_textures["textures/iraq/f_ceil1"] = {128,128};
default_textures["textures/iraq/f_ceil2"] = {128,128};
default_textures["textures/iraq/f_conwall"] = {128,128};
default_textures["textures/iraq/f_door1"] = {128,128};
default_textures["textures/iraq/f_door2l"] = {64,128};
default_textures["textures/iraq/f_floortile"] = {128,128};
default_textures["textures/iraq/f_floortile2"] = {128,128};
default_textures["textures/iraq/f_floortilea"] = {128,128};
default_textures["textures/iraq/f_griffon"] = {128,128};
default_textures["textures/iraq/f_trim_big"] = {128,128};
default_textures["textures/iraq/f_trim_big2"] = {128,128};
default_textures["textures/iraq/girdwall"] = {128,128};
default_textures["textures/iraq/girdwall_noalpha"] = {128,128};
default_textures["textures/iraq/glass_wire"] = {64,64};
default_textures["textures/iraq/grass"] = {64,64};
default_textures["textures/iraq/grassside1"] = {64,64};
default_textures["textures/iraq/gravel1"] = {64,64};
default_textures["textures/iraq/grid1"] = {128,64};
default_textures["textures/iraq/grid1d"] = {128,64};
default_textures["textures/iraq/grid2"] = {32,64};
default_textures["textures/iraq/grid2d"] = {32,64};
default_textures["textures/iraq/grid3"] = {64,64};
default_textures["textures/iraq/grid3d"] = {64,64};
default_textures["textures/iraq/hint"] = {64,64};
default_textures["textures/iraq/h_cretewall"] = {64,64};
default_textures["textures/iraq/h_cretewall_blue"] = {64,64};
default_textures["textures/iraq/h_cretewall_code"] = {64,64};
default_textures["textures/iraq/h_cretewall_code2"] = {64,64};
default_textures["textures/iraq/h_cretewall_dirty"] = {64,64};
default_textures["textures/iraq/h_cretewall_plain"] = {64,64};
default_textures["textures/iraq/h_cretewall_trim"] = {64,32};
default_textures["textures/iraq/h_cretewall_yellow"] = {64,64};
default_textures["textures/iraq/h_heli_land1"] = {64,64};
default_textures["textures/iraq/h_heli_land2"] = {16,16};
default_textures["textures/iraq/h_heli_land_a"] = {64,64};
default_textures["textures/iraq/h_heli_land_b"] = {64,64};
default_textures["textures/iraq/h_heli_land_c"] = {64,64};
default_textures["textures/iraq/h_heli_land_d"] = {64,64};
default_textures["textures/iraq/h_light1"] = {64,32};
default_textures["textures/iraq/h_light2"] = {64,32};
default_textures["textures/iraq/h_light3"] = {32,32};
default_textures["textures/iraq/h_light4"] = {32,32};
default_textures["textures/iraq/h_light5"] = {32,32};
default_textures["textures/iraq/h_yardfloor"] = {128,128};
default_textures["textures/iraq/h_yardfloor2"] = {64,64};
default_textures["textures/iraq/king_hill"] = {256,256};
default_textures["textures/iraq/ladder_rung"] = {32,16};
default_textures["textures/iraq/lightbars"] = {32,64};
default_textures["textures/iraq/light_blue"] = {32,32};
default_textures["textures/iraq/light_pool"] = {32,32};
default_textures["textures/iraq/light_red"] = {32,32};
default_textures["textures/iraq/light_small"] = {32,16};
default_textures["textures/iraq/light_tiny_red"] = {16,16};
default_textures["textures/iraq/light_tiny_white"] = {16,16};
default_textures["textures/iraq/light_tiny_yellow"] = {16,16};
default_textures["textures/iraq/light_tube"] = {128,32};
default_textures["textures/iraq/light_tubey"] = {16,64};
default_textures["textures/iraq/light_yellow2"] = {32,32};
default_textures["textures/iraq/light_yellow3"] = {32,32};
default_textures["textures/iraq/light_yellow4"] = {32,32};
default_textures["textures/iraq/light_yellow6"] = {32,32};
default_textures["textures/iraq/light_yellow7"] = {32,32};
default_textures["textures/iraq/med_door"] = {32,32};
default_textures["textures/iraq/med_inside"] = {32,32};
default_textures["textures/iraq/med_inside_used"] = {32,32};
default_textures["textures/iraq/med_red"] = {16,16};
default_textures["textures/iraq/med_sides"] = {16,16};
default_textures["textures/iraq/med_sign"] = {64,64};
default_textures["textures/iraq/med_stripe"] = {16,16};
default_textures["textures/iraq/metal1"] = {64,64};
default_textures["textures/iraq/metalside3"] = {16,16};
default_textures["textures/iraq/metal_blue"] = {16,16};
default_textures["textures/iraq/metal_dark"] = {16,16};
default_textures["textures/iraq/metal_grey"] = {16,16};
default_textures["textures/iraq/metal_rust"] = {64,64};
default_textures["textures/iraq/metal_rust1"] = {16,16};
default_textures["textures/iraq/metal_rust2"] = {16,16};
default_textures["textures/iraq/metal_rust3"] = {32,32};
default_textures["textures/iraq/metal_rust4"] = {32,32};
default_textures["textures/iraq/metal_smooth"] = {64,64};
default_textures["textures/iraq/no_draw"] = {64,64};
default_textures["textures/iraq/oil"] = {64,64};
default_textures["textures/iraq/origin"] = {64,64};
default_textures["textures/iraq/painting1"] = {32,32};
default_textures["textures/iraq/painting2"] = {32,64};
default_textures["textures/iraq/painting3"] = {64,32};
default_textures["textures/iraq/painting7"] = {128,64};
default_textures["textures/iraq/pipe3"] = {128,128};
default_textures["textures/iraq/pipe3b"] = {128,128};
default_textures["textures/iraq/pipe4"] = {64,64};
default_textures["textures/iraq/pipe4b"] = {64,64};
default_textures["textures/iraq/pipe_end"] = {64,64};
default_textures["textures/iraq/pipe_endb"] = {64,64};
default_textures["textures/iraq/pipe_paint1a"] = {64,64};
default_textures["textures/iraq/pipe_paint1b"] = {64,64};
default_textures["textures/iraq/pipe_paint2a"] = {64,64};
default_textures["textures/iraq/pipe_paint2b"] = {64,64};
default_textures["textures/iraq/plaster1"] = {64,64};
default_textures["textures/iraq/radar1"] = {64,64};
default_textures["textures/iraq/radar1d"] = {64,64};
default_textures["textures/iraq/radar2"] = {64,64};
default_textures["textures/iraq/radar3"] = {64,64};
default_textures["textures/iraq/road"] = {64,64};
default_textures["textures/iraq/roadside1"] = {32,32};
default_textures["textures/iraq/roadside2"] = {32,32};
default_textures["textures/iraq/rockwall_brown"] = {128,128};
default_textures["textures/iraq/rockwall_brown2"] = {64,64};
default_textures["textures/iraq/rug1"] = {64,128};
default_textures["textures/iraq/rug2"] = {64,128};
default_textures["textures/iraq/rustyplain"] = {32,32};
default_textures["textures/iraq/sand"] = {128,128};
default_textures["textures/iraq/sandbag64"] = {64,64};
default_textures["textures/iraq/sign1"] = {64,32};
default_textures["textures/iraq/sign10"] = {32,32};
default_textures["textures/iraq/sign11"] = {32,32};
default_textures["textures/iraq/sign12"] = {32,32};
default_textures["textures/iraq/sign2"] = {64,32};
default_textures["textures/iraq/sign3"] = {64,32};
default_textures["textures/iraq/sign4"] = {64,32};
default_textures["textures/iraq/sign5"] = {32,32};
default_textures["textures/iraq/sign6"] = {128,64};
default_textures["textures/iraq/sign7"] = {64,64};
default_textures["textures/iraq/sign8"] = {16,16};
default_textures["textures/iraq/sign9"] = {32,32};
default_textures["textures/iraq/skip"] = {64,64};
default_textures["textures/iraq/sky"] = {64,64};
default_textures["textures/iraq/stairside1"] = {16,16};
default_textures["textures/iraq/stairside2"] = {16,16};
default_textures["textures/iraq/stairside3"] = {16,16};
default_textures["textures/iraq/stairside4"] = {16,16};
default_textures["textures/iraq/stairside_wood"] = {32,16};
default_textures["textures/iraq/stairtop_con"] = {32,64};
default_textures["textures/iraq/stairtop_wood"] = {32,64};
default_textures["textures/iraq/stucco1"] = {64,64};
default_textures["textures/iraq/stucco10"] = {128,128};
default_textures["textures/iraq/stucco10a"] = {128,64};
default_textures["textures/iraq/stucco11"] = {64,64};
default_textures["textures/iraq/stucco12"] = {64,64};
default_textures["textures/iraq/stucco12a"] = {64,64};
default_textures["textures/iraq/stucco12b"] = {64,32};
default_textures["textures/iraq/stucco13"] = {64,64};
default_textures["textures/iraq/stucco13a"] = {64,64};
default_textures["textures/iraq/stucco13ad"] = {64,64};
default_textures["textures/iraq/stucco13b"] = {64,32};
default_textures["textures/iraq/stucco13c"] = {64,64};
default_textures["textures/iraq/stucco14"] = {64,64};
default_textures["textures/iraq/stucco14b"] = {64,32};
default_textures["textures/iraq/stucco14bdirt"] = {64,32};
default_textures["textures/iraq/stucco14dirt"] = {64,64};
default_textures["textures/iraq/stucco15"] = {32,32};
default_textures["textures/iraq/stucco15_bot"] = {32,16};
default_textures["textures/iraq/stucco2"] = {64,64};
default_textures["textures/iraq/stucco3"] = {64,64};
default_textures["textures/iraq/stucco3dirt"] = {64,64};
default_textures["textures/iraq/stucco5d"] = {128,128};
default_textures["textures/iraq/stucco6"] = {128,128};
default_textures["textures/iraq/stucco6b"] = {128,64};
default_textures["textures/iraq/stucco7"] = {128,128};
default_textures["textures/iraq/stucco7d"] = {128,128};
default_textures["textures/iraq/stucco8"] = {64,64};
default_textures["textures/iraq/stucco8a"] = {64,32};
default_textures["textures/iraq/stucco8b"] = {64,64};
default_textures["textures/iraq/stucco9"] = {64,64};
default_textures["textures/iraq/stucco9a"] = {64,32};
default_textures["textures/iraq/tabletop"] = {32,32};
default_textures["textures/iraq/trigger"] = {64,64};
default_textures["textures/iraq/trim1"] = {32,32};
default_textures["textures/iraq/trim10"] = {32,16};
default_textures["textures/iraq/trim11"] = {32,16};
default_textures["textures/iraq/trim12"] = {64,32};
default_textures["textures/iraq/trim3"] = {32,32};
default_textures["textures/iraq/trim4"] = {32,32};
default_textures["textures/iraq/trim5"] = {32,32};
default_textures["textures/iraq/trim6alpha"] = {32,32};
default_textures["textures/iraq/trim7"] = {64,32};
default_textures["textures/iraq/trim8"] = {64,32};
default_textures["textures/iraq/trim9"] = {32,16};
default_textures["textures/iraq/warning"] = {32,32};
default_textures["textures/iraq/warning2"] = {32,32};
default_textures["textures/iraq/warning3"] = {32,32};
default_textures["textures/iraq/water"] = {128,128};
default_textures["textures/iraq/water_jim"] = {128,128};
default_textures["textures/iraq/window1_dirt"] = {64,64};
default_textures["textures/iraq/window1_dirtd"] = {64,64};
default_textures["textures/iraq/window2"] = {64,64};
default_textures["textures/iraq/window2d"] = {64,64};
default_textures["textures/iraq/window2_dirt"] = {64,64};
default_textures["textures/iraq/window_dark"] = {32,32};
default_textures["textures/iraq/window_darkd"] = {32,32};
default_textures["textures/iraq/window_streak"] = {64,64};
default_textures["textures/iraq/window_streaked"] = {64,64};
default_textures["textures/iraq/window_streakedd"] = {64,64};
default_textures["textures/iraq/window_streak_d"] = {64,64};
default_textures["textures/iraq/window_wire"] = {64,64};
default_textures["textures/iraq/window_wire2"] = {64,64};
default_textures["textures/iraq/window_wire2_d"] = {64,64};
default_textures["textures/iraq/window_wire_d"] = {64,64};
default_textures["textures/iraq/wood1"] = {64,64};
default_textures["textures/iraq/wood2"] = {64,128};
default_textures["textures/iraq/woodfloor"] = {64,128};
default_textures["textures/iraq/woodplank1"] = {64,32};
default_textures["textures/iraq/woodtrim"] = {32,32};
default_textures["textures/iraq/w_palette1"] = {64,64};
default_textures["textures/iraq/w_palette2"] = {64,64};
default_textures["textures/iraq/w_palettenew"] = {16,16};
default_textures["textures/iraq/w_pallette3"] = {64,64};
default_textures["textures/iraq/w_semiback"] = {64,128};
default_textures["textures/iraq/w_semiside"] = {64,128};
default_textures["textures/iraq/w_semiside2"] = {64,128};
default_textures["textures/siberia/1_2_buildw_beam"] = {128,128};
default_textures["textures/siberia/1_2_floor_grate"] = {64,64};
default_textures["textures/siberia/1_2_ice_tubes"] = {64,64};
default_textures["textures/siberia/1_2_rock"] = {128,128};
default_textures["textures/siberia/1_3_stairside_wood"] = {32,16};
default_textures["textures/siberia/1_basement2b"] = {64,64};
default_textures["textures/siberia/1_basement2d"] = {64,64};
default_textures["textures/siberia/1_bigsnowwall"] = {128,128};
default_textures["textures/siberia/1_bigsnowwallbot"] = {128,64};
default_textures["textures/siberia/1_button_3a"] = {32,64};
default_textures["textures/siberia/1_button_3aa"] = {32,64};
default_textures["textures/siberia/1_button_3b"] = {32,64};
default_textures["textures/siberia/1_button_off"] = {32,32};
default_textures["textures/siberia/1_button_on"] = {32,32};
default_textures["textures/siberia/1_button_on2"] = {32,64};
default_textures["textures/siberia/1_button_on2a"] = {32,64};
default_textures["textures/siberia/1_button_on2b"] = {32,64};
default_textures["textures/siberia/1_canyonb"] = {128,128};
default_textures["textures/siberia/1_conwall_bottom"] = {64,64};
default_textures["textures/siberia/1_conwall_bottom2"] = {64,64};
default_textures["textures/siberia/1_conwall_bottom3"] = {64,64};
default_textures["textures/siberia/1_conwall_bottom_blood"] = {64,64};
default_textures["textures/siberia/1_conwall_top"] = {64,64};
default_textures["textures/siberia/1_conwall_trans"] = {64,64};
default_textures["textures/siberia/1_conwall_trans2"] = {64,64};
default_textures["textures/siberia/1_conwall_trans3"] = {64,64};
default_textures["textures/siberia/1_flare"] = {16,32};
default_textures["textures/siberia/1_grating_alpha"] = {32,32};
default_textures["textures/siberia/1_shinglesnow"] = {64,64};
default_textures["textures/siberia/1_snowall1tile"] = {128,128};
default_textures["textures/siberia/1_snowcata"] = {64,128};
default_textures["textures/siberia/1_snowcataside"] = {32,128};
default_textures["textures/siberia/1_snowcatb"] = {64,64};
default_textures["textures/siberia/1_snowcatb_plain"] = {64,64};
default_textures["textures/siberia/1_snowcatb_snow"] = {64,64};
default_textures["textures/siberia/1_snowcatfloor"] = {64,64};
default_textures["textures/siberia/1_snowcatfloor2"] = {64,64};
default_textures["textures/siberia/1_snowpipe"] = {128,128};
default_textures["textures/siberia/1_snowpipe2"] = {128,128};
default_textures["textures/siberia/1_snowpipeinside"] = {128,128};
default_textures["textures/siberia/1_snowpipetop"] = {128,64};
default_textures["textures/siberia/1_snow_cracked_tga"] = {64,64};
default_textures["textures/siberia/1_stairtop"] = {64,32};
default_textures["textures/siberia/1_stairtrim"] = {16,16};
default_textures["textures/siberia/1_woodfence"] = {64,128};
default_textures["textures/siberia/2_3_ceil1"] = {64,64};
default_textures["textures/siberia/2_3_ceil2"] = {64,64};
default_textures["textures/siberia/2_3_ceil3"] = {64,64};
default_textures["textures/siberia/2_3_door1"] = {64,128};
default_textures["textures/siberia/2_3_door2"] = {64,128};
default_textures["textures/siberia/2_3_door4"] = {64,128};
default_textures["textures/siberia/2_3_door_jam"] = {16,16};
default_textures["textures/siberia/2_3_duct"] = {64,64};
default_textures["textures/siberia/2_3_duct2"] = {64,64};
default_textures["textures/siberia/2_3_floor1"] = {64,64};
default_textures["textures/siberia/2_3_floor2"] = {32,32};
default_textures["textures/siberia/2_3_floor3"] = {32,32};
default_textures["textures/siberia/2_3_floor4"] = {32,32};
default_textures["textures/siberia/2_3_floor6"] = {32,32};
default_textures["textures/siberia/2_3_floor7"] = {32,32};
default_textures["textures/siberia/2_3_floor8"] = {64,64};
default_textures["textures/siberia/2_3_floor_ceil1"] = {64,64};
default_textures["textures/siberia/2_3_floor_ceil2"] = {64,64};
default_textures["textures/siberia/2_3_floor_ceil3"] = {64,64};
default_textures["textures/siberia/2_3_floor_ceil4"] = {64,64};
default_textures["textures/siberia/2_3_floor_ceil5"] = {64,64};
default_textures["textures/siberia/2_3_floor_ceil6"] = {64,64};
default_textures["textures/siberia/2_3_floor_ceil7"] = {64,64};
default_textures["textures/siberia/2_3_floor_ceil8"] = {64,64};
default_textures["textures/siberia/2_3_floor_ceil9"] = {64,64};
default_textures["textures/siberia/2_3_florescent"] = {32,16};
default_textures["textures/siberia/2_3_fuse3"] = {16,32};
default_textures["textures/siberia/2_3_hex_floor"] = {64,64};
default_textures["textures/siberia/2_3_metal_floor1"] = {32,32};
default_textures["textures/siberia/2_3_metal_floor2"] = {32,32};
default_textures["textures/siberia/2_3_sign1"] = {64,32};
default_textures["textures/siberia/2_3_stair_side"] = {16,16};
default_textures["textures/siberia/2_3_stair_side2"] = {16,16};
default_textures["textures/siberia/2_3_stair_top1"] = {32,64};
default_textures["textures/siberia/2_3_tile_big_blue"] = {32,32};
default_textures["textures/siberia/2_3_trim1"] = {32,32};
default_textures["textures/siberia/2_arch"] = {128,64};
default_textures["textures/siberia/2_arch2"] = {64,128};
default_textures["textures/siberia/2_archglass"] = {64,64};
default_textures["textures/siberia/2_archwall"] = {64,64};
default_textures["textures/siberia/2_archwall2"] = {64,32};
default_textures["textures/siberia/2_beam1"] = {64,32};
default_textures["textures/siberia/2_beam1_plain"] = {16,16};
default_textures["textures/siberia/2_beam2"] = {64,32};
default_textures["textures/siberia/2_beam2_plain"] = {16,16};
default_textures["textures/siberia/2_beam2_snow"] = {64,32};
default_textures["textures/siberia/2_beam2_snow2"] = {32,64};
default_textures["textures/siberia/2_beam3"] = {64,32};
default_textures["textures/siberia/2_big_pipe2"] = {64,64};
default_textures["textures/siberia/2_blockwall"] = {64,64};
default_textures["textures/siberia/2_blockwall_b"] = {64,64};
default_textures["textures/siberia/2_blockwall_bottom"] = {64,64};
default_textures["textures/siberia/2_blockwall_c"] = {64,64};
default_textures["textures/siberia/2_brick1"] = {64,64};
default_textures["textures/siberia/2_celingbars"] = {32,32};
default_textures["textures/siberia/2_cementbasetrim"] = {64,64};
default_textures["textures/siberia/2_cementbasetrim2"] = {64,64};
default_textures["textures/siberia/2_cementplain"] = {64,64};
default_textures["textures/siberia/2_cementplain_line"] = {64,64};
default_textures["textures/siberia/2_cementslab"] = {64,64};
default_textures["textures/siberia/2_cementslab_line"] = {64,64};
default_textures["textures/siberia/2_cementslab_snow"] = {64,64};
default_textures["textures/siberia/2_con3"] = {64,64};
default_textures["textures/siberia/2_con4"] = {64,64};
default_textures["textures/siberia/2_concrete1_snow"] = {64,64};
default_textures["textures/siberia/2_concrete2"] = {64,64};
default_textures["textures/siberia/2_concrete2a"] = {64,64};
default_textures["textures/siberia/2_concrete2a_ice1"] = {64,64};
default_textures["textures/siberia/2_concrete2b"] = {64,64};
default_textures["textures/siberia/2_concrete2_bottom"] = {64,64};
default_textures["textures/siberia/2_concrete3"] = {64,64};
default_textures["textures/siberia/2_concrete3a"] = {64,64};
default_textures["textures/siberia/2_concrete3a_snow"] = {64,64};
default_textures["textures/siberia/2_concrete3a_snow2"] = {64,64};
default_textures["textures/siberia/2_concrete3_bottom"] = {64,64};
default_textures["textures/siberia/2_concrete3_snow"] = {64,64};
default_textures["textures/siberia/2_controla"] = {32,64};
default_textures["textures/siberia/2_conwall_snow"] = {128,128};
default_textures["textures/siberia/2_conwall_snowb"] = {128,128};
default_textures["textures/siberia/2_display_a"] = {128,128};
default_textures["textures/siberia/2_display_a_d"] = {128,128};
default_textures["textures/siberia/2_display_b"] = {64,128};
default_textures["textures/siberia/2_display_b_d"] = {64,128};
default_textures["textures/siberia/2_door1"] = {64,128};
default_textures["textures/siberia/2_door2a"] = {64,128};
default_textures["textures/siberia/2_door2b"] = {64,64};
default_textures["textures/siberia/2_door_b"] = {64,128};
default_textures["textures/siberia/2_elevtrack"] = {64,64};
default_textures["textures/siberia/2_elevtrack_side"] = {16,16};
default_textures["textures/siberia/2_gen1"] = {64,128};
default_textures["textures/siberia/2_gen2"] = {64,128};
default_textures["textures/siberia/2_gensidea"] = {64,64};
default_textures["textures/siberia/2_gensideb"] = {64,64};
default_textures["textures/siberia/2_gentile"] = {64,64};
default_textures["textures/siberia/2_gentop"] = {64,128};
default_textures["textures/siberia/2_gratinga"] = {64,128};
default_textures["textures/siberia/2_gratingb"] = {64,64};
default_textures["textures/siberia/2_graytrim"] = {32,32};
default_textures["textures/siberia/2_hall_ceiling"] = {64,64};
default_textures["textures/siberia/2_hall_ceiling_cable"] = {64,64};
default_textures["textures/siberia/2_hall_wall"] = {64,64};
default_textures["textures/siberia/2_hall_wall_bottom"] = {64,16};
default_textures["textures/siberia/2_hall_wall_cable"] = {64,64};
default_textures["textures/siberia/2_metalfloor1"] = {64,64};
default_textures["textures/siberia/2_metalfloor1_snow"] = {64,64};
default_textures["textures/siberia/2_metaltop"] = {32,32};
default_textures["textures/siberia/2_metal_clean"] = {64,64};
default_textures["textures/siberia/2_metal_floor"] = {32,32};
default_textures["textures/siberia/2_orange_paint"] = {16,16};
default_textures["textures/siberia/2_pipe_rusty2"] = {64,32};
default_textures["textures/siberia/2_quaddoor_2a"] = {128,128};
default_textures["textures/siberia/2_quaddoor_2b"] = {16,64};
default_textures["textures/siberia/2_quaddoor_a"] = {128,128};
default_textures["textures/siberia/2_quaddoor_b"] = {16,64};
default_textures["textures/siberia/2_quaddoor_nuke"] = {128,128};
default_textures["textures/siberia/2_roughwall"] = {64,64};
default_textures["textures/siberia/2_shutedoor"] = {64,128};
default_textures["textures/siberia/2_sign2"] = {64,32};
default_textures["textures/siberia/2_sign3"] = {64,32};
default_textures["textures/siberia/2_signa"] = {64,64};
default_textures["textures/siberia/2_signc"] = {64,64};
default_textures["textures/siberia/2_stairtread"] = {32,32};
default_textures["textures/siberia/2_stairtread2"] = {32,32};
default_textures["textures/siberia/2_stair_side2"] = {16,16};
default_textures["textures/siberia/2_steelwall"] = {128,64};
default_textures["textures/siberia/2_steelwall_top"] = {128,32};
default_textures["textures/siberia/2_supportbeam"] = {64,32};
default_textures["textures/siberia/2_supportbeam_snow"] = {64,32};
default_textures["textures/siberia/2_walla_bottom"] = {64,64};
default_textures["textures/siberia/2_walla_top"] = {64,32};
default_textures["textures/siberia/2_wallb_bottom"] = {64,64};
default_textures["textures/siberia/2_wallb_top"] = {64,64};
default_textures["textures/siberia/3_beama"] = {64,32};
default_textures["textures/siberia/3_beamside"] = {32,32};
default_textures["textures/siberia/3_beamtall"] = {32,64};
default_textures["textures/siberia/3_beamtall2"] = {32,64};
default_textures["textures/siberia/3_beamtall2b"] = {32,64};
default_textures["textures/siberia/3_bigate_1"] = {64,64};
default_textures["textures/siberia/3_bigate_10"] = {64,64};
default_textures["textures/siberia/3_bigate_2"] = {64,64};
default_textures["textures/siberia/3_bigate_3"] = {64,64};
default_textures["textures/siberia/3_bigate_4"] = {64,64};
default_textures["textures/siberia/3_bigate_5"] = {64,64};
default_textures["textures/siberia/3_bigate_6"] = {64,64};
default_textures["textures/siberia/3_bigate_7"] = {64,64};
default_textures["textures/siberia/3_bigate_8"] = {64,64};
default_textures["textures/siberia/3_bigate_9"] = {64,64};
default_textures["textures/siberia/3_bigate_trim"] = {16,16};
default_textures["textures/siberia/3_box_side"] = {16,16};
default_textures["textures/siberia/3_brace_1a"] = {64,128};
default_textures["textures/siberia/3_brace_1b"] = {64,128};
default_textures["textures/siberia/3_brace_1top"] = {32,64};
default_textures["textures/siberia/3_brace_2a"] = {64,128};
default_textures["textures/siberia/3_brace_2b"] = {128,128};
default_textures["textures/siberia/3_brace_2c"] = {64,64};
default_textures["textures/siberia/3_brace_2top"] = {64,64};
default_textures["textures/siberia/3_brace_back"] = {64,64};
default_textures["textures/siberia/3_brace_back2"] = {64,64};
default_textures["textures/siberia/3_brace_hold1a"] = {16,64};
default_textures["textures/siberia/3_brace_hold1b"] = {32,32};
default_textures["textures/siberia/3_brace_hold1c"] = {16,32};
default_textures["textures/siberia/3_brace_hold1d"] = {64,32};
default_textures["textures/siberia/3_brace_plain"] = {16,16};
default_textures["textures/siberia/3_bridgein_ceil"] = {64,64};
default_textures["textures/siberia/3_bridgein_ceilside"] = {16,16};
default_textures["textures/siberia/3_bridgein_ceiltop"] = {32,32};
default_textures["textures/siberia/3_bridgeout_dark2"] = {32,32};
default_textures["textures/siberia/3_bridgeout_side"] = {64,128};
default_textures["textures/siberia/3_bridge_back2"] = {64,16};
default_textures["textures/siberia/3_bridge_ceil"] = {64,32};
default_textures["textures/siberia/3_bridge_ceilside"] = {64,16};
default_textures["textures/siberia/3_bridge_mid1"] = {64,32};
default_textures["textures/siberia/3_bridge_mid2"] = {64,128};
default_textures["textures/siberia/3_bridge_plain"] = {16,16};
default_textures["textures/siberia/3_bridge_side"] = {64,128};
default_textures["textures/siberia/3_bridge_top1"] = {64,64};
default_textures["textures/siberia/3_bridge_top2"] = {64,16};
default_textures["textures/siberia/3_bridge_topside"] = {16,64};
default_textures["textures/siberia/3_brushed_metal"] = {32,32};
default_textures["textures/siberia/3_brushed_metal1"] = {64,64};
default_textures["textures/siberia/3_cleanwall1"] = {64,128};
default_textures["textures/siberia/3_cleanwall2"] = {64,128};
default_textures["textures/siberia/3_cleanwall4"] = {64,128};
default_textures["textures/siberia/3_controla"] = {64,128};
default_textures["textures/siberia/3_controlad"] = {64,128};
default_textures["textures/siberia/3_controlb"] = {64,128};
default_textures["textures/siberia/3_controlbd"] = {64,128};
default_textures["textures/siberia/3_controlboxa1"] = {64,64};
default_textures["textures/siberia/3_controlboxa2"] = {64,32};
default_textures["textures/siberia/3_controlboxa3"] = {64,32};
default_textures["textures/siberia/3_controlboxa3_d"] = {64,32};
default_textures["textures/siberia/3_controlboxa4"] = {64,32};
default_textures["textures/siberia/3_controlboxb1"] = {64,64};
default_textures["textures/siberia/3_controlboxb2"] = {64,32};
default_textures["textures/siberia/3_controlboxb3"] = {64,32};
default_textures["textures/siberia/3_controlboxb3d"] = {64,32};
default_textures["textures/siberia/3_controlboxb4"] = {64,32};
default_textures["textures/siberia/3_controlboxb4_d"] = {64,32};
default_textures["textures/siberia/3_controlboxb5"] = {64,32};
default_textures["textures/siberia/3_controlb_d2"] = {64,128};
default_textures["textures/siberia/3_controlpanelaa"] = {64,128};
default_textures["textures/siberia/3_controlpanelaa_d"] = {64,128};
default_textures["textures/siberia/3_controlpanelab"] = {128,128};
default_textures["textures/siberia/3_controlpanelab_d"] = {128,128};
default_textures["textures/siberia/3_controlpanelac"] = {64,128};
default_textures["textures/siberia/3_controlpanelacside"] = {32,128};
default_textures["textures/siberia/3_controlpanelac_d"] = {64,128};
default_textures["textures/siberia/3_control_dk"] = {16,16};
default_textures["textures/siberia/3_cpanelbox"] = {64,32};
default_textures["textures/siberia/3_cpanelbox_d"] = {64,32};
default_textures["textures/siberia/3_cpanel_a"] = {64,32};
default_textures["textures/siberia/3_cpanel_ad"] = {64,32};
default_textures["textures/siberia/3_cpanel_b"] = {64,16};
default_textures["textures/siberia/3_cpanel_lever"] = {64,128};
default_textures["textures/siberia/3_desk_bluegrey"] = {16,16};
default_textures["textures/siberia/3_desk_medgrey"] = {16,16};
default_textures["textures/siberia/3_desk_panel"] = {32,32};
default_textures["textures/siberia/3_desk_panel2"] = {32,32};
default_textures["textures/siberia/3_desk_screen"] = {64,64};
default_textures["textures/siberia/3_desk_screen2"] = {32,64};
default_textures["textures/siberia/3_desk_screen2_d"] = {32,64};
default_textures["textures/siberia/3_desk_screen_b"] = {64,64};
default_textures["textures/siberia/3_desk_screen_bd"] = {64,64};
default_textures["textures/siberia/3_desk_screen_c"] = {64,64};
default_textures["textures/siberia/3_desk_screen_cd"] = {64,64};
default_textures["textures/siberia/3_desk_screen_d"] = {64,64};
default_textures["textures/siberia/3_desk_top"] = {16,16};
default_textures["textures/siberia/3_desk_top_keys"] = {32,32};
default_textures["textures/siberia/3_desk_top_keysd"] = {32,32};
default_textures["textures/siberia/3_desk_white"] = {32,32};
default_textures["textures/siberia/3_doora1"] = {128,128};
default_textures["textures/siberia/3_doora2"] = {128,128};
default_textures["textures/siberia/3_doorb"] = {128,128};
default_textures["textures/siberia/3_doorb_scott"] = {128,128};
default_textures["textures/siberia/3_doorc1a"] = {128,128};
default_textures["textures/siberia/3_doorc1b"] = {128,128};
default_textures["textures/siberia/3_doorc2a"] = {128,128};
default_textures["textures/siberia/3_doorc2b"] = {128,128};
default_textures["textures/siberia/3_doorframe"] = {128,128};
default_textures["textures/siberia/3_doorframe_a"] = {128,128};
default_textures["textures/siberia/3_doorframe_a2"] = {128,128};
default_textures["textures/siberia/3_doorframe_b"] = {128,128};
default_textures["textures/siberia/3_doorframe_c"] = {64,128};
default_textures["textures/siberia/3_doorframe_d"] = {128,128};
default_textures["textures/siberia/3_doorglass"] = {128,128};
default_textures["textures/siberia/3_doorglass2"] = {64,128};
default_textures["textures/siberia/3_door_d1"] = {128,128};
default_textures["textures/siberia/3_door_d1b"] = {128,128};
default_textures["textures/siberia/3_door_rectangular"] = {64,128};
default_textures["textures/siberia/3_door_trim1"] = {32,32};
default_textures["textures/siberia/3_door_trim2"] = {32,32};
default_textures["textures/siberia/3_eleceil"] = {32,32};
default_textures["textures/siberia/3_elefloor"] = {32,32};
default_textures["textures/siberia/3_elepanel"] = {32,32};
default_textures["textures/siberia/3_elewall_a"] = {64,64};
default_textures["textures/siberia/3_elewall_b"] = {64,64};
default_textures["textures/siberia/3_elexit"] = {128,128};
default_textures["textures/siberia/3_floorcenter"] = {128,128};
default_textures["textures/siberia/3_fusebox"] = {32,64};
default_textures["textures/siberia/3_fusebox_d"] = {32,64};
default_textures["textures/siberia/3_light_ceil"] = {32,64};
default_textures["textures/siberia/3_light_disco"] = {32,32};
default_textures["textures/siberia/3_light_wall"] = {32,64};
default_textures["textures/siberia/3_locker"] = {16,16};
default_textures["textures/siberia/3_missile_a"] = {128,128};
default_textures["textures/siberia/3_missile_b"] = {128,128};
default_textures["textures/siberia/3_missile_b2"] = {128,128};
default_textures["textures/siberia/3_missile_bottom"] = {64,64};
default_textures["textures/siberia/3_missile_c"] = {128,128};
default_textures["textures/siberia/3_missile_d"] = {128,64};
default_textures["textures/siberia/3_missile_e"] = {128,32};
default_textures["textures/siberia/3_office_wall"] = {64,64};
default_textures["textures/siberia/3_panel5"] = {64,32};
default_textures["textures/siberia/3_panel5d"] = {64,32};
default_textures["textures/siberia/3_pipetile_1"] = {64,64};
default_textures["textures/siberia/3_pipetile_2"] = {64,64};
default_textures["textures/siberia/3_pipetile_3"] = {64,64};
default_textures["textures/siberia/3_platform"] = {128,128};
default_textures["textures/siberia/3_railing"] = {32,32};
default_textures["textures/siberia/3_railingb"] = {64,64};
default_textures["textures/siberia/3_railingtop"] = {32,16};
default_textures["textures/siberia/3_railingtop_snow"] = {32,16};
default_textures["textures/siberia/3_railing_snow"] = {32,32};
default_textures["textures/siberia/3_recess_lower"] = {64,64};
default_textures["textures/siberia/3_recess_upper"] = {32,32};
default_textures["textures/siberia/3_sign2"] = {32,32};
default_textures["textures/siberia/3_sign3"] = {64,64};
default_textures["textures/siberia/3_sign4"] = {64,64};
default_textures["textures/siberia/3_slime"] = {64,64};
default_textures["textures/siberia/3_steelwall3"] = {64,128};
default_textures["textures/siberia/3_steel_brushed"] = {64,64};
default_textures["textures/siberia/3_steel_brushed_dark"] = {64,64};
default_textures["textures/siberia/3_subdoor"] = {64,128};
default_textures["textures/siberia/3_tank1"] = {64,128};
default_textures["textures/siberia/3_tank1_dirty"] = {64,128};
default_textures["textures/siberia/3_tanklid"] = {64,64};
default_textures["textures/siberia/3_tanklid_dirty"] = {64,64};
default_textures["textures/siberia/3_tanktop"] = {32,32};
default_textures["textures/siberia/3_tanktop_dirty"] = {32,32};
default_textures["textures/siberia/3_trim1"] = {32,32};
default_textures["textures/siberia/3_trimnew"] = {32,32};
default_textures["textures/siberia/3_warning"] = {32,32};
default_textures["textures/siberia/3_warning_snow"] = {32,32};
default_textures["textures/siberia/3_w_2greys_bottom"] = {64,32};
default_textures["textures/siberia/3_w_2greys_top"] = {64,64};
default_textures["textures/siberia/3_w_greygold"] = {64,128};
default_textures["textures/siberia/3_w_ltgrey"] = {64,128};
default_textures["textures/siberia/3_w_medgrey"] = {64,128};
default_textures["textures/siberia/3_w_midgrey"] = {64,128};
default_textures["textures/siberia/3_w_whitegold"] = {64,128};
default_textures["textures/siberia/3_w_white_2"] = {64,128};
default_textures["textures/siberia/3_w_white_3"] = {64,128};
default_textures["textures/siberia/3_w_white_bottom"] = {64,32};
default_textures["textures/siberia/3_w_white_top"] = {64,64};
default_textures["textures/siberia/alpha"] = {16,16};
default_textures["textures/siberia/bars1"] = {64,64};
default_textures["textures/siberia/bars1_snow"] = {64,64};
default_textures["textures/siberia/bars2"] = {64,64};
default_textures["textures/siberia/catwalk1"] = {64,64};
default_textures["textures/siberia/catwalk2"] = {64,64};
default_textures["textures/siberia/catwalk2_snow"] = {64,64};
default_textures["textures/siberia/catwalk4"] = {64,64};
default_textures["textures/siberia/chainlink"] = {32,32};
default_textures["textures/siberia/clip"] = {64,64};
default_textures["textures/siberia/con1"] = {64,64};
default_textures["textures/siberia/con1_snow"] = {64,64};
default_textures["textures/siberia/con2"] = {64,64};
default_textures["textures/siberia/con3"] = {64,64};
default_textures["textures/siberia/con4"] = {64,64};
default_textures["textures/siberia/crate1"] = {64,64};
default_textures["textures/siberia/crate2"] = {64,64};
default_textures["textures/siberia/crate3"] = {64,64};
default_textures["textures/siberia/crate4"] = {64,64};
default_textures["textures/siberia/crate5"] = {64,64};
default_textures["textures/siberia/ctfb_base"] = {64,64};
default_textures["textures/siberia/ctfr_base"] = {64,64};
default_textures["textures/siberia/ctfr_lightf"] = {64,32};
default_textures["textures/siberia/dm_cabin_roof"] = {64,64};
default_textures["textures/siberia/dm_cabin_roof2"] = {64,64};
default_textures["textures/siberia/d_genericsmooth"] = {64,64};
default_textures["textures/siberia/d_metalrough"] = {64,64};
default_textures["textures/siberia/d_metalsmooth"] = {64,64};
default_textures["textures/siberia/d_stonecracked"] = {64,64};
default_textures["textures/siberia/d_stoneholey"] = {64,64};
default_textures["textures/siberia/d_stonerough"] = {128,128};
default_textures["textures/siberia/d_stonesmooth"] = {64,64};
default_textures["textures/siberia/d_stucco"] = {64,64};
default_textures["textures/siberia/d_woodrough"] = {64,64};
default_textures["textures/siberia/d_woodsmooth"] = {64,64};
default_textures["textures/siberia/env1a"] = {256,256};
default_textures["textures/siberia/env1b"] = {256,256};
default_textures["textures/siberia/env1c"] = {256,256};
default_textures["textures/siberia/girder"] = {64,64};
default_textures["textures/siberia/glass_bulletproof"] = {64,64};
default_textures["textures/siberia/glass_new"] = {64,64};
default_textures["textures/siberia/glass_new2"] = {128,128};
default_textures["textures/siberia/grating_new"] = {32,32};
default_textures["textures/siberia/hint"] = {64,64};
default_textures["textures/siberia/icewall_c"] = {128,128};
default_textures["textures/siberia/ice_wall1"] = {64,64};
default_textures["textures/siberia/ladder_rung"] = {32,16};
default_textures["textures/siberia/lightb"] = {64,32};
default_textures["textures/siberia/lightc"] = {64,32};
default_textures["textures/siberia/lightd"] = {32,32};
default_textures["textures/siberia/lighte"] = {32,32};
default_textures["textures/siberia/lightf"] = {64,32};
default_textures["textures/siberia/lightg"] = {64,32};
default_textures["textures/siberia/lighth"] = {32,32};
default_textures["textures/siberia/lighti"] = {32,32};
default_textures["textures/siberia/lightj"] = {64,32};
default_textures["textures/siberia/lighttile_a"] = {16,32};
default_textures["textures/siberia/lighttile_end"] = {16,32};
default_textures["textures/siberia/light_blue6"] = {32,32};
default_textures["textures/siberia/light_blue7"] = {32,32};
default_textures["textures/siberia/light_blue8"] = {32,32};
default_textures["textures/siberia/light_green2"] = {32,32};
default_textures["textures/siberia/light_orange1"] = {32,32};
default_textures["textures/siberia/light_orange3"] = {32,32};
default_textures["textures/siberia/light_orange4"] = {32,32};
default_textures["textures/siberia/light_orange6"] = {32,32};
default_textures["textures/siberia/light_small2"] = {16,16};
default_textures["textures/siberia/light_yellow1"] = {32,32};
default_textures["textures/siberia/light_yellow3"] = {32,32};
default_textures["textures/siberia/light_yellow6"] = {32,32};
default_textures["textures/siberia/med_door"] = {32,32};
default_textures["textures/siberia/med_inside"] = {32,32};
default_textures["textures/siberia/med_inside_used"] = {32,32};
default_textures["textures/siberia/med_red"] = {16,16};
default_textures["textures/siberia/med_sides"] = {16,16};
default_textures["textures/siberia/med_stripe"] = {16,16};
default_textures["textures/siberia/metal1"] = {64,64};
default_textures["textures/siberia/metal1_snow"] = {64,64};
default_textures["textures/siberia/metal2"] = {32,32};
default_textures["textures/siberia/metal_dark"] = {32,32};
default_textures["textures/siberia/metal_dark2"] = {32,32};
default_textures["textures/siberia/metal_dark2_snow"] = {32,32};
default_textures["textures/siberia/metal_plate"] = {64,64};
default_textures["textures/siberia/metal_smooth"] = {64,64};
default_textures["textures/siberia/no_draw"] = {64,64};
default_textures["textures/siberia/origin"] = {64,64};
default_textures["textures/siberia/rubberfloor"] = {32,32};
default_textures["textures/siberia/rubberfloor2"] = {32,32};
default_textures["textures/siberia/skip"] = {64,64};
default_textures["textures/siberia/sky"] = {64,64};
default_textures["textures/siberia/snow10"] = {128,128};
default_textures["textures/siberia/snow2"] = {128,128};
default_textures["textures/siberia/snow4"] = {64,64};
default_textures["textures/siberia/snow5"] = {128,128};
default_textures["textures/siberia/snow7"] = {128,128};
default_textures["textures/siberia/snow8"] = {128,128};
default_textures["textures/siberia/snow9"] = {128,128};
default_textures["textures/siberia/snow_patchy"] = {64,64};
default_textures["textures/siberia/snow_wall_smooth"] = {128,128};
default_textures["textures/siberia/tile1"] = {64,64};
default_textures["textures/siberia/trigger"] = {64,64};
default_textures["textures/siberia/trim2"] = {32,32};
default_textures["textures/siberia/t_carbackmid"] = {64,128};
default_textures["textures/siberia/t_carbackside"] = {64,128};
default_textures["textures/siberia/t_cardoor"] = {64,128};
default_textures["textures/siberia/t_carside"] = {64,128};
default_textures["textures/siberia/t_conpipe_trim"] = {16,128};
default_textures["textures/siberia/t_floor1"] = {64,64};
default_textures["textures/siberia/t_floor1_snow"] = {64,64};
default_textures["textures/siberia/t_gmetal_end"] = {64,64};
default_textures["textures/siberia/t_light1"] = {32,32};
default_textures["textures/siberia/t_track_bed_snow"] = {64,128};
default_textures["textures/siberia/t_wheel"] = {32,32};
default_textures["textures/siberia/water"] = {128,128};
default_textures["textures/siberia/w_fencewheel"] = {32,32};
default_textures["textures/sky/bos2_side1"] = {512,512};
default_textures["textures/sky/bos2_side2"] = {512,512};
default_textures["textures/sky/bos_side1"] = {512,512};
default_textures["textures/sky/bos_side2"] = {512,512};
default_textures["textures/sky/bos_up"] = {512,512};
default_textures["textures/sky/irq2_side1"] = {512,512};
default_textures["textures/sky/irq2_side2"] = {512,512};
default_textures["textures/sky/irq2_up"] = {512,512};
default_textures["textures/sky/irq_1"] = {512,512};
default_textures["textures/sky/irq_2"] = {512,512};
default_textures["textures/sky/irq_up"] = {512,512};
default_textures["textures/sky/ny1"] = {512,512};
default_textures["textures/sky/ny2"] = {512,512};
default_textures["textures/sky/ny_up"] = {512,512};
default_textures["textures/sky/oil_1"] = {512,512};
default_textures["textures/sky/oil_2"] = {512,512};
default_textures["textures/sky/oil_up"] = {512,512};
default_textures["textures/sky/sib_dn"] = {512,512};
default_textures["textures/sky/sib_side"] = {512,512};
default_textures["textures/sky/sib_up"] = {512,512};
default_textures["textures/sky/tok1_side"] = {512,512};
default_textures["textures/sky/tok1_side2"] = {512,512};
default_textures["textures/sky/tok1_up"] = {512,512};
default_textures["textures/sky/tok2_dn"] = {512,512};
default_textures["textures/sky/tok2_side"] = {512,512};
default_textures["textures/sky/tok2_side2"] = {512,512};
default_textures["textures/sky/tok2_up"] = {512,512};
default_textures["textures/sky/train_sides"] = {512,512};
default_textures["textures/sky/train_top"] = {512,512};
default_textures["textures/sky/tut_side"] = {512,512};
default_textures["textures/sky/tut_up"] = {512,512};
default_textures["textures/sky/ugn_side"] = {512,512};
default_textures["textures/sky/ugn_up"] = {512,512};
default_textures["/sprites/beam"] = {64,64};
default_textures["/sprites/bhole2"] = {32,32};
default_textures["/sprites/bhole_glass3"] = {32,32};
default_textures["/sprites/bhole_mtl"] = {32,32};
default_textures["/sprites/bhole_wd"] = {32,32};
default_textures["/sprites/bldglob4"] = {64,64};
default_textures["/sprites/bldglob5"] = {64,64};
default_textures["/sprites/bldglob6"] = {64,64};
default_textures["/sprites/bldsprinkle"] = {64,64};
default_textures["/sprites/blood_pool"] = {64,64};
default_textures["/sprites/blood_pool2"] = {64,64};
default_textures["/sprites/bluestreak"] = {64,16};
default_textures["/sprites/boom3"] = {64,64};
default_textures["/sprites/boom5"] = {64,64};
default_textures["/sprites/bubble"] = {16,16};
default_textures["/sprites/dent"] = {64,64};
default_textures["/sprites/dirt"] = {64,64};
default_textures["/sprites/expfire1"] = {64,64};
default_textures["/sprites/expfire11"] = {64,64};
default_textures["/sprites/expfire2"] = {64,64};
default_textures["/sprites/expfire3"] = {64,64};
default_textures["/sprites/expfire4"] = {64,64};
default_textures["/sprites/explode1"] = {32,32};
default_textures["/sprites/fireburst"] = {64,64};
default_textures["/sprites/firestreak"] = {64,16};
default_textures["/sprites/flameball_y"] = {64,64};
default_textures["/sprites/flare"] = {64,64};
default_textures["/sprites/flare2"] = {64,64};
default_textures["/sprites/flare4"] = {64,64};
default_textures["/sprites/flareblue"] = {64,64};
default_textures["/sprites/flareblue2"] = {64,32};
default_textures["/sprites/flash1"] = {64,64};
default_textures["/sprites/footstep"] = {32,32};
default_textures["/sprites/footstp2"] = {32,32};
default_textures["/sprites/gb_fire01"] = {64,64};
default_textures["/sprites/gb_fire02"] = {64,64};
default_textures["/sprites/gb_fire03"] = {64,64};
default_textures["/sprites/gb_flameball01"] = {64,64};
default_textures["/sprites/gb_jet01"] = {64,64};
default_textures["/sprites/gb_mflash"] = {32,32};
default_textures["/sprites/gb_orbit01"] = {64,64};
default_textures["/sprites/gb_orbit02"] = {64,64};
default_textures["/sprites/gb_orbit10"] = {64,64};
default_textures["/sprites/gb_sgmflash01"] = {64,64};
default_textures["/sprites/gb_sgmflashsprk"] = {64,64};
default_textures["/sprites/gb_shockring01"] = {128,128};
default_textures["/sprites/gb_smoke01"] = {64,64};
default_textures["/sprites/gb_trail02"] = {64,64};
default_textures["/sprites/gb_water"] = {64,64};
default_textures["/sprites/gb_water2"] = {128,128};
default_textures["/sprites/gb_waterdrip"] = {64,64};
default_textures["/sprites/gb_waterrings"] = {256,256};
default_textures["/sprites/lightning"] = {64,16};
default_textures["/sprites/lightning_y"] = {64,16};
default_textures["/sprites/lit"] = {64,16};
default_textures["/sprites/lita"] = {64,16};
default_textures["/sprites/mflash"] = {32,32};
default_textures["/sprites/mflash3"] = {32,32};
default_textures["/sprites/mflash4"] = {32,32};
default_textures["/sprites/mflash8"] = {32,32};
default_textures["/sprites/minimiflash1"] = {64,64};
default_textures["/sprites/minimiflash2"] = {64,64};
default_textures["/sprites/minimiflash3"] = {64,64};
default_textures["/sprites/pipeleft"] = {16,16};
default_textures["/sprites/pipe_sink"] = {16,16};
default_textures["/sprites/powder"] = {64,64};
default_textures["/sprites/puddle"] = {64,64};
default_textures["/sprites/rain"] = {32,16};
default_textures["/sprites/ring1"] = {64,64};
default_textures["/sprites/ring2"] = {64,64};
default_textures["/sprites/ripple"] = {64,64};
default_textures["/sprites/scorch"] = {64,64};
default_textures["/sprites/scorchwht"] = {64,64};
default_textures["/sprites/shock2"] = {64,64};
default_textures["/sprites/shock3"] = {64,64};
default_textures["/sprites/shock4"] = {64,64};
default_textures["/sprites/slash"] = {64,32};
default_textures["/sprites/smkgrn"] = {64,64};
default_textures["/sprites/smkgry"] = {64,64};
default_textures["/sprites/smkwht"] = {64,64};
default_textures["/sprites/smoke1"] = {64,64};
default_textures["/sprites/smoke2"] = {64,64};
default_textures["/sprites/smoke3"] = {64,64};
default_textures["/sprites/snow"] = {16,16};
default_textures["/sprites/sparkpart"] = {64,64};
default_textures["/sprites/sparkpartblue"] = {16,16};
default_textures["/sprites/toxic_ooze"] = {64,64};
default_textures["/sprites/water"] = {64,64};
default_textures["/sprites/waterdrop"] = {32,32};
default_textures["/sprites/whitestreak"] = {64,16};
default_textures["textures/subway/1_2_subfloor1"] = {64,64};
default_textures["textures/subway/1_bathroom_floor"] = {64,64};
default_textures["textures/subway/1_bathroom_floor_drain"] = {64,64};
default_textures["textures/subway/1_bathroom_floor_edge"] = {64,64};
default_textures["textures/subway/1_bathroom_men"] = {64,64};
default_textures["textures/subway/1_bathroom_men_bottom"] = {32,32};
default_textures["textures/subway/1_bathroom_men_damage"] = {64,64};
default_textures["textures/subway/1_bathroom_men_graf2"] = {64,64};
default_textures["textures/subway/1_bathroom_men_graf3"] = {64,64};
default_textures["textures/subway/1_bathroom_men_graf4"] = {64,64};
default_textures["textures/subway/1_bathroom_men_nos"] = {64,64};
default_textures["textures/subway/1_bathroom_men_top"] = {32,16};
default_textures["textures/subway/1_bathroom_women"] = {64,64};
default_textures["textures/subway/1_bathroom_women_bottom"] = {32,32};
default_textures["textures/subway/1_bathroom_women_graf1"] = {64,64};
default_textures["textures/subway/1_bathroom_women_nos"] = {64,64};
default_textures["textures/subway/1_bathroom_women_top"] = {32,16};
default_textures["textures/subway/1_bath_stall"] = {64,64};
default_textures["textures/subway/1_bath_stalldoor"] = {64,64};
default_textures["textures/subway/1_bath_stallurinal"] = {32,64};
default_textures["textures/subway/1_bath_towels"] = {64,128};
default_textures["textures/subway/1_beam1"] = {64,32};
default_textures["textures/subway/1_beam2"] = {64,32};
default_textures["textures/subway/1_beam2_plain"] = {16,16};
default_textures["textures/subway/1_beam3"] = {64,32};
default_textures["textures/subway/1_beam4"] = {64,32};
default_textures["textures/subway/1_black"] = {16,16};
default_textures["textures/subway/1_build_win"] = {64,64};
default_textures["textures/subway/1_build_wind"] = {64,64};
default_textures["textures/subway/1_clock2"] = {64,64};
default_textures["textures/subway/1_clock2d"] = {64,64};
default_textures["textures/subway/1_concrete_floor"] = {128,128};
default_textures["textures/subway/1_concrete_floor2"] = {128,128};
default_textures["textures/subway/1_cork_ceiling"] = {64,64};
default_textures["textures/subway/1_danger"] = {128,32};
default_textures["textures/subway/1_danger2"] = {128,32};
default_textures["textures/subway/1_door_jam"] = {16,16};
default_textures["textures/subway/1_duct"] = {64,64};
default_textures["textures/subway/1_duct2"] = {64,64};
default_textures["textures/subway/1_duct_rust"] = {64,64};
default_textures["textures/subway/1_florescent"] = {32,16};
default_textures["textures/subway/1_fusebox"] = {32,64};
default_textures["textures/subway/1_fusebox_side"] = {16,16};
default_textures["textures/subway/1_grating"] = {64,64};
default_textures["textures/subway/1_lobby1"] = {64,64};
default_textures["textures/subway/1_lobby1a"] = {64,64};
default_textures["textures/subway/1_lobby1aa"] = {64,64};
default_textures["textures/subway/1_lobby1ab"] = {64,64};
default_textures["textures/subway/1_lobby1ac"] = {64,64};
default_textures["textures/subway/1_lobby1ad"] = {64,64};
default_textures["textures/subway/1_lobby1ae"] = {64,64};
default_textures["textures/subway/1_lobby1af"] = {128,64};
default_textures["textures/subway/1_lobby1ag"] = {128,64};
default_textures["textures/subway/1_lobby1agb"] = {128,64};
default_textures["textures/subway/1_lobby1ah"] = {64,64};
default_textures["textures/subway/1_lobby2"] = {64,64};
default_textures["textures/subway/1_lobby3"] = {64,64};
default_textures["textures/subway/1_lobby4"] = {64,64};
default_textures["textures/subway/1_lobby_floor"] = {64,64};
default_textures["textures/subway/1_lobby_floor_edge"] = {64,64};
default_textures["textures/subway/1_map"] = {128,64};
default_textures["textures/subway/1_mapd"] = {128,64};
default_textures["textures/subway/1_mtrim1"] = {64,32};
default_textures["textures/subway/1_news_front1"] = {64,64};
default_textures["textures/subway/1_news_front1d"] = {64,64};
default_textures["textures/subway/1_news_frontb"] = {64,64};
default_textures["textures/subway/1_news_frontbd"] = {64,64};
default_textures["textures/subway/1_news_side1"] = {32,64};
default_textures["textures/subway/1_news_sideb"] = {32,64};
default_textures["textures/subway/1_news_top1"] = {32,64};
default_textures["textures/subway/1_news_topb"] = {32,64};
default_textures["textures/subway/1_poster1"] = {64,64};
default_textures["textures/subway/1_poster2"] = {64,64};
default_textures["textures/subway/1_poster2a"] = {64,64};
default_textures["textures/subway/1_poster3"] = {64,64};
default_textures["textures/subway/1_poster3a"] = {64,64};
default_textures["textures/subway/1_poster_top"] = {64,16};
default_textures["textures/subway/1_restroom1"] = {32,32};
default_textures["textures/subway/1_restroom2"] = {32,32};
default_textures["textures/subway/1_sign1"] = {128,32};
default_textures["textures/subway/1_sign1d"] = {128,32};
default_textures["textures/subway/1_sign2"] = {128,32};
default_textures["textures/subway/1_sign2d"] = {128,32};
default_textures["textures/subway/1_sign3"] = {128,32};
default_textures["textures/subway/1_sign3d"] = {128,32};
default_textures["textures/subway/1_sign4"] = {128,32};
default_textures["textures/subway/1_sign4b"] = {128,32};
default_textures["textures/subway/1_sign5"] = {64,64};
default_textures["textures/subway/1_sign6"] = {64,64};
default_textures["textures/subway/1_sign7"] = {64,32};
default_textures["textures/subway/1_sign8"] = {128,32};
default_textures["textures/subway/1_soda_front_a"] = {64,64};
default_textures["textures/subway/1_soda_front_a3"] = {64,64};
default_textures["textures/subway/1_soda_front_b"] = {64,32};
default_textures["textures/subway/1_soda_front_b3"] = {64,32};
default_textures["textures/subway/1_soda_side_a"] = {64,64};
default_textures["textures/subway/1_soda_side_ad"] = {64,64};
default_textures["textures/subway/1_soda_side_b"] = {64,32};
default_textures["textures/subway/1_soda_side_bd"] = {64,32};
default_textures["textures/subway/1_soda_top"] = {64,64};
default_textures["textures/subway/1_stair_side1"] = {16,16};
default_textures["textures/subway/1_stair_side2"] = {16,16};
default_textures["textures/subway/1_stair_top1"] = {32,64};
default_textures["textures/subway/1_static1"] = {64,64};
default_textures["textures/subway/1_static2"] = {64,64};
default_textures["textures/subway/1_static3"] = {64,64};
default_textures["textures/subway/1_static_tv"] = {32,32};
default_textures["textures/subway/1_tile"] = {64,64};
default_textures["textures/subway/1_tilegraf14"] = {64,64};
default_textures["textures/subway/1_tilegraf15"] = {64,64};
default_textures["textures/subway/1_tilegraf15b"] = {64,64};
default_textures["textures/subway/1_tilegraf16"] = {64,64};
default_textures["textures/subway/1_tilegraf17"] = {64,64};
default_textures["textures/subway/1_tilegraf18"] = {64,64};
default_textures["textures/subway/1_tilegraf19"] = {128,64};
default_textures["textures/subway/1_tilegraf19b"] = {128,64};
default_textures["textures/subway/1_tilegraf20"] = {128,64};
default_textures["textures/subway/1_tile_big"] = {32,32};
default_textures["textures/subway/1_tile_big_blue"] = {32,32};
default_textures["textures/subway/1_tile_big_green"] = {32,32};
default_textures["textures/subway/1_tile_blood"] = {64,64};
default_textures["textures/subway/1_tile_bottom"] = {32,32};
default_textures["textures/subway/1_tile_damage1"] = {64,64};
default_textures["textures/subway/1_tile_damage2"] = {64,64};
default_textures["textures/subway/1_tile_graf1"] = {64,64};
default_textures["textures/subway/1_tile_graf10"] = {128,64};
default_textures["textures/subway/1_tile_graf10b"] = {128,64};
default_textures["textures/subway/1_tile_graf11"] = {128,64};
default_textures["textures/subway/1_tile_graf11b"] = {128,64};
default_textures["textures/subway/1_tile_graf12"] = {64,64};
default_textures["textures/subway/1_tile_graf12b"] = {64,64};
default_textures["textures/subway/1_tile_graf13"] = {64,64};
default_textures["textures/subway/1_tile_graf2"] = {64,64};
default_textures["textures/subway/1_tile_graf2b"] = {64,64};
default_textures["textures/subway/1_tile_graf3"] = {64,64};
default_textures["textures/subway/1_tile_graf4"] = {64,64};
default_textures["textures/subway/1_tile_graf5"] = {64,64};
default_textures["textures/subway/1_tile_graf5b"] = {64,64};
default_textures["textures/subway/1_tile_graf6"] = {64,64};
default_textures["textures/subway/1_tile_graf7"] = {64,64};
default_textures["textures/subway/1_tile_graf8"] = {64,64};
default_textures["textures/subway/1_tile_graf9"] = {64,64};
default_textures["textures/subway/1_tile_graf9b"] = {64,64};
default_textures["textures/subway/1_tile_middle"] = {32,32};
default_textures["textures/subway/1_tile_top"] = {32,16};
default_textures["textures/subway/1_trim1"] = {32,32};
default_textures["textures/subway/1_tunnelfloor_1"] = {32,32};
default_textures["textures/subway/1_tunnelfloor_2"] = {32,32};
default_textures["textures/subway/1_tunneltrack_1"] = {64,64};
default_textures["textures/subway/1_tunneltrack_2"] = {64,64};
default_textures["textures/subway/1_tunneltrack_3"] = {64,64};
default_textures["textures/subway/1_tunneltrack_end1"] = {64,64};
default_textures["textures/subway/1_tunneltrack_end2"] = {64,64};
default_textures["textures/subway/1_tunneltrack_plain"] = {64,64};
default_textures["textures/subway/1_tunnelwall"] = {64,64};
default_textures["textures/subway/1_tunnelwall_2"] = {64,64};
default_textures["textures/subway/1_tunnelwall_3"] = {64,64};
default_textures["textures/subway/1_tunnelwall_floor"] = {64,32};
default_textures["textures/subway/1_tunnelwall_pipe"] = {64,64};
default_textures["textures/subway/1_tunnel_ceiling"] = {64,64};
default_textures["textures/subway/1_tunnel_ceiling2"] = {64,64};
default_textures["textures/subway/1_tunnel_ceiling3"] = {64,64};
default_textures["textures/subway/1_tunnel_ceiling4"] = {64,64};
default_textures["textures/subway/2_anglebrk1"] = {64,64};
default_textures["textures/subway/2_boiler_side"] = {64,64};
default_textures["textures/subway/2_brick3a"] = {64,64};
default_textures["textures/subway/2_brick4"] = {64,64};
default_textures["textures/subway/2_brick4bot"] = {64,64};
default_textures["textures/subway/2_brick5"] = {64,64};
default_textures["textures/subway/2_brick5bot"] = {64,64};
default_textures["textures/subway/2_cinderbloc"] = {32,16};
default_textures["textures/subway/2_cinderblock"] = {64,64};
default_textures["textures/subway/2_cinderblock_bottom"] = {64,64};
default_textures["textures/subway/2_cinderblock_bottom2"] = {64,64};
default_textures["textures/subway/2_cinderblock_dirty1"] = {64,64};
default_textures["textures/subway/2_cinderblock_dirty1b"] = {64,64};
default_textures["textures/subway/2_cinderblock_dirty1c"] = {64,64};
default_textures["textures/subway/2_cinderblock_dirty1d"] = {64,64};
default_textures["textures/subway/2_cinderblock_graf1"] = {128,64};
default_textures["textures/subway/2_cinderblock_graf10"] = {64,64};
default_textures["textures/subway/2_cinderblock_graf11"] = {64,64};
default_textures["textures/subway/2_cinderblock_graf12"] = {128,64};
default_textures["textures/subway/2_cinderblock_graf13"] = {64,64};
default_textures["textures/subway/2_cinderblock_graf16"] = {64,64};
default_textures["textures/subway/2_cinderblock_graf16b"] = {64,64};
default_textures["textures/subway/2_cinderblock_graf19b"] = {64,64};
default_textures["textures/subway/2_cinderblock_graf20"] = {128,64};
default_textures["textures/subway/2_cinderblock_graf20b"] = {128,64};
default_textures["textures/subway/2_cinderblock_graf4"] = {128,64};
default_textures["textures/subway/2_cinderblock_graf6"] = {128,64};
default_textures["textures/subway/2_cinderblock_graf6b"] = {128,64};
default_textures["textures/subway/2_conwall1"] = {64,64};
default_textures["textures/subway/2_conwall1bot"] = {64,32};
default_textures["textures/subway/2_conwall2"] = {64,64};
default_textures["textures/subway/2_conwall2bot"] = {64,32};
default_textures["textures/subway/2_dirt_floor"] = {64,64};
default_textures["textures/subway/2_dirt_floor2"] = {64,64};
default_textures["textures/subway/2_door_2"] = {64,128};
default_textures["textures/subway/2_fan_grate"] = {128,128};
default_textures["textures/subway/2_fuse1"] = {32,64};
default_textures["textures/subway/2_fuse2"] = {32,64};
default_textures["textures/subway/2_fuse3d"] = {32,64};
default_textures["textures/subway/2_fuseside"] = {16,16};
default_textures["textures/subway/2_guage"] = {16,16};
default_textures["textures/subway/2_holes"] = {32,32};
default_textures["textures/subway/2_oldceiling1a"] = {64,64};
default_textures["textures/subway/2_oldceiling1ab"] = {64,64};
default_textures["textures/subway/2_oldceiling1ab2"] = {64,64};
default_textures["textures/subway/2_oldceiling1b"] = {64,64};
default_textures["textures/subway/2_oldceiling2"] = {64,32};
default_textures["textures/subway/2_oldwall1"] = {64,64};
default_textures["textures/subway/2_oldwall2"] = {64,64};
default_textures["textures/subway/2_oldwall2c"] = {64,64};
default_textures["textures/subway/2_oldwall3"] = {64,64};
default_textures["textures/subway/2_oldwallwet"] = {64,64};
default_textures["textures/subway/2_open_box"] = {64,64};
default_textures["textures/subway/2_open_box2"] = {64,64};
default_textures["textures/subway/2_open_box2d"] = {64,64};
default_textures["textures/subway/2_open_box3"] = {64,64};
default_textures["textures/subway/2_open_boxd"] = {64,64};
default_textures["textures/subway/2_pipe_int"] = {64,64};
default_textures["textures/subway/2_scumconcrete"] = {64,64};
default_textures["textures/subway/2_sign1"] = {64,64};
default_textures["textures/subway/2_sign3"] = {64,64};
default_textures["textures/subway/2_sign4"] = {64,64};
default_textures["textures/subway/2_sign5"] = {64,32};
default_textures["textures/subway/2_stair_side2"] = {16,16};
default_textures["textures/subway/2_steelceilb"] = {64,64};
default_textures["textures/subway/2_steelwalla"] = {64,128};
default_textures["textures/subway/2_steelwallb"] = {64,128};
default_textures["textures/subway/2_steelwallc5"] = {64,64};
default_textures["textures/subway/2_steelwallh"] = {64,128};
default_textures["textures/subway/2_steelwallred"] = {128,128};
default_textures["textures/subway/2_steelwallredb"] = {128,128};
default_textures["textures/subway/2_subwood"] = {64,64};
default_textures["textures/subway/2_subwood1"] = {64,64};
default_textures["textures/subway/2_warning"] = {32,32};
default_textures["textures/subway/2_water1"] = {64,64};
default_textures["textures/subway/2_watersteam"] = {128,128};
default_textures["textures/subway/2_wirewindow"] = {64,64};
default_textures["textures/subway/3_apt"] = {64,64};
default_textures["textures/subway/3_apt_bottom1"] = {64,64};
default_textures["textures/subway/3_apt_window"] = {64,64};
default_textures["textures/subway/3_apt_windowd"] = {64,64};
default_textures["textures/subway/3_building1a"] = {128,32};
default_textures["textures/subway/3_catwalk2"] = {64,64};
default_textures["textures/subway/3_cement"] = {64,64};
default_textures["textures/subway/3_cement_edge"] = {64,64};
default_textures["textures/subway/3_cement_line4"] = {64,64};
default_textures["textures/subway/3_cobble"] = {64,64};
default_textures["textures/subway/3_cobble_edge4"] = {64,64};
default_textures["textures/subway/3_dirt"] = {128,128};
default_textures["textures/subway/3_door3"] = {64,128};
default_textures["textures/subway/3_door4"] = {64,128};
default_textures["textures/subway/3_door5"] = {64,128};
default_textures["textures/subway/3_door6"] = {64,128};
default_textures["textures/subway/3_door6_burn"] = {64,128};
default_textures["textures/subway/3_door_5_b"] = {64,128};
default_textures["textures/subway/3_door_5_burn"] = {64,128};
default_textures["textures/subway/3_door_blocked2"] = {64,128};
default_textures["textures/subway/3_elevator_but"] = {32,64};
default_textures["textures/subway/3_elevator_butd"] = {32,64};
default_textures["textures/subway/3_elevator_door"] = {64,128};
default_textures["textures/subway/3_elevator_door_2b"] = {64,128};
default_textures["textures/subway/3_elevator_sign"] = {64,32};
default_textures["textures/subway/3_flatwood"] = {64,64};
default_textures["textures/subway/3_grocery1"] = {64,64};
default_textures["textures/subway/3_grocery1_window"] = {64,64};
default_textures["textures/subway/3_grocery1_window_wide"] = {128,64};
default_textures["textures/subway/3_grocery1_window_wide_l"] = {128,64};
default_textures["textures/subway/3_grocery1_window_wide_ld"] = {128,64};
default_textures["textures/subway/3_grocery2_bottom"] = {64,64};
default_textures["textures/subway/3_grocery2_bottom_g2"] = {128,64};
default_textures["textures/subway/3_grocery2_dark"] = {64,64};
default_textures["textures/subway/3_grocery2_dark_btm"] = {64,64};
default_textures["textures/subway/3_market1"] = {64,64};
default_textures["textures/subway/3_market1_bottom"] = {64,64};
default_textures["textures/subway/3_market1_top"] = {64,64};
default_textures["textures/subway/3_market2"] = {64,64};
default_textures["textures/subway/3_market2_bottom"] = {64,64};
default_textures["textures/subway/3_market2_top"] = {64,64};
default_textures["textures/subway/3_market3"] = {64,64};
default_textures["textures/subway/3_market3_bottom"] = {64,64};
default_textures["textures/subway/3_market3_top"] = {64,64};
default_textures["textures/subway/3_market_window1"] = {64,64};
default_textures["textures/subway/3_market_window1d"] = {64,64};
default_textures["textures/subway/3_market_window2"] = {64,64};
default_textures["textures/subway/3_medcrate1"] = {64,64};
default_textures["textures/subway/3_medcrate2"] = {64,64};
default_textures["textures/subway/3_metalside"] = {64,64};
default_textures["textures/subway/3_mrust1"] = {64,64};
default_textures["textures/subway/3_mrust2"] = {64,64};
default_textures["textures/subway/3_mrust3"] = {64,64};
default_textures["textures/subway/3_mtrim2"] = {64,32};
default_textures["textures/subway/3_orange2"] = {64,64};
default_textures["textures/subway/3_orange_bottom1"] = {64,64};
default_textures["textures/subway/3_panel2"] = {64,64};
default_textures["textures/subway/3_panel2d"] = {64,64};
default_textures["textures/subway/3_panel7"] = {128,64};
default_textures["textures/subway/3_panel7d"] = {128,64};
default_textures["textures/subway/3_pipes2"] = {64,32};
default_textures["textures/subway/3_redbrick"] = {64,64};
default_textures["textures/subway/3_redbrick_bottom1"] = {64,64};
default_textures["textures/subway/3_redbrick_bottom1b"] = {64,64};
default_textures["textures/subway/3_redbrick_trim"] = {16,16};
default_textures["textures/subway/3_redbrick_trim_a"] = {64,64};
default_textures["textures/subway/3_redbrick_window_a"] = {64,64};
default_textures["textures/subway/3_redbrick_window_ad1"] = {64,64};
default_textures["textures/subway/3_redbrick_window_ad2"] = {64,64};
default_textures["textures/subway/3_redbrick_window_b"] = {64,64};
default_textures["textures/subway/3_redpaint"] = {16,16};
default_textures["textures/subway/3_shingle"] = {64,64};
default_textures["textures/subway/3_slats"] = {64,64};
default_textures["textures/subway/3_storewall_burn"] = {128,128};
default_textures["textures/subway/3_storewall_burn_btm"] = {128,128};
default_textures["textures/subway/3_store_brick1"] = {64,64};
default_textures["textures/subway/3_store_brick1_bottom"] = {64,64};
default_textures["textures/subway/3_store_brick3"] = {64,64};
default_textures["textures/subway/3_store_brick3_bottom"] = {64,64};
default_textures["textures/subway/3_store_brick_window"] = {64,64};
default_textures["textures/subway/3_store_brick_window2"] = {64,64};
default_textures["textures/subway/3_store_brick_window2d"] = {64,64};
default_textures["textures/subway/3_store_brick_windowd"] = {64,64};
default_textures["textures/subway/3_store_end"] = {32,64};
default_textures["textures/subway/3_store_flr_burn"] = {128,128};
default_textures["textures/subway/3_store_flr_burn2"] = {128,128};
default_textures["textures/subway/3_store_flr_burn3"] = {64,128};
default_textures["textures/subway/3_store_trans"] = {64,64};
default_textures["textures/subway/3_store_trans_bottom"] = {64,64};
default_textures["textures/subway/3_store_white"] = {64,64};
default_textures["textures/subway/3_store_white2"] = {64,64};
default_textures["textures/subway/3_store_white_bottom"] = {64,64};
default_textures["textures/subway/3_store_white_bottom2"] = {64,64};
default_textures["textures/subway/3_store_white_sign1"] = {64,64};
default_textures["textures/subway/3_store_white_sign2"] = {64,64};
default_textures["textures/subway/3_store_white_sign3"] = {64,64};
default_textures["textures/subway/3_store_white_sign4"] = {64,64};
default_textures["textures/subway/3_store_white_sign5"] = {64,64};
default_textures["textures/subway/3_store_white_sign6"] = {64,64};
default_textures["textures/subway/3_store_white_top"] = {64,32};
default_textures["textures/subway/3_street"] = {64,64};
default_textures["textures/subway/3_street_edge1"] = {64,64};
default_textures["textures/subway/3_street_middle"] = {64,64};
default_textures["textures/subway/3_street_trans"] = {64,64};
default_textures["textures/subway/3_sweatfloor"] = {64,64};
default_textures["textures/subway/3_sweatwall"] = {64,64};
default_textures["textures/subway/3_sweatwall2"] = {64,64};
default_textures["textures/subway/3_sweatwall3"] = {64,64};
default_textures["textures/subway/3_sweatwall4"] = {64,64};
default_textures["textures/subway/3_sweatwall4d"] = {64,64};
default_textures["textures/subway/3_sweatwall_bottom"] = {64,64};
default_textures["textures/subway/3_sweatwall_top"] = {64,64};
default_textures["textures/subway/3_sweatwall_top2"] = {64,64};
default_textures["textures/subway/3_sweatwall_top3"] = {64,128};
default_textures["textures/subway/3_sweat_bars"] = {32,32};
default_textures["textures/subway/3_sweat_ceil1"] = {64,64};
default_textures["textures/subway/3_sweat_ceil2"] = {64,64};
default_textures["textures/subway/3_sweat_ceil_edge1"] = {64,64};
default_textures["textures/subway/3_sweat_ceil_edge2"] = {64,64};
default_textures["textures/subway/3_sweat_ceil_edge2a"] = {64,64};
default_textures["textures/subway/3_sweat_flr"] = {64,64};
default_textures["textures/subway/3_sweat_flr_trim"] = {32,64};
default_textures["textures/subway/3_sweat_trim"] = {8,8};
default_textures["textures/subway/3_sweat_trim1"] = {32,16};
default_textures["textures/subway/3_tarpaper"] = {64,64};
default_textures["textures/subway/3_tarpaper_vert"] = {64,64};
default_textures["textures/subway/3_wall2"] = {128,128};
default_textures["textures/subway/3_wall2_bottom"] = {128,128};
default_textures["textures/subway/3_wareceiling1"] = {128,128};
default_textures["textures/subway/3_woodfence"] = {64,128};
default_textures["textures/subway/3_woodfence_alpha"] = {64,128};
default_textures["textures/subway/3_woodfloor"] = {64,64};
default_textures["textures/subway/3_woodfloor2"] = {128,128};
default_textures["textures/subway/ammobox_end"] = {32,32};
default_textures["textures/subway/ammobox_side"] = {64,32};
default_textures["textures/subway/ammobox_top"] = {64,32};
default_textures["textures/subway/ammobox_topd"] = {64,32};
default_textures["textures/subway/barbwire"] = {128,64};
default_textures["textures/subway/barrel"] = {64,64};
default_textures["textures/subway/barrel_big_end"] = {64,64};
default_textures["textures/subway/barrel_big_endb"] = {64,64};
default_textures["textures/subway/barrel_top"] = {32,32};
default_textures["textures/subway/bars1"] = {64,64};
default_textures["textures/subway/bars2"] = {64,64};
default_textures["textures/subway/big_pipe2"] = {64,64};
default_textures["textures/subway/button2a"] = {32,32};
default_textures["textures/subway/button2b"] = {32,32};
default_textures["textures/subway/button2d"] = {32,32};
default_textures["textures/subway/camera1"] = {64,64};
default_textures["textures/subway/camera2"] = {64,64};
default_textures["textures/subway/camera3"] = {64,64};
default_textures["textures/subway/cargo_elev1"] = {64,64};
default_textures["textures/subway/cargo_elev2"] = {64,64};
default_textures["textures/subway/cargo_elev3"] = {64,64};
default_textures["textures/subway/cargo_elev4"] = {64,64};
default_textures["textures/subway/cargo_elev_but"] = {16,32};
default_textures["textures/subway/cargo_elev_butd"] = {16,32};
default_textures["textures/subway/catwalk"] = {64,64};
default_textures["textures/subway/chain1"] = {16,64};
default_textures["textures/subway/chainlink"] = {32,32};
default_textures["textures/subway/clip"] = {64,64};
default_textures["textures/subway/con1"] = {64,64};
default_textures["textures/subway/con10"] = {64,64};
default_textures["textures/subway/con11"] = {64,64};
default_textures["textures/subway/con11_seam"] = {64,64};
default_textures["textures/subway/con12"] = {32,64};
default_textures["textures/subway/con14"] = {64,64};
default_textures["textures/subway/con15"] = {64,64};
default_textures["textures/subway/con1a"] = {16,64};
default_textures["textures/subway/con1b"] = {32,64};
default_textures["textures/subway/con2"] = {64,64};
default_textures["textures/subway/con2_corner"] = {32,64};
default_textures["textures/subway/con2_pipe1"] = {64,64};
default_textures["textures/subway/con2_pipe2"] = {64,64};
default_textures["textures/subway/con3"] = {64,64};
default_textures["textures/subway/con4"] = {64,64};
default_textures["textures/subway/con5"] = {64,64};
default_textures["textures/subway/con6"] = {64,64};
default_textures["textures/subway/contrim1"] = {128,64};
default_textures["textures/subway/con_ceil"] = {64,64};
default_textures["textures/subway/con_ceil2"] = {64,32};
default_textures["textures/subway/crate1a"] = {64,64};
default_textures["textures/subway/crate1b"] = {64,64};
default_textures["textures/subway/crate2a"] = {64,64};
default_textures["textures/subway/crate2a_burn"] = {64,64};
default_textures["textures/subway/crate2b"] = {64,64};
default_textures["textures/subway/crate2b_burn"] = {64,64};
default_textures["textures/subway/crate3a"] = {64,64};
default_textures["textures/subway/crate3b"] = {64,64};
default_textures["textures/subway/crate4a"] = {64,64};
default_textures["textures/subway/crate4b"] = {64,64};
default_textures["textures/subway/ctfb_base"] = {64,64};
default_textures["textures/subway/ctfr_base"] = {64,64};
default_textures["textures/subway/dump-lid"] = {64,64};
default_textures["textures/subway/dumpbck"] = {128,64};
default_textures["textures/subway/d_genericsmooth"] = {64,64};
default_textures["textures/subway/d_gravel"] = {64,64};
default_textures["textures/subway/d_metalbump"] = {32,32};
default_textures["textures/subway/d_metalribbed"] = {64,64};
default_textures["textures/subway/d_metalrough"] = {64,64};
default_textures["textures/subway/d_metalsmooth"] = {64,64};
default_textures["textures/subway/d_stonecracked"] = {64,64};
default_textures["textures/subway/d_stoneholey2"] = {64,64};
default_textures["textures/subway/d_stonerough"] = {128,128};
default_textures["textures/subway/d_stonesmooth"] = {64,64};
default_textures["textures/subway/d_stucco"] = {64,64};
default_textures["textures/subway/d_tile"] = {32,32};
default_textures["textures/subway/d_woodrough"] = {64,64};
default_textures["textures/subway/d_woodrough2"] = {64,64};
default_textures["textures/subway/d_woodsmooth"] = {64,64};
default_textures["textures/subway/env1a"] = {256,256};
default_textures["textures/subway/env1b"] = {256,256};
default_textures["textures/subway/env1c"] = {256,256};
default_textures["textures/subway/exit"] = {64,32};
default_textures["textures/subway/exitd"] = {64,32};
default_textures["textures/subway/fanblade"] = {32,16};
default_textures["textures/subway/file_front"] = {32,64};
default_textures["textures/subway/file_frontd2"] = {32,64};
default_textures["textures/subway/file_side"] = {32,64};
default_textures["textures/subway/girdwall"] = {128,128};
default_textures["textures/subway/girdwall2"] = {128,128};
default_textures["textures/subway/glass"] = {32,32};
default_textures["textures/subway/glass_wire"] = {64,64};
default_textures["textures/subway/glass_wire_d"] = {64,64};
default_textures["textures/subway/grating1"] = {64,64};
default_textures["textures/subway/grating2"] = {64,64};
default_textures["textures/subway/grating4"] = {32,32};
default_textures["textures/subway/greenbox_front1"] = {64,64};
default_textures["textures/subway/greenbox_front2"] = {64,64};
default_textures["textures/subway/greenbox_front3"] = {64,64};
default_textures["textures/subway/greenbox_front3d"] = {64,64};
default_textures["textures/subway/greenbox_side"] = {16,16};
default_textures["textures/subway/grenade"] = {16,16};
default_textures["textures/subway/hint"] = {64,64};
default_textures["textures/subway/h_cretewall_dirty"] = {64,64};
default_textures["textures/subway/h_cretewall_plain"] = {64,64};
default_textures["textures/subway/h_heli_land1"] = {64,64};
default_textures["textures/subway/h_heli_land_b"] = {64,64};
default_textures["textures/subway/h_heli_land_c"] = {64,64};
default_textures["textures/subway/h_heli_land_d"] = {64,64};
default_textures["textures/subway/h_light5"] = {32,32};
default_textures["textures/subway/h_yardfloor"] = {128,128};
default_textures["textures/subway/ladder_rung"] = {32,16};
default_textures["textures/subway/light_blue"] = {32,32};
default_textures["textures/subway/light_bluey"] = {32,32};
default_textures["textures/subway/light_emergency"] = {16,16};
default_textures["textures/subway/light_florescent"] = {16,16};
default_textures["textures/subway/light_orange"] = {32,32};
default_textures["textures/subway/light_red"] = {32,32};
default_textures["textures/subway/light_small"] = {32,16};
default_textures["textures/subway/light_square"] = {32,32};
default_textures["textures/subway/light_tiny_yellow"] = {16,16};
default_textures["textures/subway/light_tube"] = {128,32};
default_textures["textures/subway/light_tube1"] = {64,32};
default_textures["textures/subway/light_tube2"] = {64,16};
default_textures["textures/subway/light_tubey"] = {16,64};
default_textures["textures/subway/light_tube_d"] = {64,32};
default_textures["textures/subway/light_tungsten"] = {16,16};
default_textures["textures/subway/light_yellow"] = {32,32};
default_textures["textures/subway/med_door"] = {32,32};
default_textures["textures/subway/med_inside"] = {32,32};
default_textures["textures/subway/med_inside_used"] = {32,32};
default_textures["textures/subway/med_red"] = {16,16};
default_textures["textures/subway/med_sides"] = {16,16};
default_textures["textures/subway/med_sign"] = {64,64};
default_textures["textures/subway/med_stripe"] = {16,16};
default_textures["textures/subway/metal1"] = {64,64};
default_textures["textures/subway/metal2"] = {64,64};
default_textures["textures/subway/metalfloor1"] = {64,64};
default_textures["textures/subway/metalfloor1b"] = {64,64};
default_textures["textures/subway/metalrib1"] = {32,32};
default_textures["textures/subway/metalrib1b"] = {32,32};
default_textures["textures/subway/metalrib2"] = {64,64};
default_textures["textures/subway/metal_clean"] = {64,64};
default_textures["textures/subway/metal_panel"] = {64,64};
default_textures["textures/subway/metal_rust"] = {64,64};
default_textures["textures/subway/meter1"] = {64,64};
default_textures["textures/subway/meter1d"] = {64,64};
default_textures["textures/subway/mirror"] = {64,64};
default_textures["textures/subway/mirrord"] = {64,64};
default_textures["textures/subway/newpipe1"] = {64,32};
default_textures["textures/subway/newpipe1hilight"] = {64,32};
default_textures["textures/subway/no_draw"] = {64,64};
default_textures["textures/subway/origin"] = {64,64};
default_textures["textures/subway/pipe3b"] = {128,128};
default_textures["textures/subway/pipe_cap"] = {64,64};
default_textures["textures/subway/pipe_grey1"] = {64,64};
default_textures["textures/subway/pipe_grey2"] = {64,64};
default_textures["textures/subway/pipe_rusty2"] = {64,32};
default_textures["textures/subway/sidewalk"] = {64,64};
default_textures["textures/subway/sign11"] = {128,64};
default_textures["textures/subway/sign12"] = {128,64};
default_textures["textures/subway/sign13"] = {128,128};
default_textures["textures/subway/sign13b"] = {128,128};
default_textures["textures/subway/sign14"] = {64,64};
default_textures["textures/subway/sign16"] = {128,64};
default_textures["textures/subway/sign17"] = {64,128};
default_textures["textures/subway/sign17_d"] = {64,128};
default_textures["textures/subway/sign18"] = {64,128};
default_textures["textures/subway/sign18_d"] = {64,128};
default_textures["textures/subway/sign2"] = {32,32};
default_textures["textures/subway/sign3"] = {64,64};
default_textures["textures/subway/sign4"] = {64,64};
default_textures["textures/subway/sign6"] = {128,64};
default_textures["textures/subway/sign7"] = {128,64};
default_textures["textures/subway/sign8"] = {128,64};
default_textures["textures/subway/sign9"] = {64,64};
default_textures["textures/subway/skip"] = {64,64};
default_textures["textures/subway/sky"] = {64,64};
default_textures["textures/subway/sky2"] = {64,64};
default_textures["textures/subway/stair_side"] = {16,16};
default_textures["textures/subway/s_asphalt"] = {64,64};
default_textures["textures/subway/s_asphalt_hole"] = {64,64};
default_textures["textures/subway/s_curb"] = {16,128};
default_textures["textures/subway/s_sidewalk"] = {128,128};
default_textures["textures/subway/tire1"] = {64,32};
default_textures["textures/subway/tire2"] = {64,32};
default_textures["textures/subway/trigger"] = {64,64};
default_textures["textures/subway/trim2a"] = {32,32};
default_textures["textures/subway/window1_dirt"] = {64,64};
default_textures["textures/subway/window1_dirt_d1"] = {64,64};
default_textures["textures/subway/window1_dirt_d2"] = {64,64};
default_textures["textures/subway/window2"] = {64,64};
default_textures["textures/subway/window2_d1"] = {64,64};
default_textures["textures/subway/window2_d2"] = {64,64};
default_textures["textures/subway/window2_dirt"] = {64,64};
default_textures["textures/subway/window2_dirt_d"] = {64,64};
default_textures["textures/subway/window2_dirt_d2"] = {64,64};
default_textures["textures/subway/wood1"] = {64,64};
default_textures["textures/subway/w_crane_beam1"] = {16,16};
default_textures["textures/subway/w_crane_beam1a"] = {128,128};
default_textures["textures/subway/w_crane_body1"] = {64,64};
default_textures["textures/subway/w_crane_body1a"] = {64,64};
default_textures["textures/subway/w_crane_body_vent"] = {32,32};
default_textures["textures/subway/w_crane_weight"] = {64,64};
default_textures["textures/subway/w_crane_weight2"] = {64,64};
default_textures["textures/subway/w_crate1"] = {64,32};
default_textures["textures/subway/w_crate2"] = {32,32};
default_textures["textures/subway/w_crete1"] = {64,64};
default_textures["textures/subway/w_fencewheel"] = {32,32};
default_textures["textures/subway/w_gardoor1"] = {128,128};
default_textures["textures/subway/w_gardoor3"] = {128,128};
default_textures["textures/subway/w_gardoor3_g1"] = {128,128};
default_textures["textures/subway/w_palette1"] = {64,64};
default_textures["textures/subway/w_palette2"] = {64,64};
default_textures["textures/subway/w_pallette3"] = {64,64};
default_textures["textures/subway/w_rustwall1"] = {64,64};
default_textures["textures/subway/w_rustwall1a"] = {64,64};
default_textures["textures/subway/w_rustwall3a"] = {128,128};
default_textures["textures/subway/w_rustwall4"] = {64,64};
default_textures["textures/subway/w_rustwall4b"] = {64,64};
default_textures["textures/subway/w_rustwall5"] = {64,64};
default_textures["textures/subway/w_rustwall6"] = {64,64};
default_textures["textures/subway/w_semiback"] = {64,128};
default_textures["textures/subway/w_semiback2"] = {64,128};
default_textures["textures/subway/w_semiback3"] = {64,128};
default_textures["textures/subway/w_semiside"] = {64,128};
default_textures["textures/subway/w_semiside2"] = {64,128};
default_textures["textures/subway/w_semiside3"] = {64,128};
default_textures["textures/subway/w_shtmetal1"] = {64,64};
default_textures["textures/subway/w_win1_noalpha"] = {64,64};
default_textures["textures/subway/w_win1_noalphad1"] = {64,64};
default_textures["textures/subway/w_win2_noalpha"] = {64,64};
default_textures["textures/subway/w_win3_noalpha"] = {64,64};
default_textures["textures/subway/w_win3_noalphad"] = {64,64};
default_textures["textures/tokyo/1_2_brownfloor"] = {32,32};
default_textures["textures/tokyo/1_2_darkfloor"] = {32,32};
default_textures["textures/tokyo/1_2_directory"] = {128,64};
default_textures["textures/tokyo/1_2_directoryd"] = {128,64};
default_textures["textures/tokyo/1_2_door1"] = {64,128};
default_textures["textures/tokyo/1_2_door2"] = {64,128};
default_textures["textures/tokyo/1_2_door3"] = {64,128};
default_textures["textures/tokyo/1_2_door3b"] = {64,128};
default_textures["textures/tokyo/1_2_door4a"] = {64,128};
default_textures["textures/tokyo/1_2_door4b"] = {64,128};
default_textures["textures/tokyo/1_2_door4c"] = {32,64};
default_textures["textures/tokyo/1_2_door5"] = {64,128};
default_textures["textures/tokyo/1_2_doortrim"] = {16,16};
default_textures["textures/tokyo/1_2_suni"] = {128,64};
default_textures["textures/tokyo/1_3_meter1"] = {64,64};
default_textures["textures/tokyo/1_3_meter1_d"] = {64,64};
default_textures["textures/tokyo/1_3_paperwall"] = {32,32};
default_textures["textures/tokyo/1_3_rail"] = {64,32};
default_textures["textures/tokyo/1_3_rug1"] = {64,128};
default_textures["textures/tokyo/1_3_support1"] = {64,128};
default_textures["textures/tokyo/1_3_support2"] = {64,32};
default_textures["textures/tokyo/1_3_woodtrim"] = {32,16};
default_textures["textures/tokyo/1_bamboo"] = {32,64};
default_textures["textures/tokyo/1_bamboo2"] = {32,32};
default_textures["textures/tokyo/1_bamboo_flat"] = {32,32};
default_textures["textures/tokyo/1_basement1a"] = {32,32};
default_textures["textures/tokyo/1_basement1b"] = {64,64};
default_textures["textures/tokyo/1_basement2b"] = {64,64};
default_textures["textures/tokyo/1_basement2c"] = {64,64};
default_textures["textures/tokyo/1_basement2d"] = {64,64};
default_textures["textures/tokyo/1_basement3a"] = {64,64};
default_textures["textures/tokyo/1_basement4"] = {64,64};
default_textures["textures/tokyo/1_basementpln"] = {64,64};
default_textures["textures/tokyo/1_blinds_a"] = {64,128};
default_textures["textures/tokyo/1_block1"] = {64,64};
default_textures["textures/tokyo/1_block1_bot"] = {64,64};
default_textures["textures/tokyo/1_block2"] = {64,64};
default_textures["textures/tokyo/1_block2_bot"] = {64,64};
default_textures["textures/tokyo/1_block4"] = {64,64};
default_textures["textures/tokyo/1_block4_bot"] = {64,64};
default_textures["textures/tokyo/1_build1"] = {32,32};
default_textures["textures/tokyo/1_build1_win1"] = {64,32};
default_textures["textures/tokyo/1_build1_win1a"] = {64,32};
default_textures["textures/tokyo/1_build1_win1ad"] = {64,32};
default_textures["textures/tokyo/1_build1_win1d"] = {64,32};
default_textures["textures/tokyo/1_build1_win2"] = {64,32};
default_textures["textures/tokyo/1_build2"] = {32,32};
default_textures["textures/tokyo/1_build2_win1"] = {32,32};
default_textures["textures/tokyo/1_build2_win1a"] = {32,32};
default_textures["textures/tokyo/1_build2_win1ad"] = {32,32};
default_textures["textures/tokyo/1_build2_win1d"] = {32,32};
default_textures["textures/tokyo/1_build2_win2"] = {32,32};
default_textures["textures/tokyo/1_build3"] = {64,64};
default_textures["textures/tokyo/1_build3_bot"] = {64,16};
default_textures["textures/tokyo/1_build3_btm"] = {64,16};
default_textures["textures/tokyo/1_build3_win1ad"] = {64,64};
default_textures["textures/tokyo/1_build3_win2"] = {64,64};
default_textures["textures/tokyo/1_build5"] = {64,64};
default_textures["textures/tokyo/1_build5_base"] = {64,64};
default_textures["textures/tokyo/1_build5_base2"] = {64,32};
default_textures["textures/tokyo/1_build6"] = {64,64};
default_textures["textures/tokyo/1_build6ad"] = {64,64};
default_textures["textures/tokyo/1_build7"] = {64,64};
default_textures["textures/tokyo/1_build7a"] = {64,64};
default_textures["textures/tokyo/1_build7ad"] = {64,64};
default_textures["textures/tokyo/1_build8"] = {64,64};
default_textures["textures/tokyo/1_build8_trim"] = {16,64};
default_textures["textures/tokyo/1_build9c"] = {64,64};
default_textures["textures/tokyo/1_build9dd"] = {64,64};
default_textures["textures/tokyo/1_build_crete"] = {32,32};
default_textures["textures/tokyo/1_button_on"] = {32,32};
default_textures["textures/tokyo/1_cabinet1"] = {128,128};
default_textures["textures/tokyo/1_cabinet2"] = {128,128};
default_textures["textures/tokyo/1_cabinet3"] = {128,64};
default_textures["textures/tokyo/1_cam1"] = {64,64};
default_textures["textures/tokyo/1_cam1b"] = {64,64};
default_textures["textures/tokyo/1_cam2"] = {64,64};
default_textures["textures/tokyo/1_cam2b"] = {64,64};
default_textures["textures/tokyo/1_cam3"] = {64,64};
default_textures["textures/tokyo/1_cam4"] = {64,64};
default_textures["textures/tokyo/1_camd"] = {64,64};
default_textures["textures/tokyo/1_catwalk_floor"] = {32,32};
default_textures["textures/tokyo/1_chainlink"] = {16,16};
default_textures["textures/tokyo/1_confloor"] = {64,64};
default_textures["textures/tokyo/1_confloor2"] = {64,64};
default_textures["textures/tokyo/1_confloor3"] = {128,128};
default_textures["textures/tokyo/1_confloor6"] = {64,64};
default_textures["textures/tokyo/1_confloor7"] = {64,64};
default_textures["textures/tokyo/1_confloor8"] = {128,128};
default_textures["textures/tokyo/1_confloor_dirt"] = {32,16};
default_textures["textures/tokyo/1_confloor_trim"] = {32,32};
default_textures["textures/tokyo/1_conrib"] = {64,64};
default_textures["textures/tokyo/1_conseam1"] = {64,64};
default_textures["textures/tokyo/1_contan"] = {64,64};
default_textures["textures/tokyo/1_contrim1"] = {128,64};
default_textures["textures/tokyo/1_con_rough"] = {64,64};
default_textures["textures/tokyo/1_crate1"] = {32,32};
default_textures["textures/tokyo/1_crate2"] = {32,32};
default_textures["textures/tokyo/1_crate3"] = {32,32};
default_textures["textures/tokyo/1_desklight"] = {64,32};
default_textures["textures/tokyo/1_desktop"] = {32,32};
default_textures["textures/tokyo/1_door4"] = {64,128};
default_textures["textures/tokyo/1_door5"] = {64,128};
default_textures["textures/tokyo/1_doorwindow"] = {32,128};
default_textures["textures/tokyo/1_door_1"] = {64,128};
default_textures["textures/tokyo/1_door_cor"] = {64,64};
default_textures["textures/tokyo/1_door_freeze"] = {64,128};
default_textures["textures/tokyo/1_drywall"] = {64,64};
default_textures["textures/tokyo/1_drywall_bot"] = {64,32};
default_textures["textures/tokyo/1_drywall_top"] = {64,32};
default_textures["textures/tokyo/1_dumpbck"] = {128,64};
default_textures["textures/tokyo/1_dumpfrt"] = {128,32};
default_textures["textures/tokyo/1_dumplid"] = {64,64};
default_textures["textures/tokyo/1_dumplid64"] = {64,64};
default_textures["textures/tokyo/1_elenumber1"] = {32,32};
default_textures["textures/tokyo/1_elenumber2"] = {32,32};
default_textures["textures/tokyo/1_freezerceil"] = {64,64};
default_textures["textures/tokyo/1_freezerfloor"] = {64,64};
default_textures["textures/tokyo/1_freezerwall"] = {64,128};
default_textures["textures/tokyo/1_freeze_cardbox"] = {16,16};
default_textures["textures/tokyo/1_freeze_fishbox"] = {16,16};
default_textures["textures/tokyo/1_fridge_a"] = {128,128};
default_textures["textures/tokyo/1_fridge_b"] = {128,128};
default_textures["textures/tokyo/1_fuse_metal"] = {16,16};
default_textures["textures/tokyo/1_garage"] = {32,32};
default_textures["textures/tokyo/1_garage_bot"] = {32,16};
default_textures["textures/tokyo/1_gardoor1"] = {128,128};
default_textures["textures/tokyo/1_girder"] = {64,32};
default_textures["textures/tokyo/1_grate"] = {64,64};
default_textures["textures/tokyo/1_grating"] = {64,64};
default_textures["textures/tokyo/1_grnbox_frt1"] = {64,64};
default_textures["textures/tokyo/1_grnbox_frt2"] = {64,64};
default_textures["textures/tokyo/1_grnbox_frt3"] = {64,64};
default_textures["textures/tokyo/1_grnbox_frt3d"] = {64,64};
default_textures["textures/tokyo/1_grnbox_side"] = {16,16};
default_textures["textures/tokyo/1_kitchblock"] = {64,64};
default_textures["textures/tokyo/1_kitchblock2"] = {64,64};
default_textures["textures/tokyo/1_kitchblock2a"] = {64,64};
default_textures["textures/tokyo/1_kitchblock2b"] = {64,64};
default_textures["textures/tokyo/1_kitchblock2c"] = {64,64};
default_textures["textures/tokyo/1_kitchblock2_top"] = {64,32};
default_textures["textures/tokyo/1_kitchblock3"] = {64,64};
default_textures["textures/tokyo/1_kitchblock_blue"] = {64,64};
default_textures["textures/tokyo/1_kitchblock_flr"] = {32,32};
default_textures["textures/tokyo/1_kitchblock_red"] = {64,64};
default_textures["textures/tokyo/1_kitchcab1"] = {128,128};
default_textures["textures/tokyo/1_kitchcab2"] = {128,128};
default_textures["textures/tokyo/1_kitchcounter"] = {64,64};
default_textures["textures/tokyo/1_kitchfloor1"] = {32,32};
default_textures["textures/tokyo/1_kitchfloor1trim"] = {16,16};
default_textures["textures/tokyo/1_kitchtop"] = {32,32};
default_textures["textures/tokyo/1_kitch_stove1"] = {64,64};
default_textures["textures/tokyo/1_kitch_stove2"] = {32,32};
default_textures["textures/tokyo/1_kitch_stove3"] = {64,64};
default_textures["textures/tokyo/1_kitch_stove4"] = {32,32};
default_textures["textures/tokyo/1_meat"] = {64,32};
default_textures["textures/tokyo/1_metalpanel"] = {64,64};
default_textures["textures/tokyo/1_metalside1"] = {64,64};
default_textures["textures/tokyo/1_metal_power"] = {64,64};
default_textures["textures/tokyo/1_metal_side"] = {64,64};
default_textures["textures/tokyo/1_metal_side2"] = {64,64};
default_textures["textures/tokyo/1_mside_thick"] = {32,64};
default_textures["textures/tokyo/1_news_front1"] = {64,64};
default_textures["textures/tokyo/1_news_front1d"] = {64,64};
default_textures["textures/tokyo/1_news_side1"] = {32,64};
default_textures["textures/tokyo/1_news_top1"] = {32,64};
default_textures["textures/tokyo/1_pdoor1"] = {64,128};
default_textures["textures/tokyo/1_pdoor2"] = {64,128};
default_textures["textures/tokyo/1_pipe_rust"] = {64,64};
default_textures["textures/tokyo/1_ramp_ceil1"] = {64,64};
default_textures["textures/tokyo/1_ramp_ceil2"] = {64,64};
default_textures["textures/tokyo/1_ramp_ceil3"] = {64,64};
default_textures["textures/tokyo/1_ramp_ceil4"] = {64,64};
default_textures["textures/tokyo/1_ramp_floor"] = {64,64};
default_textures["textures/tokyo/1_ramp_floor2"] = {64,64};
default_textures["textures/tokyo/1_ramp_floor3"] = {64,64};
default_textures["textures/tokyo/1_ramp_line"] = {16,16};
default_textures["textures/tokyo/1_ramp_rail1"] = {64,32};
default_textures["textures/tokyo/1_ramp_rail2"] = {64,32};
default_textures["textures/tokyo/1_ramp_rail3"] = {64,32};
default_textures["textures/tokyo/1_ramp_rail4"] = {64,32};
default_textures["textures/tokyo/1_ramp_stall"] = {64,64};
default_textures["textures/tokyo/1_ramp_stall2"] = {64,64};
default_textures["textures/tokyo/1_ramp_wall1"] = {64,64};
default_textures["textures/tokyo/1_ramp_wall1a"] = {64,32};
default_textures["textures/tokyo/1_ramp_wall1_blue"] = {64,64};
default_textures["textures/tokyo/1_ramp_wall1_red"] = {64,64};
default_textures["textures/tokyo/1_ramp_wall1_top"] = {64,32};
default_textures["textures/tokyo/1_ramp_wall1_top2"] = {64,32};
default_textures["textures/tokyo/1_ramp_wall2"] = {64,64};
default_textures["textures/tokyo/1_ramp_wall2a"] = {64,32};
default_textures["textures/tokyo/1_ramp_wall2_blue"] = {64,64};
default_textures["textures/tokyo/1_ramp_wall2_red"] = {64,64};
default_textures["textures/tokyo/1_ramp_wall2_top"] = {64,32};
default_textures["textures/tokyo/1_ramp_wall_bot"] = {64,32};
default_textures["textures/tokyo/1_ramp_wall_mid"] = {64,64};
default_textures["textures/tokyo/1_ramp_wall_mid_d"] = {64,64};
default_textures["textures/tokyo/1_ramp_wall_top"] = {64,64};
default_textures["textures/tokyo/1_ravensign"] = {64,128};
default_textures["textures/tokyo/1_rickboard"] = {128,128};
default_textures["textures/tokyo/1_roada"] = {64,64};
default_textures["textures/tokyo/1_shelf"] = {16,16};
default_textures["textures/tokyo/1_shelf1"] = {32,32};
default_textures["textures/tokyo/1_shelf1b"] = {16,16};
default_textures["textures/tokyo/1_shelf2"] = {64,32};
default_textures["textures/tokyo/1_shelf3"] = {64,32};
default_textures["textures/tokyo/1_shelf_liquor"] = {64,64};
default_textures["textures/tokyo/1_sidewalk"] = {64,64};
default_textures["textures/tokyo/1_sidewalk2"] = {64,32};
default_textures["textures/tokyo/1_sidewalk3"] = {64,64};
default_textures["textures/tokyo/1_sidewalk4"] = {64,64};
default_textures["textures/tokyo/1_sign5"] = {128,64};
default_textures["textures/tokyo/1_sign6"] = {128,64};
default_textures["textures/tokyo/1_sign7"] = {32,64};
default_textures["textures/tokyo/1_sign7_d"] = {32,64};
default_textures["textures/tokyo/1_sign8"] = {32,64};
default_textures["textures/tokyo/1_sign8_d"] = {32,64};
default_textures["textures/tokyo/1_sign_kiosk"] = {64,32};
default_textures["textures/tokyo/1_skywalk"] = {128,128};
default_textures["textures/tokyo/1_sushi_decor"] = {32,16};
default_textures["textures/tokyo/1_trailerside"] = {32,32};
default_textures["textures/tokyo/1_trailerside2"] = {128,32};
default_textures["textures/tokyo/1_trailer_tire1"] = {64,32};
default_textures["textures/tokyo/1_trailer_tire2"] = {64,32};
default_textures["textures/tokyo/1_wires1"] = {16,32};
default_textures["textures/tokyo/1_woodtrim"] = {32,16};
default_textures["textures/tokyo/2_3_whitewall"] = {32,32};
default_textures["textures/tokyo/2_accesspanel"] = {64,64};
default_textures["textures/tokyo/2_bars_clean"] = {16,64};
default_textures["textures/tokyo/2_basevent"] = {32,16};
default_textures["textures/tokyo/2_chart"] = {128,128};
default_textures["textures/tokyo/2_chart_d"] = {128,128};
default_textures["textures/tokyo/2_checkfloor"] = {64,64};
default_textures["textures/tokyo/2_clean1"] = {32,32};
default_textures["textures/tokyo/2_clean10a"] = {64,32};
default_textures["textures/tokyo/2_clean10a_d"] = {64,32};
default_textures["textures/tokyo/2_clean10b"] = {64,32};
default_textures["textures/tokyo/2_clean10c"] = {64,16};
default_textures["textures/tokyo/2_clean10c_d"] = {64,16};
default_textures["textures/tokyo/2_clean11a"] = {64,64};
default_textures["textures/tokyo/2_clean11a_d"] = {64,64};
default_textures["textures/tokyo/2_clean11b"] = {64,64};
default_textures["textures/tokyo/2_clean12a"] = {32,64};
default_textures["textures/tokyo/2_clean12a_d"] = {32,64};
default_textures["textures/tokyo/2_clean12b"] = {32,64};
default_textures["textures/tokyo/2_clean2"] = {32,32};
default_textures["textures/tokyo/2_clean4"] = {32,32};
default_textures["textures/tokyo/2_clean5"] = {64,64};
default_textures["textures/tokyo/2_clean6"] = {64,64};
default_textures["textures/tokyo/2_clean6b"] = {64,64};
default_textures["textures/tokyo/2_clean6c"] = {64,64};
default_textures["textures/tokyo/2_clean6c_d"] = {64,64};
default_textures["textures/tokyo/2_clean6d"] = {32,64};
default_textures["textures/tokyo/2_clean6d_d"] = {32,64};
default_textures["textures/tokyo/2_clean7"] = {32,64};
default_textures["textures/tokyo/2_clean7d"] = {32,64};
default_textures["textures/tokyo/2_clean8a"] = {64,64};
default_textures["textures/tokyo/2_clean8a_d"] = {64,64};
default_textures["textures/tokyo/2_clean8b"] = {64,64};
default_textures["textures/tokyo/2_clean9a"] = {32,64};
default_textures["textures/tokyo/2_clean9a_d"] = {32,64};
default_textures["textures/tokyo/2_clean9b"] = {32,64};
default_textures["textures/tokyo/2_cleanacces1"] = {32,64};
default_textures["textures/tokyo/2_cleanacces1d"] = {32,64};
default_textures["textures/tokyo/2_cleanacces2"] = {32,64};
default_textures["textures/tokyo/2_cleanbig1"] = {128,128};
default_textures["textures/tokyo/2_cleanbigpanel"] = {64,128};
default_textures["textures/tokyo/2_cleanbigpaneld"] = {64,128};
default_textures["textures/tokyo/2_cleanblocks"] = {64,64};
default_textures["textures/tokyo/2_cleanceil"] = {64,64};
default_textures["textures/tokyo/2_cleandoor"] = {64,128};
default_textures["textures/tokyo/2_cleandoor1"] = {64,128};
default_textures["textures/tokyo/2_cleanpanel"] = {64,64};
default_textures["textures/tokyo/2_cleanpanel2"] = {32,64};
default_textures["textures/tokyo/2_cleanpanel2d"] = {32,64};
default_textures["textures/tokyo/2_cleanpaneld"] = {64,64};
default_textures["textures/tokyo/2_cleanrail1"] = {16,32};
default_textures["textures/tokyo/2_cleanscreen"] = {64,64};
default_textures["textures/tokyo/2_cleanscreen_d"] = {64,64};
default_textures["textures/tokyo/2_cleansteel"] = {64,64};
default_textures["textures/tokyo/2_cleantechwall"] = {64,64};
default_textures["textures/tokyo/2_cleantech_a"] = {64,16};
default_textures["textures/tokyo/2_cleantech_b"] = {16,32};
default_textures["textures/tokyo/2_cleantech_c"] = {16,16};
default_textures["textures/tokyo/2_cleantech_d"] = {16,32};
default_textures["textures/tokyo/2_cleantech_plain"] = {16,16};
default_textures["textures/tokyo/2_cleanw1"] = {32,32};
default_textures["textures/tokyo/2_cleanw2"] = {64,64};
default_textures["textures/tokyo/2_cleanw2_bot"] = {64,16};
default_textures["textures/tokyo/2_cleanw2_top"] = {64,16};
default_textures["textures/tokyo/2_cleanw3a"] = {32,16};
default_textures["textures/tokyo/2_cleanw3b"] = {32,16};
default_textures["textures/tokyo/2_cleanwall1"] = {32,32};
default_textures["textures/tokyo/2_cleanwall1_bot"] = {32,32};
default_textures["textures/tokyo/2_cleanwall1_bot2"] = {32,32};
default_textures["textures/tokyo/2_cleanwall1_top1"] = {32,32};
default_textures["textures/tokyo/2_cleanwall1_top2"] = {32,32};
default_textures["textures/tokyo/2_cleanwall1_top3"] = {32,32};
default_textures["textures/tokyo/2_clean_big2"] = {128,128};
default_textures["textures/tokyo/2_clean_bigpan1"] = {32,128};
default_textures["textures/tokyo/2_clean_bigpan1_d"] = {32,128};
default_textures["textures/tokyo/2_clean_bigpan1_top"] = {32,32};
default_textures["textures/tokyo/2_clean_bigpan2"] = {32,128};
default_textures["textures/tokyo/2_clean_ceil"] = {64,64};
default_textures["textures/tokyo/2_clean_ceiling1"] = {32,32};
default_textures["textures/tokyo/2_clean_ceiling2"] = {32,32};
default_textures["textures/tokyo/2_clean_ceiling3"] = {32,32};
default_textures["textures/tokyo/2_clean_floor1"] = {16,32};
default_textures["textures/tokyo/2_clean_grate"] = {64,64};
default_textures["textures/tokyo/2_clean_grate2"] = {64,64};
default_textures["textures/tokyo/2_clean_laserport"] = {16,16};
default_textures["textures/tokyo/2_clean_pillar1a"] = {32,128};
default_textures["textures/tokyo/2_clean_pillar1a_d"] = {32,128};
default_textures["textures/tokyo/2_clean_pillar1b"] = {32,128};
default_textures["textures/tokyo/2_clean_pillar2a_d"] = {16,128};
default_textures["textures/tokyo/2_clean_pillar2b"] = {16,128};
default_textures["textures/tokyo/2_clean_station"] = {128,128};
default_textures["textures/tokyo/2_clean_station_side1"] = {128,32};
default_textures["textures/tokyo/2_clean_station_side1d"] = {128,32};
default_textures["textures/tokyo/2_clean_station_top"] = {64,16};
default_textures["textures/tokyo/2_cube1"] = {32,64};
default_textures["textures/tokyo/2_cube2"] = {16,64};
default_textures["textures/tokyo/2_cube3"] = {16,16};
default_textures["textures/tokyo/2_doorslide"] = {64,128};
default_textures["textures/tokyo/2_doorslidetrim"] = {32,32};
default_textures["textures/tokyo/2_elev1"] = {32,32};
default_textures["textures/tokyo/2_elev2"] = {16,16};
default_textures["textures/tokyo/2_elev3"] = {16,16};
default_textures["textures/tokyo/2_elev4"] = {16,16};
default_textures["textures/tokyo/2_elevator_door"] = {32,32};
default_textures["textures/tokyo/2_elevator_door2"] = {32,128};
default_textures["textures/tokyo/2_elevcar_roof"] = {64,64};
default_textures["textures/tokyo/2_elevtrack"] = {64,64};
default_textures["textures/tokyo/2_elevtrack_side"] = {16,16};
default_textures["textures/tokyo/2_elevwall_back"] = {128,128};
default_textures["textures/tokyo/2_elevwall_doors"] = {128,128};
default_textures["textures/tokyo/2_elev_trim1"] = {32,32};
default_textures["textures/tokyo/2_floor_grate2"] = {64,64};
default_textures["textures/tokyo/2_floor_tile"] = {32,32};
default_textures["textures/tokyo/2_gen1"] = {64,128};
default_textures["textures/tokyo/2_gen2"] = {64,128};
default_textures["textures/tokyo/2_gensidea"] = {64,64};
default_textures["textures/tokyo/2_gensideb"] = {64,64};
default_textures["textures/tokyo/2_gentop"] = {64,128};
default_textures["textures/tokyo/2_grey"] = {16,16};
default_textures["textures/tokyo/2_hallceiling"] = {16,16};
default_textures["textures/tokyo/2_hallceiling1"] = {32,32};
default_textures["textures/tokyo/2_hallceiling2"] = {32,32};
default_textures["textures/tokyo/2_hallceiling3"] = {64,64};
default_textures["textures/tokyo/2_hallcenter"] = {32,32};
default_textures["textures/tokyo/2_hallfloor"] = {32,32};
default_textures["textures/tokyo/2_hallfoortrim"] = {16,16};
default_textures["textures/tokyo/2_halltop"] = {16,16};
default_textures["textures/tokyo/2_lblue"] = {16,16};
default_textures["textures/tokyo/2_locker"] = {32,32};
default_textures["textures/tokyo/2_locker_side"] = {16,16};
default_textures["textures/tokyo/2_metalwall"] = {32,32};
default_textures["textures/tokyo/2_metalwalla"] = {32,32};
default_textures["textures/tokyo/2_metalwalltop"] = {32,32};
default_textures["textures/tokyo/2_speaker_side"] = {16,16};
default_textures["textures/tokyo/2_steela"] = {64,64};
default_textures["textures/tokyo/2_white"] = {32,32};
default_textures["textures/tokyo/2_whiteboard"] = {128,64};
default_textures["textures/tokyo/2_whiteboard2"] = {128,128};
default_textures["textures/tokyo/2_wood"] = {32,32};
default_textures["textures/tokyo/2_wood1"] = {64,64};
default_textures["textures/tokyo/2_wood2"] = {32,32};
default_textures["textures/tokyo/2_wood3"] = {64,64};
default_textures["textures/tokyo/2_wood4"] = {32,32};
default_textures["textures/tokyo/2_wood_dark"] = {64,64};
default_textures["textures/tokyo/3_bed1"] = {128,128};
default_textures["textures/tokyo/3_bed2"] = {128,16};
default_textures["textures/tokyo/3_blastsheild"] = {128,64};
default_textures["textures/tokyo/3_books"] = {32,128};
default_textures["textures/tokyo/3_books2"] = {16,128};
default_textures["textures/tokyo/3_books3"] = {32,128};
default_textures["textures/tokyo/3_books4a"] = {32,128};
default_textures["textures/tokyo/3_books4b"] = {32,128};
default_textures["textures/tokyo/3_decor_door"] = {64,128};
default_textures["textures/tokyo/3_desk_top_keys"] = {32,32};
default_textures["textures/tokyo/3_dresser"] = {64,64};
default_textures["textures/tokyo/3_elev1b"] = {64,64};
default_textures["textures/tokyo/3_elev2"] = {16,16};
default_textures["textures/tokyo/3_elev3"] = {16,16};
default_textures["textures/tokyo/3_elev4"] = {16,16};
default_textures["textures/tokyo/3_elevator_door"] = {64,128};
default_textures["textures/tokyo/3_elevdoor2"] = {32,128};
default_textures["textures/tokyo/3_elevdoortopa"] = {64,16};
default_textures["textures/tokyo/3_fishglass"] = {16,64};
default_textures["textures/tokyo/3_fishtank"] = {64,64};
default_textures["textures/tokyo/3_h"] = {16,16};
default_textures["textures/tokyo/3_headboard"] = {128,64};
default_textures["textures/tokyo/3_kimono"] = {64,64};
default_textures["textures/tokyo/3_pillar"] = {32,64};
default_textures["textures/tokyo/3_pipesoily"] = {64,64};
default_textures["textures/tokyo/3_pool"] = {16,16};
default_textures["textures/tokyo/3_pooltileflr"] = {32,32};
default_textures["textures/tokyo/3_pooltrim"] = {64,32};
default_textures["textures/tokyo/3_poolwall2"] = {64,64};
default_textures["textures/tokyo/3_poolwally"] = {64,64};
default_textures["textures/tokyo/3_pool_bottom"] = {128,128};
default_textures["textures/tokyo/3_red"] = {16,16};
default_textures["textures/tokyo/3_spout"] = {32,32};
default_textures["textures/tokyo/3_storedoor"] = {64,128};
default_textures["textures/tokyo/3_wood1"] = {64,32};
default_textures["textures/tokyo/3_wood2"] = {64,32};
default_textures["textures/tokyo/3_wooddec2"] = {32,16};
default_textures["textures/tokyo/3_wooddec3"] = {32,32};
default_textures["textures/tokyo/3_wooddec_ceil"] = {32,32};
default_textures["textures/tokyo/alpha"] = {16,16};
default_textures["textures/tokyo/bathdoor"] = {64,64};
default_textures["textures/tokyo/bathdoor2"] = {64,64};
default_textures["textures/tokyo/bathdoor2_edge"] = {16,16};
default_textures["textures/tokyo/bathdoor_edge"] = {16,16};
default_textures["textures/tokyo/bathroomwall_3a"] = {16,16};
default_textures["textures/tokyo/bathroomwall_3b"] = {16,16};
default_textures["textures/tokyo/bathroom_flr1"] = {32,32};
default_textures["textures/tokyo/bathroom_wall1"] = {32,32};
default_textures["textures/tokyo/bathroom_wall1b"] = {32,32};
default_textures["textures/tokyo/bathroom_wall2"] = {32,32};
default_textures["textures/tokyo/bathroom_wall2b"] = {32,32};
default_textures["textures/tokyo/bathroom_wall4"] = {16,16};
default_textures["textures/tokyo/bathtrim"] = {16,32};
default_textures["textures/tokyo/bathtrim2"] = {32,32};
default_textures["textures/tokyo/bathtrim3"] = {16,16};
default_textures["textures/tokyo/bath_mirror"] = {128,64};
default_textures["textures/tokyo/bath_mirrord"] = {128,64};
default_textures["textures/tokyo/bath_porclin"] = {32,32};
default_textures["textures/tokyo/bath_towels"] = {64,128};
default_textures["textures/tokyo/black"] = {16,16};
default_textures["textures/tokyo/bluefloor"] = {32,32};
default_textures["textures/tokyo/brushed_metal"] = {32,32};
default_textures["textures/tokyo/brushed_metal1"] = {64,64};
default_textures["textures/tokyo/brushed_metal2"] = {32,32};
default_textures["textures/tokyo/carpet12"] = {32,32};
default_textures["textures/tokyo/carpet13a"] = {32,32};
default_textures["textures/tokyo/carpet13b"] = {32,32};
default_textures["textures/tokyo/carpet13stair1"] = {16,16};
default_textures["textures/tokyo/carpet13stair2"] = {16,16};
default_textures["textures/tokyo/carpet13stair3"] = {16,16};
default_textures["textures/tokyo/carpet13stair4"] = {16,16};
default_textures["textures/tokyo/carpet13stair5"] = {16,16};
default_textures["textures/tokyo/carpet13stair6"] = {16,16};
default_textures["textures/tokyo/carpet13_trim"] = {16,16};
default_textures["textures/tokyo/carpet14a"] = {32,32};
default_textures["textures/tokyo/carpet14b"] = {32,32};
default_textures["textures/tokyo/carpet2"] = {32,32};
default_textures["textures/tokyo/carpet4"] = {32,32};
default_textures["textures/tokyo/carpet4step"] = {16,16};
default_textures["textures/tokyo/carpet4trim"] = {16,16};
default_textures["textures/tokyo/carpet6"] = {64,64};
default_textures["textures/tokyo/carpetstep"] = {16,16};
default_textures["textures/tokyo/carpet_batha"] = {32,32};
default_textures["textures/tokyo/carpet_batha2"] = {32,32};
default_textures["textures/tokyo/carpet_bath_trim"] = {32,32};
default_textures["textures/tokyo/carpet_lobbya"] = {32,32};
default_textures["textures/tokyo/carpet_lobbyb"] = {32,32};
default_textures["textures/tokyo/carpet_lobbytrim"] = {32,32};
default_textures["textures/tokyo/clip"] = {64,64};
default_textures["textures/tokyo/corkceil2a"] = {32,32};
default_textures["textures/tokyo/corkceil2b"] = {32,32};
default_textures["textures/tokyo/corkceil2c"] = {32,32};
default_textures["textures/tokyo/corkceil2d"] = {32,32};
default_textures["textures/tokyo/corkceil2e"] = {32,32};
default_textures["textures/tokyo/corkceil2f"] = {32,32};
default_textures["textures/tokyo/cork_ceiling"] = {64,64};
default_textures["textures/tokyo/ctfb_base"] = {64,64};
default_textures["textures/tokyo/ctfr_base"] = {64,64};
default_textures["textures/tokyo/door_dark"] = {64,128};
default_textures["textures/tokyo/duct1"] = {64,64};
default_textures["textures/tokyo/duct2"] = {64,64};
default_textures["textures/tokyo/d_genericsmooth"] = {64,64};
default_textures["textures/tokyo/d_metalsmooth"] = {64,64};
default_textures["textures/tokyo/d_stonecracked"] = {64,64};
default_textures["textures/tokyo/d_stonesmooth"] = {64,64};
default_textures["textures/tokyo/d_woodsmooth"] = {64,64};
default_textures["textures/tokyo/d_woodsmooth2"] = {64,64};
default_textures["textures/tokyo/elev1"] = {32,64};
default_textures["textures/tokyo/elev2"] = {32,64};
default_textures["textures/tokyo/elev2d"] = {32,64};
default_textures["textures/tokyo/elevnumb87"] = {32,32};
default_textures["textures/tokyo/elevnumb_d"] = {32,32};
default_textures["textures/tokyo/env1a"] = {256,256};
default_textures["textures/tokyo/env1b"] = {256,256};
default_textures["textures/tokyo/env1c"] = {256,256};
default_textures["textures/tokyo/exit"] = {64,32};
default_textures["textures/tokyo/exit2"] = {64,32};
default_textures["textures/tokyo/exit_sign"] = {64,32};
default_textures["textures/tokyo/fanblade"] = {32,16};
default_textures["textures/tokyo/file_front"] = {32,64};
default_textures["textures/tokyo/file_side"] = {32,64};
default_textures["textures/tokyo/file_top"] = {16,16};
default_textures["textures/tokyo/fire_glass"] = {32,32};
default_textures["textures/tokyo/fuse1"] = {32,64};
default_textures["textures/tokyo/fuse1d"] = {32,64};
default_textures["textures/tokyo/fuse2"] = {32,64};
default_textures["textures/tokyo/fuse2d"] = {32,64};
default_textures["textures/tokyo/fuse3"] = {32,64};
default_textures["textures/tokyo/glass1"] = {128,64};
default_textures["textures/tokyo/glass2"] = {32,32};
default_textures["textures/tokyo/glasshelf"] = {16,16};
default_textures["textures/tokyo/gold"] = {16,16};
default_textures["textures/tokyo/hint"] = {64,64};
default_textures["textures/tokyo/ladder_rung1"] = {32,16};
default_textures["textures/tokyo/ladder_rung2"] = {32,16};
default_textures["textures/tokyo/light_blue"] = {32,32};
default_textures["textures/tokyo/light_blue3"] = {32,32};
default_textures["textures/tokyo/light_blue9"] = {32,32};
default_textures["textures/tokyo/light_classy"] = {32,32};
default_textures["textures/tokyo/light_clean1"] = {16,16};
default_textures["textures/tokyo/light_clean3"] = {32,32};
default_textures["textures/tokyo/light_clean4"] = {32,16};
default_textures["textures/tokyo/light_crappy"] = {32,32};
default_textures["textures/tokyo/light_floresc"] = {32,16};
default_textures["textures/tokyo/light_florescent"] = {16,16};
default_textures["textures/tokyo/light_floresc_new"] = {64,32};
default_textures["textures/tokyo/light_pool"] = {32,32};
default_textures["textures/tokyo/light_recessed"] = {32,32};
default_textures["textures/tokyo/light_recessed2"] = {32,32};
default_textures["textures/tokyo/light_recessed3"] = {32,32};
default_textures["textures/tokyo/light_recessed4"] = {32,32};
default_textures["textures/tokyo/light_recessed4b"] = {16,16};
default_textures["textures/tokyo/light_red"] = {16,16};
default_textures["textures/tokyo/light_sign10a"] = {32,64};
default_textures["textures/tokyo/light_sign10b"] = {32,64};
default_textures["textures/tokyo/light_sign10c"] = {32,64};
default_textures["textures/tokyo/light_sign10d"] = {32,64};
default_textures["textures/tokyo/light_sign11a"] = {64,64};
default_textures["textures/tokyo/light_sign11b"] = {64,64};
default_textures["textures/tokyo/light_sign12"] = {64,16};
default_textures["textures/tokyo/light_sign13"] = {64,16};
default_textures["textures/tokyo/light_sign15"] = {64,16};
default_textures["textures/tokyo/light_sign16"] = {16,128};
default_textures["textures/tokyo/light_sign3"] = {32,128};
default_textures["textures/tokyo/light_sign6"] = {32,64};
default_textures["textures/tokyo/light_sign7a"] = {64,64};
default_textures["textures/tokyo/light_sign7b"] = {64,64};
default_textures["textures/tokyo/light_sign7c"] = {64,64};
default_textures["textures/tokyo/light_sign8a"] = {32,128};
default_textures["textures/tokyo/light_sign8b"] = {32,128};
default_textures["textures/tokyo/light_sign9a"] = {128,32};
default_textures["textures/tokyo/light_sign9b"] = {128,32};
default_textures["textures/tokyo/light_sign_fish2"] = {64,64};
default_textures["textures/tokyo/light_small"] = {32,16};
default_textures["textures/tokyo/light_strip1"] = {32,32};
default_textures["textures/tokyo/light_strip2"] = {32,32};
default_textures["textures/tokyo/light_tube"] = {64,32};
default_textures["textures/tokyo/light_tube2"] = {64,32};
default_textures["textures/tokyo/light_warm"] = {16,16};
default_textures["textures/tokyo/light_white"] = {32,32};
default_textures["textures/tokyo/light_yellow"] = {64,32};
default_textures["textures/tokyo/marbleblack"] = {64,64};
default_textures["textures/tokyo/marblefloor1"] = {32,32};
default_textures["textures/tokyo/marblefloor2"] = {32,32};
default_textures["textures/tokyo/marblefloor3"] = {64,64};
default_textures["textures/tokyo/marblefloor4"] = {128,128};
default_textures["textures/tokyo/marbletan"] = {64,64};
default_textures["textures/tokyo/marbletrim1"] = {32,32};
default_textures["textures/tokyo/marbletrim1a"] = {32,32};
default_textures["textures/tokyo/marblewall1"] = {64,64};
default_textures["textures/tokyo/marblewall1_trim1"] = {32,16};
default_textures["textures/tokyo/marblewall1_trim2"] = {32,16};
default_textures["textures/tokyo/marblewall_trim1"] = {32,16};
default_textures["textures/tokyo/marblewall_trim2"] = {32,32};
default_textures["textures/tokyo/marble_trim4"] = {32,32};
default_textures["textures/tokyo/med_door"] = {32,32};
default_textures["textures/tokyo/med_red"] = {16,16};
default_textures["textures/tokyo/med_sides"] = {16,16};
default_textures["textures/tokyo/med_stripe"] = {16,16};
default_textures["textures/tokyo/metal_clean"] = {64,64};
default_textures["textures/tokyo/metal_cool"] = {32,32};
default_textures["textures/tokyo/metal_dark"] = {32,32};
default_textures["textures/tokyo/mirror"] = {32,32};
default_textures["textures/tokyo/mirror_d"] = {32,32};
default_textures["textures/tokyo/no_draw"] = {64,64};
default_textures["textures/tokyo/origin"] = {64,64};
default_textures["textures/tokyo/painting1"] = {64,128};
default_textures["textures/tokyo/painting10"] = {64,128};
default_textures["textures/tokyo/painting11"] = {64,128};
default_textures["textures/tokyo/painting12"] = {128,128};
default_textures["textures/tokyo/painting2"] = {64,128};
default_textures["textures/tokyo/painting3"] = {64,128};
default_textures["textures/tokyo/painting4"] = {64,128};
default_textures["textures/tokyo/painting5"] = {64,32};
default_textures["textures/tokyo/painting6"] = {128,128};
default_textures["textures/tokyo/painting7"] = {64,128};
default_textures["textures/tokyo/painting8"] = {64,128};
default_textures["textures/tokyo/painting9"] = {64,128};
default_textures["textures/tokyo/restroom1"] = {32,32};
default_textures["textures/tokyo/restroom2"] = {32,32};
default_textures["textures/tokyo/sign1"] = {64,32};
default_textures["textures/tokyo/sign2"] = {32,32};
default_textures["textures/tokyo/sign3"] = {16,16};
default_textures["textures/tokyo/sign_neon1"] = {128,64};
default_textures["textures/tokyo/sign_neon1d"] = {128,64};
default_textures["textures/tokyo/skip"] = {64,64};
default_textures["textures/tokyo/sky"] = {64,64};
default_textures["textures/tokyo/stair_side"] = {16,16};
default_textures["textures/tokyo/stair_side2"] = {16,16};
default_textures["textures/tokyo/trigger"] = {64,64};
default_textures["textures/tokyo/tv_monitor"] = {32,32};
default_textures["textures/tokyo/tv_monitor_d"] = {32,32};
default_textures["textures/tokyo/vend1_fronta"] = {64,64};
default_textures["textures/tokyo/vend1_fronta_d"] = {64,64};
default_textures["textures/tokyo/vend1_frontb"] = {64,32};
default_textures["textures/tokyo/vend1_frontb_d"] = {64,32};
default_textures["textures/tokyo/vend1_sidea"] = {64,64};
default_textures["textures/tokyo/vend1_sidead"] = {64,64};
default_textures["textures/tokyo/vend1_sideb"] = {64,32};
default_textures["textures/tokyo/vend1_sidebd"] = {64,32};
default_textures["textures/tokyo/vend2_fronta_d"] = {64,64};
default_textures["textures/tokyo/vend2_frontb_d"] = {64,32};
default_textures["textures/tokyo/vend2_frta"] = {64,64};
default_textures["textures/tokyo/vend2_frtb"] = {64,32};
default_textures["textures/tokyo/vend2_sidea"] = {64,64};
default_textures["textures/tokyo/vend2_sidea_d"] = {64,64};
default_textures["textures/tokyo/vend2_sideb"] = {64,32};
default_textures["textures/tokyo/vend2_sideb_d"] = {64,32};
default_textures["textures/tokyo/vend_top"] = {64,64};
default_textures["textures/tokyo/vent"] = {64,32};
default_textures["textures/tokyo/wall1"] = {32,32};
default_textures["textures/tokyo/wall2"] = {32,32};
default_textures["textures/tokyo/wallpaper1"] = {64,64};
default_textures["textures/tokyo/wallpaper1_base"] = {64,64};
default_textures["textures/tokyo/wallpaper1_trim2"] = {16,16};
default_textures["textures/tokyo/wallpaper4"] = {32,32};
default_textures["textures/tokyo/wallpaper4_base"] = {32,32};
default_textures["textures/tokyo/wallpaper4_top"] = {32,16};
default_textures["textures/tokyo/wallpaper5"] = {64,64};
default_textures["textures/tokyo/wallpaper5_trim"] = {32,16};
default_textures["textures/tokyo/wallpaper7"] = {16,16};
default_textures["textures/tokyo/wallpaper8"] = {64,64};
default_textures["textures/tokyo/wallpaper8_base"] = {32,32};
default_textures["textures/tokyo/wallpaper8_top"] = {32,32};
default_textures["textures/tokyo/wallpaper8_trim"] = {32,32};
default_textures["textures/tokyo/wallpaper8_trim2"] = {32,32};
default_textures["textures/tokyo/wallpaper_lobbytop"] = {32,32};
default_textures["textures/tokyo/walltrim3"] = {16,16};
default_textures["textures/tokyo/walltrim6"] = {16,32};
default_textures["textures/tokyo/walltrim7"] = {16,16};
default_textures["textures/tokyo/water"] = {128,128};
default_textures["textures/tokyo/woodbase"] = {32,32};
default_textures["textures/tokyo/woodtrim"] = {128,64};
default_textures["textures/tokyo/wood_planks"] = {32,32};
default_textures["textures/uganda/1_2_brickgrn"] = {64,64};
default_textures["textures/uganda/1_2_brickgrn2"] = {128,128};
default_textures["textures/uganda/1_2_brickgrn_top"] = {64,32};
default_textures["textures/uganda/1_2_builds"] = {128,128};
default_textures["textures/uganda/1_2_builds_bot"] = {128,64};
default_textures["textures/uganda/1_2_builds_win"] = {128,128};
default_textures["textures/uganda/1_2_builds_wind"] = {128,128};
default_textures["textures/uganda/1_2_buildw"] = {128,128};
default_textures["textures/uganda/1_2_buildw_beam"] = {128,128};
default_textures["textures/uganda/1_2_buildw_win"] = {128,128};
default_textures["textures/uganda/1_2_buildw_wind"] = {128,128};
default_textures["textures/uganda/1_2_build_trans"] = {128,128};
default_textures["textures/uganda/1_2_build_trans_beam"] = {128,128};
default_textures["textures/uganda/1_2_crackwall"] = {128,128};
default_textures["textures/uganda/1_2_crackwall_btm"] = {128,64};
default_textures["textures/uganda/1_2_crackwall_top"] = {128,64};
default_textures["textures/uganda/1_2_crete1"] = {64,64};
default_textures["textures/uganda/1_2_crete1_bot"] = {64,32};
default_textures["textures/uganda/1_2_crete1_trans"] = {64,32};
default_textures["textures/uganda/1_2_crete2"] = {64,64};
default_textures["textures/uganda/1_2_flatwood"] = {64,64};
default_textures["textures/uganda/1_2_hayfront"] = {64,64};
default_textures["textures/uganda/1_2_hayside"] = {128,64};
default_textures["textures/uganda/1_2_inside1"] = {64,64};
default_textures["textures/uganda/1_2_inside2"] = {64,64};
default_textures["textures/uganda/1_2_inside2_bot"] = {64,64};
default_textures["textures/uganda/1_2_inside2_top"] = {64,32};
default_textures["textures/uganda/1_2_inside2_wire"] = {64,64};
default_textures["textures/uganda/1_2_inside3"] = {64,64};
default_textures["textures/uganda/1_2_insideb_2"] = {64,64};
default_textures["textures/uganda/1_2_inside_bot1"] = {64,64};
default_textures["textures/uganda/1_2_inside_top"] = {64,32};
default_textures["textures/uganda/1_2_mwood1"] = {128,128};
default_textures["textures/uganda/1_2_mwood2"] = {128,128};
default_textures["textures/uganda/1_2_mwood3"] = {128,128};
default_textures["textures/uganda/1_2_redbrick"] = {64,64};
default_textures["textures/uganda/1_2_redbrick2"] = {128,128};
default_textures["textures/uganda/1_2_redbrick_bot"] = {64,32};
default_textures["textures/uganda/1_2_redbrick_top"] = {64,32};
default_textures["textures/uganda/1_2_roof1"] = {64,64};
default_textures["textures/uganda/1_2_rug1"] = {128,128};
default_textures["textures/uganda/1_2_wbeam1"] = {16,64};
default_textures["textures/uganda/1_2_wbeam2"] = {16,64};
default_textures["textures/uganda/1_2_wgate"] = {128,128};
default_textures["textures/uganda/1_2_woodalpha1"] = {64,64};
default_textures["textures/uganda/1_2_woodfloor"] = {128,128};
default_textures["textures/uganda/1_2_woodplank2"] = {64,32};
default_textures["textures/uganda/1_3_stairside_wood"] = {32,16};
default_textures["textures/uganda/1_3_stairslab"] = {64,64};
default_textures["textures/uganda/1_3_stairtop_wood"] = {32,64};
default_textures["textures/uganda/1_button_off"] = {32,32};
default_textures["textures/uganda/1_button_on"] = {32,32};
default_textures["textures/uganda/1_cinderb"] = {64,64};
default_textures["textures/uganda/1_cinderb2"] = {64,64};
default_textures["textures/uganda/1_cinderb2_bot"] = {64,64};
default_textures["textures/uganda/1_cinderb3"] = {64,64};
default_textures["textures/uganda/1_cinderb3_bot"] = {64,64};
default_textures["textures/uganda/1_cinderb_bot"] = {64,64};
default_textures["textures/uganda/1_cinderb_bot2"] = {64,64};
default_textures["textures/uganda/1_cinderb_bot_bot"] = {64,32};
default_textures["textures/uganda/1_cinderb_top"] = {64,32};
default_textures["textures/uganda/1_clamp1"] = {64,32};
default_textures["textures/uganda/1_clamp2"] = {16,16};
default_textures["textures/uganda/1_clamp3"] = {32,32};
default_textures["textures/uganda/1_controlpanel2"] = {64,32};
default_textures["textures/uganda/1_controlpanel2b"] = {128,64};
default_textures["textures/uganda/1_controlpanel2b_d"] = {128,64};
default_textures["textures/uganda/1_controlpanel2_d"] = {64,32};
default_textures["textures/uganda/1_crackdirt"] = {128,128};
default_textures["textures/uganda/1_dirt"] = {128,128};
default_textures["textures/uganda/1_dirt2"] = {128,128};
default_textures["textures/uganda/1_grass"] = {128,128};
default_textures["textures/uganda/1_grassdirt"] = {128,128};
default_textures["textures/uganda/1_grassdirt_track"] = {128,128};
default_textures["textures/uganda/1_grasstrans"] = {128,128};
default_textures["textures/uganda/1_metalfloor"] = {64,64};
default_textures["textures/uganda/1_metalwall"] = {64,64};
default_textures["textures/uganda/1_railtop"] = {16,16};
default_textures["textures/uganda/1_redbrick"] = {128,128};
default_textures["textures/uganda/1_redbrick_bot1"] = {128,128};
default_textures["textures/uganda/1_redbrick_damage1"] = {128,128};
default_textures["textures/uganda/1_redbrick_damage2"] = {128,128};
default_textures["textures/uganda/1_redbrick_damage3"] = {128,128};
default_textures["textures/uganda/1_redbrick_top"] = {128,64};
default_textures["textures/uganda/1_redbrick_win2"] = {128,128};
default_textures["textures/uganda/1_redbrick_win2d"] = {128,128};
default_textures["textures/uganda/1_redbrick_win3"] = {128,128};
default_textures["textures/uganda/1_sand"] = {128,128};
default_textures["textures/uganda/1_stucco_1"] = {128,128};
default_textures["textures/uganda/1_stucco_1b"] = {128,128};
default_textures["textures/uganda/1_stucco_1c"] = {128,128};
default_textures["textures/uganda/1_stucco_1d"] = {128,128};
default_textures["textures/uganda/1_stucco_1e"] = {128,64};
default_textures["textures/uganda/1_trim1"] = {32,32};
default_textures["textures/uganda/1_woodbeam_1"] = {64,128};
default_textures["textures/uganda/1_woodfence"] = {64,128};
default_textures["textures/uganda/1_woodtrim_1"] = {64,128};
default_textures["textures/uganda/1_wtrim1"] = {64,32};
default_textures["textures/uganda/2_3_clock"] = {64,64};
default_textures["textures/uganda/2_3_clockd"] = {64,64};
default_textures["textures/uganda/2_3_florescent"] = {32,16};
default_textures["textures/uganda/2_3_mfloor"] = {64,64};
default_textures["textures/uganda/2_bfloor1"] = {64,64};
default_textures["textures/uganda/2_bfloor1a"] = {64,64};
default_textures["textures/uganda/2_bfloor2"] = {64,64};
default_textures["textures/uganda/2_bfloor2c"] = {64,64};
default_textures["textures/uganda/2_bfloormetal"] = {64,64};
default_textures["textures/uganda/2_bgrate"] = {64,64};
default_textures["textures/uganda/2_blong_grate"] = {16,128};
default_textures["textures/uganda/2_blood1"] = {64,64};
default_textures["textures/uganda/2_bsew1"] = {128,128};
default_textures["textures/uganda/2_bsew1plain"] = {128,128};
default_textures["textures/uganda/2_bsew1_bot"] = {128,64};
default_textures["textures/uganda/2_bsew2"] = {128,128};
default_textures["textures/uganda/2_bsewfloor1"] = {128,128};
default_textures["textures/uganda/2_bsewfloor3"] = {128,128};
default_textures["textures/uganda/2_btileb"] = {64,64};
default_textures["textures/uganda/2_btileb_clean"] = {64,64};
default_textures["textures/uganda/2_btile_clean"] = {64,64};
default_textures["textures/uganda/2_btile_top"] = {64,32};
default_textures["textures/uganda/2_btrapdoor"] = {32,128};
default_textures["textures/uganda/2_bulletin"] = {128,64};
default_textures["textures/uganda/2_con2"] = {64,64};
default_textures["textures/uganda/2_con3"] = {64,64};
default_textures["textures/uganda/2_con4"] = {64,64};
default_textures["textures/uganda/2_confloor1"] = {128,128};
default_textures["textures/uganda/2_confloor2"] = {64,64};
default_textures["textures/uganda/2_controlpanel"] = {64,32};
default_textures["textures/uganda/2_controlpaneld"] = {64,32};
default_textures["textures/uganda/2_door"] = {64,128};
default_textures["textures/uganda/2_freezebeam"] = {16,64};
default_textures["textures/uganda/2_freezebeam_track"] = {16,64};
default_textures["textures/uganda/2_freezebeam_trackend"] = {16,16};
default_textures["textures/uganda/2_freezerceil"] = {64,64};
default_textures["textures/uganda/2_freezerfloor"] = {64,64};
default_textures["textures/uganda/2_freezerflr2"] = {64,64};
default_textures["textures/uganda/2_freezertrim2"] = {64,32};
default_textures["textures/uganda/2_freezerwall1a"] = {64,128};
default_textures["textures/uganda/2_freezerwall1b"] = {64,64};
default_textures["textures/uganda/2_freezerwall2a"] = {64,64};
default_textures["textures/uganda/2_freezetrim"] = {32,32};
default_textures["textures/uganda/2_freeze_door1"] = {64,128};
default_textures["textures/uganda/2_freeze_door2"] = {64,128};
default_textures["textures/uganda/2_meatslabs"] = {64,32};
default_textures["textures/uganda/2_metalside"] = {64,64};
default_textures["textures/uganda/2_roughwall"] = {64,64};
default_textures["textures/uganda/2_roughwall2"] = {64,64};
default_textures["textures/uganda/2_shutedoor"] = {64,128};
default_textures["textures/uganda/2_steelwall1a"] = {64,128};
default_textures["textures/uganda/2_tabletop1"] = {64,32};
default_textures["textures/uganda/2_tabletop2"] = {128,32};
default_textures["textures/uganda/2_woodfloor"] = {128,128};
default_textures["textures/uganda/3_big_pipe1"] = {64,64};
default_textures["textures/uganda/3_big_pipe2"] = {64,64};
default_textures["textures/uganda/3_big_pipe2b"] = {64,64};
default_textures["textures/uganda/3_big_pipe2c"] = {64,64};
default_textures["textures/uganda/3_bulletin2"] = {128,64};
default_textures["textures/uganda/3_catwalk"] = {64,64};
default_textures["textures/uganda/3_catwalk2"] = {64,64};
default_textures["textures/uganda/3_catwalk3"] = {128,128};
default_textures["textures/uganda/3_cavefloor1"] = {128,128};
default_textures["textures/uganda/3_caverock"] = {128,128};
default_textures["textures/uganda/3_computer"] = {64,64};
default_textures["textures/uganda/3_computer2"] = {64,128};
default_textures["textures/uganda/3_computer2b"] = {64,128};
default_textures["textures/uganda/3_computer2d"] = {64,128};
default_textures["textures/uganda/3_computer3a"] = {128,64};
default_textures["textures/uganda/3_computer3b"] = {128,64};
default_textures["textures/uganda/3_computer3d"] = {128,64};
default_textures["textures/uganda/3_computer3_front"] = {64,64};
default_textures["textures/uganda/3_computer3_side"] = {64,64};
default_textures["textures/uganda/3_computer3_side2"] = {64,128};
default_textures["textures/uganda/3_computer4a"] = {64,64};
default_textures["textures/uganda/3_computer4b"] = {64,64};
default_textures["textures/uganda/3_computer4d"] = {64,64};
default_textures["textures/uganda/3_computerb"] = {64,64};
default_textures["textures/uganda/3_computerd"] = {64,64};
default_textures["textures/uganda/3_concrete"] = {128,128};
default_textures["textures/uganda/3_controlpanel"] = {64,32};
default_textures["textures/uganda/3_conveyor"] = {128,128};
default_textures["textures/uganda/3_conveyor2"] = {128,128};
default_textures["textures/uganda/3_conveyorside"] = {64,64};
default_textures["textures/uganda/3_conveyorside_bld"] = {64,64};
default_textures["textures/uganda/3_crate1"] = {64,64};
default_textures["textures/uganda/3_crusher"] = {64,64};
default_textures["textures/uganda/3_door_bot"] = {128,64};
default_textures["textures/uganda/3_door_top"] = {128,64};
default_textures["textures/uganda/3_fdoor"] = {128,64};
default_textures["textures/uganda/3_girdwall"] = {128,128};
default_textures["textures/uganda/3_light_red"] = {32,32};
default_textures["textures/uganda/3_mbeam"] = {16,64};
default_textures["textures/uganda/3_mbeam2"] = {16,64};
default_textures["textures/uganda/3_mbeam_track"] = {16,64};
default_textures["textures/uganda/3_mbeam_trackend"] = {16,16};
default_textures["textures/uganda/3_mbigbolt"] = {128,128};
default_textures["textures/uganda/3_mbolts"] = {64,64};
default_textures["textures/uganda/3_mceiling"] = {128,128};
default_textures["textures/uganda/3_metalrivet"] = {128,128};
default_textures["textures/uganda/3_metalrivets"] = {128,128};
default_textures["textures/uganda/3_metalside"] = {64,64};
default_textures["textures/uganda/3_meter2"] = {64,64};
default_textures["textures/uganda/3_mfloor1"] = {128,128};
default_textures["textures/uganda/3_mfloor2"] = {128,128};
default_textures["textures/uganda/3_mfloor3"] = {128,128};
default_textures["textures/uganda/3_mfloor4"] = {128,128};
default_textures["textures/uganda/3_mis1"] = {64,64};
default_textures["textures/uganda/3_mis2"] = {64,64};
default_textures["textures/uganda/3_mis3"] = {64,64};
default_textures["textures/uganda/3_mis4"] = {64,64};
default_textures["textures/uganda/3_misa"] = {32,32};
default_textures["textures/uganda/3_misb"] = {32,32};
default_textures["textures/uganda/3_misback"] = {64,64};
default_textures["textures/uganda/3_misc"] = {32,32};
default_textures["textures/uganda/3_misfin"] = {128,64};
default_textures["textures/uganda/3_mismetal"] = {32,64};
default_textures["textures/uganda/3_molten1"] = {128,128};
default_textures["textures/uganda/3_mplate"] = {128,128};
default_textures["textures/uganda/3_mplate2"] = {128,128};
default_textures["textures/uganda/3_mplate3"] = {128,128};
default_textures["textures/uganda/3_mribs"] = {128,128};
default_textures["textures/uganda/3_mrust1"] = {64,64};
default_textures["textures/uganda/3_mrust2"] = {64,64};
default_textures["textures/uganda/3_mrust3"] = {64,64};
default_textures["textures/uganda/3_mrust4"] = {64,64};
default_textures["textures/uganda/3_mrwall"] = {128,128};
default_textures["textures/uganda/3_mstrips"] = {64,64};
default_textures["textures/uganda/3_mtile1"] = {64,64};
default_textures["textures/uganda/3_mtile2"] = {64,64};
default_textures["textures/uganda/3_mtrim1"] = {64,32};
default_textures["textures/uganda/3_mtrim2"] = {64,32};
default_textures["textures/uganda/3_panel5"] = {64,32};
default_textures["textures/uganda/3_panel5d"] = {64,32};
default_textures["textures/uganda/3_panelmike"] = {64,64};
default_textures["textures/uganda/3_panelmike2"] = {64,64};
default_textures["textures/uganda/3_pipe_rusty"] = {64,32};
default_textures["textures/uganda/3_rustplate"] = {64,64};
default_textures["textures/uganda/3_steelwall1"] = {64,128};
default_textures["textures/uganda/3_steelwall2"] = {64,128};
default_textures["textures/uganda/3_steelwall3"] = {64,128};
default_textures["textures/uganda/3_trim1"] = {32,32};
default_textures["textures/uganda/alpha"] = {16,16};
default_textures["textures/uganda/barbwire"] = {128,64};
default_textures["textures/uganda/bars1"] = {64,64};
default_textures["textures/uganda/black"] = {16,16};
default_textures["textures/uganda/button_grn"] = {16,32};
default_textures["textures/uganda/button_grn_d"] = {16,32};
default_textures["textures/uganda/calendar2"] = {32,64};
default_textures["textures/uganda/calendar2b"] = {32,64};
default_textures["textures/uganda/chain1"] = {16,64};
default_textures["textures/uganda/chainlink"] = {16,16};
default_textures["textures/uganda/chainlink_pipe"] = {8,32};
default_textures["textures/uganda/Clip"] = {64,64};
default_textures["textures/uganda/cloud"] = {128,128};
default_textures["textures/uganda/con6"] = {64,64};
default_textures["textures/uganda/crate1"] = {64,64};
default_textures["textures/uganda/crate2"] = {64,64};
default_textures["textures/uganda/crate3"] = {64,64};
default_textures["textures/uganda/crate4"] = {64,64};
default_textures["textures/uganda/crate_long"] = {64,32};
default_textures["textures/uganda/ctfb_1_cinderb2"] = {64,64};
default_textures["textures/uganda/ctfb_1_cinderb2_bot"] = {64,64};
default_textures["textures/uganda/ctfb_1_cinderb_bot"] = {64,64};
default_textures["textures/uganda/ctfb_1_cinderb_bot2"] = {64,64};
default_textures["textures/uganda/ctfb_1_trim1"] = {32,32};
default_textures["textures/uganda/ctfb_3_caverock"] = {128,128};
default_textures["textures/uganda/ctfb_3_mplate"] = {128,128};
default_textures["textures/uganda/ctfb_3_mrust2"] = {64,64};
default_textures["textures/uganda/ctfb_base"] = {64,64};
default_textures["textures/uganda/ctfb_light_flor"] = {32,64};
default_textures["textures/uganda/ctfb_light_square"] = {32,32};
default_textures["textures/uganda/ctfb_light_yellow3"] = {32,32};
default_textures["textures/uganda/ctfr_1_cinderb2"] = {64,64};
default_textures["textures/uganda/ctfr_1_cinderb2_bot"] = {64,64};
default_textures["textures/uganda/ctfr_1_cinderb_bot"] = {64,64};
default_textures["textures/uganda/ctfr_1_cinderb_bot2"] = {64,64};
default_textures["textures/uganda/ctfr_3_caverock"] = {128,128};
default_textures["textures/uganda/ctfr_3_mplate"] = {128,128};
default_textures["textures/uganda/ctfr_3_mrust2"] = {64,64};
default_textures["textures/uganda/ctfr_base"] = {64,64};
default_textures["textures/uganda/ctfr_light_flor"] = {32,64};
default_textures["textures/uganda/ctfr_light_square"] = {32,32};
default_textures["textures/uganda/ctfr_light_yellow3"] = {32,32};
default_textures["textures/uganda/desktop"] = {128,64};
default_textures["textures/uganda/door1"] = {64,128};
default_textures["textures/uganda/door2"] = {64,128};
default_textures["textures/uganda/door3"] = {64,128};
default_textures["textures/uganda/door4"] = {64,128};
default_textures["textures/uganda/doorside_green"] = {16,16};
default_textures["textures/uganda/doorside_grey"] = {16,16};
default_textures["textures/uganda/duct_rust_btm"] = {64,64};
default_textures["textures/uganda/duct_rust_ceiling"] = {64,64};
default_textures["textures/uganda/duct_rust_side"] = {64,64};
default_textures["textures/uganda/d_dirt"] = {128,128};
default_textures["textures/uganda/d_genericsmooth"] = {64,64};
default_textures["textures/uganda/d_grass"] = {128,128};
default_textures["textures/uganda/d_metalbump"] = {32,32};
default_textures["textures/uganda/d_metalrough"] = {64,64};
default_textures["textures/uganda/d_metalsmooth"] = {64,64};
default_textures["textures/uganda/d_sand"] = {64,64};
default_textures["textures/uganda/d_stonecracked"] = {64,64};
default_textures["textures/uganda/d_stonerough"] = {128,128};
default_textures["textures/uganda/d_stonesmooth"] = {64,64};
default_textures["textures/uganda/d_stucco"] = {64,64};
default_textures["textures/uganda/d_woodrough"] = {64,64};
default_textures["textures/uganda/d_woodrough2"] = {64,64};
default_textures["textures/uganda/d_woodsmooth"] = {64,64};
default_textures["textures/uganda/d_woodsmooth2"] = {64,64};
default_textures["textures/uganda/env1a"] = {256,256};
default_textures["textures/uganda/env1b"] = {256,256};
default_textures["textures/uganda/env1c"] = {256,256};
default_textures["textures/uganda/file_front"] = {32,64};
default_textures["textures/uganda/file_frontd"] = {32,64};
default_textures["textures/uganda/file_side"] = {32,64};
default_textures["textures/uganda/file_top"] = {32,32};
default_textures["textures/uganda/gear_side"] = {64,64};
default_textures["textures/uganda/girdwall"] = {128,128};
default_textures["textures/uganda/girdwall2"] = {128,128};
default_textures["textures/uganda/glass1"] = {64,64};
default_textures["textures/uganda/glass_wire"] = {64,64};
default_textures["textures/uganda/glass_wire_d"] = {64,64};
default_textures["textures/uganda/hint"] = {64,64};
default_textures["textures/uganda/k_3_steelwall2b"] = {64,128};
default_textures["textures/uganda/ladder_rung1"] = {32,16};
default_textures["textures/uganda/ladder_rung2"] = {32,16};
default_textures["textures/uganda/light_blue1"] = {32,32};
default_textures["textures/uganda/light_blue2"] = {32,32};
default_textures["textures/uganda/light_flor"] = {32,64};
default_textures["textures/uganda/light_square"] = {32,32};
default_textures["textures/uganda/light_tungsten"] = {32,32};
default_textures["textures/uganda/light_yellow1"] = {32,32};
default_textures["textures/uganda/light_yellow2"] = {32,32};
default_textures["textures/uganda/light_yellow3"] = {32,32};
default_textures["textures/uganda/logs1"] = {64,64};
default_textures["textures/uganda/logs2"] = {64,64};
default_textures["textures/uganda/med_door"] = {32,32};
default_textures["textures/uganda/med_inside"] = {32,32};
default_textures["textures/uganda/med_inside_used"] = {32,32};
default_textures["textures/uganda/med_red"] = {16,16};
default_textures["textures/uganda/med_sides"] = {16,16};
default_textures["textures/uganda/med_stripe"] = {16,16};
default_textures["textures/uganda/metal2"] = {32,32};
default_textures["textures/uganda/metalside1"] = {64,64};
default_textures["textures/uganda/metalside3"] = {16,16};
default_textures["textures/uganda/metal_clean"] = {64,64};
default_textures["textures/uganda/metal_cool"] = {32,32};
default_textures["textures/uganda/metal_dark"] = {32,32};
default_textures["textures/uganda/metal_rust"] = {64,64};
default_textures["textures/uganda/metal_smooth"] = {64,64};
default_textures["textures/uganda/meter1"] = {64,64};
default_textures["textures/uganda/meter1d"] = {64,64};
default_textures["textures/uganda/no_draw"] = {64,64};
default_textures["textures/uganda/Origin"] = {64,64};
default_textures["textures/uganda/paint_orange"] = {16,16};
default_textures["textures/uganda/screen1"] = {64,64};
default_textures["textures/uganda/sign1"] = {128,64};
default_textures["textures/uganda/sign2"] = {64,64};
default_textures["textures/uganda/sign3"] = {64,64};
default_textures["textures/uganda/skip"] = {64,64};
default_textures["textures/uganda/sky"] = {64,64};
default_textures["textures/uganda/stair_side"] = {16,16};
default_textures["textures/uganda/stair_side2"] = {16,16};
default_textures["textures/uganda/stair_top1"] = {32,64};
default_textures["textures/uganda/trigger"] = {64,64};
default_textures["textures/uganda/trim2"] = {32,32};
default_textures["textures/uganda/trim3_wood"] = {64,32};
default_textures["textures/uganda/t_belt"] = {16,16};
default_textures["textures/uganda/t_boxcar1_door"] = {64,128};
default_textures["textures/uganda/t_boxcar1_roof_door"] = {64,64};
default_textures["textures/uganda/t_boxcar1_roof_flr"] = {64,64};
default_textures["textures/uganda/t_boxcar1_side"] = {64,128};
default_textures["textures/uganda/t_boxcar1_side2"] = {64,128};
default_textures["textures/uganda/t_boxcar2_door"] = {64,128};
default_textures["textures/uganda/t_boxcar2_roof_flr"] = {64,64};
default_textures["textures/uganda/t_cab_side1"] = {64,128};
default_textures["textures/uganda/t_cab_side2"] = {64,128};
default_textures["textures/uganda/t_carbackmid"] = {64,128};
default_textures["textures/uganda/t_carbackside"] = {64,128};
default_textures["textures/uganda/t_cardoor"] = {64,128};
default_textures["textures/uganda/t_carside"] = {64,128};
default_textures["textures/uganda/t_cartop2_mid"] = {64,64};
default_textures["textures/uganda/t_cartop2_side"] = {64,64};
default_textures["textures/uganda/t_cartop_mid"] = {64,64};
default_textures["textures/uganda/t_cartop_side"] = {64,64};
default_textures["textures/uganda/t_coal"] = {64,64};
default_textures["textures/uganda/t_conpipe_trim"] = {16,128};
default_textures["textures/uganda/t_dirt_sandtran"] = {128,64};
default_textures["textures/uganda/t_dirt_slopea"] = {64,128};
default_textures["textures/uganda/t_dirt_slopeb"] = {64,128};
default_textures["textures/uganda/t_door"] = {64,128};
default_textures["textures/uganda/t_floor1"] = {64,64};
default_textures["textures/uganda/t_floor2"] = {64,64};
default_textures["textures/uganda/t_floor3"] = {64,64};
default_textures["textures/uganda/t_gmetal_end"] = {64,64};
default_textures["textures/uganda/t_light1"] = {32,32};
default_textures["textures/uganda/t_lower_sidea"] = {64,32};
default_textures["textures/uganda/t_lower_sideb"] = {64,32};
default_textures["textures/uganda/t_lower_tank"] = {64,32};
default_textures["textures/uganda/t_mwooda"] = {128,128};
default_textures["textures/uganda/t_mwoodc"] = {128,128};
default_textures["textures/uganda/t_planks"] = {128,128};
default_textures["textures/uganda/t_planks2"] = {128,128};
default_textures["textures/uganda/t_roof"] = {32,32};
default_textures["textures/uganda/t_sidea"] = {64,128};
default_textures["textures/uganda/t_sideb"] = {64,128};
default_textures["textures/uganda/t_sidebd"] = {64,128};
default_textures["textures/uganda/t_sidec_window"] = {64,128};
default_textures["textures/uganda/t_sided_window"] = {64,128};
default_textures["textures/uganda/t_side_vent"] = {32,32};
default_textures["textures/uganda/t_tankbrace"] = {32,32};
default_textures["textures/uganda/t_tankgeneric"] = {64,64};
default_textures["textures/uganda/t_tankgeneric2"] = {64,64};
default_textures["textures/uganda/t_terrain3"] = {128,128};
default_textures["textures/uganda/t_terrain5"] = {128,128};
default_textures["textures/uganda/t_track_bed"] = {64,128};
default_textures["textures/uganda/t_train_railing"] = {32,32};
default_textures["textures/uganda/t_up_frt_ledge"] = {32,32};
default_textures["textures/uganda/t_wheel"] = {32,32};
default_textures["textures/uganda/t_win_frnt"] = {32,32};
default_textures["textures/uganda/t_win_frnta"] = {32,32};
default_textures["textures/uganda/t_woodroof"] = {64,128};
default_textures["textures/uganda/t_woodwall"] = {128,128};
default_textures["textures/uganda/Water1"] = {128,128};
default_textures["textures/uganda/water2"] = {128,128};
default_textures["textures/uganda/window1"] = {64,64};
default_textures["textures/uganda/window1d"] = {64,64};
default_textures["textures/uganda/window1_dirt"] = {64,64};
default_textures["textures/uganda/window1_dirtd"] = {64,64};
default_textures["textures/uganda/window2"] = {64,64};
default_textures["textures/uganda/window2d"] = {64,64};
default_textures["textures/uganda/window2_dirt"] = {64,64};
default_textures["textures/uganda/window2_dirtd"] = {64,64};


}