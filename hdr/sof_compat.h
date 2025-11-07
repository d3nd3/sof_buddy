// compatibility types and engine interfaces
#ifndef SOF_COMPAT_H
#define SOF_COMPAT_H

#ifdef _WIN32
#if _WIN32_WINNT < 0x0600
#include <stdint.h>
#ifndef ULONGLONG
typedef uint64_t ULONGLONG;
#endif
ULONGLONG GetTickCount64Compat(void);
#define GetTickCount64 GetTickCount64Compat
#endif
#endif

typedef unsigned char 		byte;
typedef unsigned short 		word;
typedef int					qboolean;

#define MAX_PATH        260      // Windows max filename length

typedef struct cvar_s cvar_t;
typedef void (*cvarcommand_t) (cvar_t *cvar);

typedef struct cvar_s
{
   char        *name;
   char        *string;
   char        *latched_string;  // for CVAR_LATCH vars
   int            flags;
   cvarcommand_t  command;
   qboolean    modified;   // set each time the cvar is changed
   float       value;
   struct cvar_s *   next;
} cvar_t;

typedef struct paletteRGB_s
{
   struct
   {
      byte r,g,b;
   };
} paletteRGB_t;

typedef struct paletteRGBA_s
{
   union
   {
      struct
      {
         byte r,g,b,a;
      };
      unsigned c;
      byte c_array[4];
   };
} paletteRGBA_t;


extern cvar_t *(*orig_Cvar_Get)(const char * name, const char * value, int flags, cvarcommand_t command);
extern cvar_t *(*orig_Cvar_Set2) (char *var_name, char *value, qboolean force);
extern void (*orig_Cvar_SetInternal)(bool active); 

#define  CVAR_ARCHIVE   0x00000001  // set to cause it to be saved to config.cfg

//shared between client/server over network.
#define  CVAR_USERINFO  0x00000002  // added to userinfo  when changed
#define  CVAR_SERVERINFO   0x00000004  // added to serverinfo when changed

//apply immidiately / delayed impact
#define  CVAR_LATCH     0x00000010  // save changes until server restart

//permissions
#define CVAR_INTERNAL   0x00000080  // can only be set internally by the code, never by the user in any way directly

//modification
#define  CVAR_NOSET     0x00000008  // don't allow change from console at all, but can be set from the command line
#define  CVAR_INT    0x00000020  // value is locked to integral
#define CVAR_PLAIN      0x00000040  // can't increment, decrement, or toggle this guy


//menu cvars
#define CVAR_WEAPON     0x00000100  // this cvar defines possession of a weapon
#define CVAR_ITEM    0x00000200  //  "   "      "       "     "  " item
#define CVAR_AMMO    0x00000400  //  "   "      "       "     "  " quantity of ammo
#define CVAR_MISC    0x00000800
#define CVAR_PARENTAL   0x00001000  // this cvar is stored in the registry through special means

#define CVAR_MENU_MASK  (CVAR_WEAPON | CVAR_ITEM | CVAR_AMMO | CVAR_MISC)
#define CVAR_INFO    (CVAR_MENU_MASK | CVAR_USERINFO | CVAR_SERVERINFO)


extern void ( *orig_Com_Printf)(const char * msg, ...);
extern void (*orig_Qcommon_Frame) (int msec);
extern void (*orig_Qcommon_Init) (int argc, char **argv);
extern qboolean (*orig_Cbuf_AddLateCommands)(void);

//vid_ref access
extern qboolean (*orig_VID_LoadRefresh)( char *name );
extern void (*orig_GL_BuildPolygonFromSurface)(void *fa);
extern int (*orig_R_Init)( void *hinstance, void *hWnd, void * unknown );
extern void (*orig_drawTeamIcons)(void * param1,void * param2,void * param3,void * param4);

//util access
extern void (*orig_Cmd_ExecuteString)(const char * string);

// Renderer CVars (for texture filtering features)
extern cvar_t* _gl_texturemode;  // OpenGL texture mode cvar

extern void InitDefaults(void);

#endif // SOF_COMPAT_H


