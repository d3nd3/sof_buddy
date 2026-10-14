// compatibility types and engine interfaces
#ifndef SOF_COMPAT_H
#define SOF_COMPAT_H

typedef unsigned char 		byte;
typedef unsigned short 		word;
typedef int					qboolean;

#define MAX_PATH        260      // Windows max filename length

typedef struct cvar_s cvar_t;
typedef void (*cvarcommand_t) (cvar_t *cvar);
typedef void (*xcommand_t)(void);

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


typedef enum {
	ca_uninitialized,			// 0-default *ca_uninitialized* BEFORE CL_InitLocal()
	ca_disconnected,			// 1-Not connected  *ca_disconnected*
	ca_connecting,				// 2-Attempting to connect to a server  *ca_connecting* ca_earlyconnecting (reconnect_f or connect_f)
	ca_won_receive_result,		// 3-Loki WON ReceiveResult CL_HandleChallenge()
	ca_connecting_stage2,		// 4-(Re)connecting to a server  *ca_connecting_stage2* ( CL_CheckForResend())
	ca_won_challenge,			// 5- Loki WON Challenge CL_HandleChallenge()
	ca_won_receive_challenge1,	// 6- Loki WON ReceiveChallenge1 CL_HandleChallenge()
	ca_connected,				// 7-Loading map  *ca_connected*client_connect CL_ConnectionlessPacket()
	ca_active					// 8-Spawned into the map *ca_active*
} connstate_t;

// Preferred style: use generated detour pointers directly (detour_*::o*).
namespace detour_Cvar_Get { extern cvar_t* (__cdecl *oCvar_Get)(const char*, const char*, int, cvarcommand_t); }
namespace detour_Cvar_Set2 { extern cvar_t* (__cdecl *oCvar_Set2)(char*, char*, qboolean); }
namespace detour_Cvar_SetInternal { extern void (__cdecl *oCvar_SetInternal)(bool); }

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

// sof_buddy-local persistence marker (saved to base/sofbuddy.cfg by sof_buddy code)
#define CVAR_SOFBUDDY_ARCHIVE 0x40000000


namespace detour_Com_Printf { extern void (__cdecl *oCom_Printf)(const char*, ...); }
namespace detour_Qcommon_Frame { extern void (__cdecl *oQcommon_Frame)(int); }
namespace detour_Qcommon_Init { extern void (__cdecl *oQcommon_Init)(int, char**); }
namespace detour_Com_DPrintf { extern void (__cdecl *oCom_DPrintf)(const char*, ...); }
extern qboolean (*orig_Cbuf_AddLateCommands)(void);

//vid_ref access
extern qboolean (*orig_VID_LoadRefresh)( char *name );
extern void (*orig_GL_BuildPolygonFromSurface)(void *fa);
extern int (*orig_R_Init)( void *hinstance, void *hWnd, void * unknown );
extern void (*orig_drawTeamIcons)(void * param1,void * param2,void * param3,void * param4);

//util access
namespace detour_Cmd_ExecuteString { extern void (__cdecl *oCmd_ExecuteString)(const char*); }
namespace detour_Z_Free { extern void (__cdecl *oZ_Free)(void*); }
namespace detour_CopyString { extern char* (__cdecl *oCopyString)(char*); }
namespace detour_Cmd_AddCommand { extern void (__cdecl *oCmd_AddCommand)(char*, xcommand_t); }
namespace detour_Cmd_Argc { extern int (__cdecl *oCmd_Argc)(); }
namespace detour_Cmd_Argv { extern char* (__cdecl *oCmd_Argv)(int); }

// Renderer CVars (for texture filtering features)
extern cvar_t* _gl_texturemode;  // OpenGL texture mode cvar

extern void InitDefaults(void);

#endif // SOF_COMPAT_H

