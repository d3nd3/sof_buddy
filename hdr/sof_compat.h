

// Include guard
#ifndef SOF_COMPAT_H
#define SOF_COMPAT_H

typedef unsigned char 		byte;
typedef unsigned short 		word;
typedef int					qboolean;

#define MAX_PATH        260      // Windows max filename length
#define MAX_CLIENTS     32

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


extern void ( *orig_Com_Printf)(char * msg, ...);
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

extern void InitDefaults(void);


extern float * level_time; 

#ifndef MAX_STATS
#define MAX_STATS 16
#endif

// Minimal pmove/frame/playerstate copies (shared between core and demo_analyzer)
typedef enum { PM_NORMAL, PM_SPECTATOR, PM_DEAD, PM_GIB, PM_FREEZE } pmtype_t;
typedef struct {
    pmtype_t pm_type;
    short origin[3]; //4
    short velocity[3]; //A
    byte pm_flags; //10
    byte pm_time; //11
    short gravity; //12
    short delta_angles[3]; //14
} pmove_state_t;

typedef struct {
    pmove_state_t    pmove; // + 2 bytes padding
    float            viewangles[3]; //1C,20,24
    float            viewoffset[3]; //28,2C,30
    float            kick_angles[3]; //34,38, 3C
    float            weaponkick_angles[3]; //40,44,48
    float            remote_vieworigin[3]; //4C,50,54
    float            remote_viewangles[3]; //0x58,0x5C,0x60
    int              remote_id; //0x64
    byte             remote_type; //0x68 + 3 byte padding
    void*            gun; // 0x6C
    short            gunUUID; //0x70
    short            gunType; //0x72
    short            gunClip; // 0x74
    short            gunAmmo; //0x76
    byte             gunReload; //0x78
    byte             restart_count; //0x79
    byte             buttons_inhibit; //0x7A (+ 1 byte Padding)
    void*            bod; //0x7C
    short            bodUUID; //0x80 (2 bytes padding here?) this might be Integer
    float            blend[4]; //(0x84,0x88,0x8C,90)
    float            fov; //0x94
    int              rdflags; //0x98
    short            soundID; //0x9C
    byte             musicID; //0x9E
    short            damageLoc; //0xA0
    short            damageDir; //0xA2
    short             stats[MAX_STATS]; //0xA4 ... 0xA4 + 0x20 = 0xC4
    byte             dmRank; //0xC4
    byte             dmRankedPlyrs; //0xC5   
    byte             spectatorId; //0xC6
    byte             cinematicfreeze; //0xC7
} player_state_t;

/* Compile-time layout checks for player_state_t offsets (hex offsets from struct start)
   These asserts validate the annotated offsets in the header match the compiled layout.
   If you build for a different ABI (e.g. 64-bit pointers), update expected offsets accordingly. */
#include <stddef.h>

/* Offset assertions */
#if defined(__cplusplus)
  static_assert(offsetof(player_state_t, pmove) == 0x00, "pmove offset mismatch");
  static_assert(offsetof(player_state_t, viewangles) == 0x1C, "viewangles offset mismatch");
  static_assert(offsetof(player_state_t, viewoffset) == 0x28, "viewoffset offset mismatch");
  static_assert(offsetof(player_state_t, kick_angles) == 0x34, "kick_angles offset mismatch");
  static_assert(offsetof(player_state_t, weaponkick_angles) == 0x40, "weaponkick_angles offset mismatch");
  static_assert(offsetof(player_state_t, remote_vieworigin) == 0x4C, "remote_vieworigin offset mismatch");
  static_assert(offsetof(player_state_t, remote_viewangles) == 0x58, "remote_viewangles offset mismatch");
  static_assert(offsetof(player_state_t, remote_id) == 0x64, "remote_id offset mismatch");
  static_assert(offsetof(player_state_t, remote_type) == 0x68, "remote_type offset mismatch");
  static_assert(offsetof(player_state_t, gun) == 0x6C, "gun offset mismatch");
  static_assert(offsetof(player_state_t, gunUUID) == 0x70, "gunUUID offset mismatch");
  static_assert(offsetof(player_state_t, gunType) == 0x72, "gunType offset mismatch");
  static_assert(offsetof(player_state_t, gunClip) == 0x74, "gunClip offset mismatch");
  static_assert(offsetof(player_state_t, gunAmmo) == 0x76, "gunAmmo offset mismatch");
  static_assert(offsetof(player_state_t, gunReload) == 0x78, "gunReload offset mismatch");
  static_assert(offsetof(player_state_t, restart_count) == 0x79, "restart_count offset mismatch");
  static_assert(offsetof(player_state_t, buttons_inhibit) == 0x7A, "buttons_inhibit offset mismatch");
  static_assert(offsetof(player_state_t, bod) == 0x7C, "bod offset mismatch");
  static_assert(offsetof(player_state_t, bodUUID) == 0x80, "bodUUID offset mismatch");
  static_assert(offsetof(player_state_t, blend) == 0x84, "blend offset mismatch");
  static_assert(offsetof(player_state_t, fov) == 0x94, "fov offset mismatch");
  static_assert(offsetof(player_state_t, rdflags) == 0x98, "rdflags offset mismatch");
  static_assert(offsetof(player_state_t, soundID) == 0x9C, "soundID offset mismatch");
  static_assert(offsetof(player_state_t, musicID) == 0x9E, "musicID offset mismatch");
  static_assert(offsetof(player_state_t, damageLoc) == 0xA0, "damageLoc offset mismatch");
  static_assert(offsetof(player_state_t, damageDir) == 0xA2, "damageDir offset mismatch");
  static_assert(offsetof(player_state_t, stats) == 0xA4, "stats offset mismatch");
  static_assert(offsetof(player_state_t, dmRank) == 0xC4, "dmRank offset mismatch");
  static_assert(offsetof(player_state_t, dmRankedPlyrs) == 0xC5, "dmRankedPlyrs offset mismatch");
  static_assert(offsetof(player_state_t, spectatorId) == 0xC6, "spectatorId offset mismatch");
  static_assert(offsetof(player_state_t, cinematicfreeze) == 0xC7, "cinematicfreeze offset mismatch");
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
  _Static_assert(offsetof(player_state_t, pmove) == 0x00, "pmove offset mismatch");
  _Static_assert(offsetof(player_state_t, viewangles) == 0x1C, "viewangles offset mismatch");
  _Static_assert(offsetof(player_state_t, viewoffset) == 0x28, "viewoffset offset mismatch");
  _Static_assert(offsetof(player_state_t, kick_angles) == 0x34, "kick_angles offset mismatch");
  _Static_assert(offsetof(player_state_t, weaponkick_angles) == 0x40, "weaponkick_angles offset mismatch");
  _Static_assert(offsetof(player_state_t, remote_vieworigin) == 0x4C, "remote_vieworigin offset mismatch");
  _Static_assert(offsetof(player_state_t, remote_viewangles) == 0x58, "remote_viewangles offset mismatch");
  _Static_assert(offsetof(player_state_t, remote_id) == 0x64, "remote_id offset mismatch");
  _Static_assert(offsetof(player_state_t, remote_type) == 0x68, "remote_type offset mismatch");
  _Static_assert(offsetof(player_state_t, gun) == 0x6C, "gun offset mismatch");
  _Static_assert(offsetof(player_state_t, gunUUID) == 0x70, "gunUUID offset mismatch");
  _Static_assert(offsetof(player_state_t, gunType) == 0x72, "gunType offset mismatch");
  _Static_assert(offsetof(player_state_t, gunClip) == 0x74, "gunClip offset mismatch");
  _Static_assert(offsetof(player_state_t, gunAmmo) == 0x76, "gunAmmo offset mismatch");
  _Static_assert(offsetof(player_state_t, gunReload) == 0x78, "gunReload offset mismatch");
  _Static_assert(offsetof(player_state_t, restart_count) == 0x79, "restart_count offset mismatch");
  _Static_assert(offsetof(player_state_t, buttons_inhibit) == 0x7A, "buttons_inhibit offset mismatch");
  _Static_assert(offsetof(player_state_t, bod) == 0x7C, "bod offset mismatch");
  _Static_assert(offsetof(player_state_t, bodUUID) == 0x80, "bodUUID offset mismatch");
  _Static_assert(offsetof(player_state_t, blend) == 0x84, "blend offset mismatch");
  _Static_assert(offsetof(player_state_t, fov) == 0x94, "fov offset mismatch");
  _Static_assert(offsetof(player_state_t, rdflags) == 0x98, "rdflags offset mismatch");
  _Static_assert(offsetof(player_state_t, soundID) == 0x9C, "soundID offset mismatch");
  _Static_assert(offsetof(player_state_t, musicID) == 0x9E, "musicID offset mismatch");
  _Static_assert(offsetof(player_state_t, damageLoc) == 0xA0, "damageLoc offset mismatch");
  _Static_assert(offsetof(player_state_t, damageDir) == 0xA2, "damageDir offset mismatch");
  _Static_assert(offsetof(player_state_t, stats) == 0xA4, "stats offset mismatch");
  _Static_assert(offsetof(player_state_t, dmRank) == 0xC4, "dmRank offset mismatch");
  _Static_assert(offsetof(player_state_t, dmRankedPlyrs) == 0xC5, "dmRankedPlyrs offset mismatch");
  _Static_assert(offsetof(player_state_t, spectatorId) == 0xC6, "spectatorId offset mismatch");
  _Static_assert(offsetof(player_state_t, cinematicfreeze) == 0xC7, "cinematicfreeze offset mismatch");
#else
  /* Fallback - will produce compile-time error if any offset mismatches */
  typedef char assert_pmove_offset[(offsetof(player_state_t, pmove) == 0x00) ? 1 : -1];
  typedef char assert_viewangles_offset[(offsetof(player_state_t, viewangles) == 0x1C) ? 1 : -1];
  typedef char assert_viewoffset_offset[(offsetof(player_state_t, viewoffset) == 0x28) ? 1 : -1];
  typedef char assert_kick_angles_offset[(offsetof(player_state_t, kick_angles) == 0x34) ? 1 : -1];
  typedef char assert_weaponkick_angles_offset[(offsetof(player_state_t, weaponkick_angles) == 0x40) ? 1 : -1];
  typedef char assert_remote_vieworigin_offset[(offsetof(player_state_t, remote_vieworigin) == 0x4C) ? 1 : -1];
  typedef char assert_remote_viewangles_offset[(offsetof(player_state_t, remote_viewangles) == 0x58) ? 1 : -1];
  typedef char assert_remote_id_offset[(offsetof(player_state_t, remote_id) == 0x64) ? 1 : -1];
  typedef char assert_remote_type_offset[(offsetof(player_state_t, remote_type) == 0x68) ? 1 : -1];
  typedef char assert_gun_offset[(offsetof(player_state_t, gun) == 0x6C) ? 1 : -1];
  typedef char assert_gunUUID_offset[(offsetof(player_state_t, gunUUID) == 0x70) ? 1 : -1];
  typedef char assert_gunType_offset[(offsetof(player_state_t, gunType) == 0x72) ? 1 : -1];
  typedef char assert_gunClip_offset[(offsetof(player_state_t, gunClip) == 0x74) ? 1 : -1];
  typedef char assert_gunAmmo_offset[(offsetof(player_state_t, gunAmmo) == 0x76) ? 1 : -1];
  typedef char assert_gunReload_offset[(offsetof(player_state_t, gunReload) == 0x78) ? 1 : -1];
  typedef char assert_restart_count_offset[(offsetof(player_state_t, restart_count) == 0x79) ? 1 : -1];
  typedef char assert_buttons_inhibit_offset[(offsetof(player_state_t, buttons_inhibit) == 0x7A) ? 1 : -1];
  typedef char assert_bod_offset[(offsetof(player_state_t, bod) == 0x7C) ? 1 : -1];
  typedef char assert_bodUUID_offset[(offsetof(player_state_t, bodUUID) == 0x80) ? 1 : -1];
  typedef char assert_blend_offset[(offsetof(player_state_t, blend) == 0x84) ? 1 : -1];
  typedef char assert_fov_offset[(offsetof(player_state_t, fov) == 0x94) ? 1 : -1];
  typedef char assert_rdflags_offset[(offsetof(player_state_t, rdflags) == 0x98) ? 1 : -1];
  typedef char assert_soundID_offset[(offsetof(player_state_t, soundID) == 0x9C) ? 1 : -1];
  typedef char assert_musicID_offset[(offsetof(player_state_t, musicID) == 0x9E) ? 1 : -1];
  typedef char assert_damageLoc_offset[(offsetof(player_state_t, damageLoc) == 0xA0) ? 1 : -1];
  typedef char assert_damageDir_offset[(offsetof(player_state_t, damageDir) == 0xA2) ? 1 : -1];
  typedef char assert_stats_offset[(offsetof(player_state_t, stats) == 0xA4) ? 1 : -1];
  typedef char assert_dmRank_offset[(offsetof(player_state_t, dmRank) == 0xC4) ? 1 : -1];
  typedef char assert_dmRankedPlyrs_offset[(offsetof(player_state_t, dmRankedPlyrs) == 0xC5) ? 1 : -1];
  typedef char assert_spectatorId_offset[(offsetof(player_state_t, spectatorId) == 0xC6) ? 1 : -1];
  typedef char assert_cinematicfreeze_offset[(offsetof(player_state_t, cinematicfreeze) == 0xC7) ? 1 : -1];
#endif

typedef unsigned int fakebyte;
typedef struct {
    fakebyte valid;
    int serverframe;
    int servertime;
    int deltaframe;
    float ghoultime;
    byte areabits[256/8];
    player_state_t playerstate;
    int num_entities;
    int parse_entities;
} frame_t;

// Flags
#define STAT_FLAG_SCOPE					0x0001
#define STAT_FLAG_INFOTICKER			0x0002
#define STAT_FLAG_WIND					0x0004
#define STAT_FLAG_MISSIONFAIL			0x0008
#define STAT_FLAG_HIT					0x0010
#define STAT_FLAG_TEAM					0x0020
#define STAT_FLAG_COUNTDOWN				0x0040
#define STAT_FLAG_MISSIONACC			0x0080
#define STAT_FLAG_MISSIONEXIT			0x0100
#define STAT_FLAG_SHOW_STEALTH			0x0200
#define STAT_FLAG_OBJECTIVES			0x0400
#define STAT_FLAG_HIDECROSSHAIR			0x0800

typedef enum
{
	SFW_EMPTYSLOT = 0,
	SFW_KNIFE,
	SFW_PISTOL2,
	SFW_PISTOL1,
	SFW_MACHINEPISTOL,
	SFW_ASSAULTRIFLE,
	SFW_SNIPER,
	SFW_AUTOSHOTGUN,
	SFW_SHOTGUN,
	SFW_MACHINEGUN,
	SFW_ROCKET,
	SFW_MICROWAVEPULSE,
	SFW_FLAMEGUN,
	SFW_HURTHAND,
	SFW_THROWHAND,
	SFW_NUM_WEAPONS
} weapons_t;

typedef enum
{
	SFE_EMPTYSLOT = 0,
	SFE_FLASHPACK,
//	SFE_NEURAL_GRENADE,
	SFE_C4,
	SFE_LIGHT_GOGGLES,
	SFE_CLAYMORE,
	SFE_MEDKIT,
	SFE_GRENADE,
	SFE_CTFFLAG,
	SFE_NUMITEMS
} equipment_t;

// gunType values for player_state_t.gunType
typedef enum {
    GUN_NONE = 0,
    GUN_KNIFE,
    GUN_PISTOL2,
    GUN_PISTOL1,
    GUN_MACHINEPISTOL,
    GUN_ASSAULTRIFLE,
    GUN_SNIPER,
    GUN_SLUGGER,
    GUN_SHOTGUN,
    GUN_MACHINEGUN,
    GUN_ROCKET,
    GUN_MPG,
    GUN_FLAMEGUN,
    GUN_NUM_TYPES
} gun_type_t;

// player_state->stats[] indexes
#define STAT_INV_TYPE			0	// cleared each frame
#define	STAT_HEALTH				1
#define	STAT_CLIP_AMMO			2
#define	STAT_AMMO				3
#define	STAT_CLIP_MAX			4
#define	STAT_ARMOR				5
#define	STAT_WEAPON				6
#define	STAT_INV_COUNT			7	// # of current inventory item
#define	STAT_FLAGS				8
#define	STAT_LAYOUTS			9
#define	STAT_FRAGS				10
#define STAT_STEALTH			11
#define STAT_FORCEHUD			12	// draw the HUD regardless of current weapon (tutorial level)

// UNUSED I BELIEVE
#define STAT_CASH2				12	// cash in bank
#define STAT_CASH3				13	// cash to be awarded upon completion of mission
#define STAT_DAMAGELOC			14	// last damage done to player
#define STAT_DAMAGEDIR			15	// direction of the damage bracket on interface

// Bit flags for client damage location
#define CLDAM_HEAD		0x00000001
#define CLDAM_L_ARM		0x00000002
#define CLDAM_L_LEG		0x00000004
#define CLDAM_R_ARM		0x00000008
#define CLDAM_R_LEG		0x00000010
#define CLDAM_TORSO		0x00000020

// Index for client damage locations
#define CL_HEAD			0
#define CL_L_ARM		1
#define CL_L_LEG		2
#define CL_R_ARM		3
#define CL_R_LEG		4
#define CL_TORSO		5
#define MAX_CLDAMAGE	6

// Bit flags for client damage brackets
#define CLDAM_T_BRACKET		0x00000001	// Top bracket
#define CLDAM_B_BRACKET		0x00000002	// Bottom bracket
#define CLDAM_R_BRACKET		0x00000004	// Right bracket
#define CLDAM_L_BRACKET		0x00000008	// Left bracket
#define CLDAM_ALL_BRACKET	0x00000010	// All brackets

// Layout BITS
//Show scoreboard when dead, or intermission or user requested score or help/objective
#define LAYOUT_SCOREBOARD 1
//active in Deathmatch mode only
#define LAYOUT_DMRANKING 2

#define	PMF_DUCKED			1
#define	PMF_JUMP_HELD		2
#define	PMF_ON_GROUND		4
#define	PMF_TIME_WATERJUMP	8	// pm_time is waterjump
#define	PMF_TIME_LAND		16	// pm_time is time before rejump
#define	PMF_TIME_TELEPORT	32	// pm_time is non-moving time
#define	PMF_FATIGUED		64	// player is fatigued and cannot run
#define PMF_NO_PREDICTION	128	// temporarily disables prediction (used for grappling hook)

// player_state_t->rdflags  (refdef flags)
#define	RDF_UNDERWATER		0x01		// warp the screen as apropriate
#define RDF_NOWORLDMODEL	0x02		// used for player configuration screen
#define RDF_GOGGLES			0x04		// Low light goggles.

//for player_state_t->remote_type
#define REMOTE_TYPE_FPS			0x00		// normal fps view -- WOW, it sucks that this is zero...
#define REMOTE_TYPE_TPS			0x01		// third person view
#define REMOTE_TYPE_LETTERBOX	0x02		// show's black boxes above and below (letterbox)
#define REMOTE_TYPE_CAMERA		0x04		// displays a camera graphic border
#define REMOTE_TYPE_NORMAL		0x08		// remote view, nothing fancy
#define REMOTE_TYPE_SNIPER		0x10		// sniper scope


typedef struct aEffectInfo_s
{
	short	effectId;
	short	bolt;//actually a ghoul ID
	short	size;
}aEffectInfo_t;

#define NUM_EFFECTS 4

typedef struct entity_state_s
{
	int			number;			// edict index

	float		origin[3];//4
	float		angles[3];//10
	float		angle_diff;//1C
	int			modelindex;//20
	int			renderindex;//24
	int			frame;//28
	int			skinnum;//2C
	int			effects;//30
	int			renderfx;//34
	int			solid;			// 38 For client side prediction, (bits 0-7) is x/y radius
								// (bits 8-15) is z down distance, (bits16-23) is z up.
								// The msb is unused (and unsent).
								// gi.linkentity sets this properly
	int			sound;			// 3C for looping sounds, to guarantee shutoff
	int			sound_data;		// 40 data for volume on looping sound, as well as attenuation
	int			event;			// 44impulse events -- muzzle flashes, footsteps, etc
	int			event2;			// 48 events only go out for a single frame, they
	int			event3;			// 4C are automatically cleared each frame - 3 just in case...
	int			data;			// 50 data for the event FX
	int			data2;			// 54 most of the time, these are
	int			data3;			// 58 empty

	aEffectInfo_t	effectData[NUM_EFFECTS]; //0x5C
} entity_state_t;

/* Compile-time asserts for entity_state_t offsets */
#include <stddef.h>

#if defined(__cplusplus)
  static_assert(offsetof(entity_state_t, number) == 0x00, "entity_state_t.number offset mismatch");
  static_assert(offsetof(entity_state_t, origin) == 0x04, "entity_state_t.origin offset mismatch");
  static_assert(offsetof(entity_state_t, angles) == 0x10, "entity_state_t.angles offset mismatch");
  static_assert(offsetof(entity_state_t, angle_diff) == 0x1C, "entity_state_t.angle_diff offset mismatch");
  static_assert(offsetof(entity_state_t, modelindex) == 0x20, "entity_state_t.modelindex offset mismatch");
  static_assert(offsetof(entity_state_t, renderindex) == 0x24, "entity_state_t.renderindex offset mismatch");
  static_assert(offsetof(entity_state_t, frame) == 0x28, "entity_state_t.frame offset mismatch");
  static_assert(offsetof(entity_state_t, skinnum) == 0x2C, "entity_state_t.skinnum offset mismatch");
  static_assert(offsetof(entity_state_t, effects) == 0x30, "entity_state_t.effects offset mismatch");
  static_assert(offsetof(entity_state_t, renderfx) == 0x34, "entity_state_t.renderfx offset mismatch");
  static_assert(offsetof(entity_state_t, solid) == 0x38, "entity_state_t.solid offset mismatch");
  static_assert(offsetof(entity_state_t, sound) == 0x3C, "entity_state_t.sound offset mismatch");
  static_assert(offsetof(entity_state_t, sound_data) == 0x40, "entity_state_t.sound_data offset mismatch");
  static_assert(offsetof(entity_state_t, event) == 0x44, "entity_state_t.event offset mismatch");
  static_assert(offsetof(entity_state_t, event2) == 0x48, "entity_state_t.event2 offset mismatch");
  static_assert(offsetof(entity_state_t, event3) == 0x4C, "entity_state_t.event3 offset mismatch");
  static_assert(offsetof(entity_state_t, data) == 0x50, "entity_state_t.data offset mismatch");
  static_assert(offsetof(entity_state_t, data2) == 0x54, "entity_state_t.data2 offset mismatch");
  static_assert(offsetof(entity_state_t, data3) == 0x58, "entity_state_t.data3 offset mismatch");
  static_assert(offsetof(entity_state_t, effectData) == 0x5C, "entity_state_t.effectData offset mismatch");
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
  _Static_assert(offsetof(entity_state_t, number) == 0x00, "entity_state_t.number offset mismatch");
  _Static_assert(offsetof(entity_state_t, origin) == 0x04, "entity_state_t.origin offset mismatch");
  _Static_assert(offsetof(entity_state_t, angles) == 0x10, "entity_state_t.angles offset mismatch");
  _Static_assert(offsetof(entity_state_t, angle_diff) == 0x1C, "entity_state_t.angle_diff offset mismatch");
  _Static_assert(offsetof(entity_state_t, modelindex) == 0x20, "entity_state_t.modelindex offset mismatch");
  _Static_assert(offsetof(entity_state_t, renderindex) == 0x24, "entity_state_t.renderindex offset mismatch");
  _Static_assert(offsetof(entity_state_t, frame) == 0x28, "entity_state_t.frame offset mismatch");
  _Static_assert(offsetof(entity_state_t, skinnum) == 0x2C, "entity_state_t.skinnum offset mismatch");
  _Static_assert(offsetof(entity_state_t, effects) == 0x30, "entity_state_t.effects offset mismatch");
  _Static_assert(offsetof(entity_state_t, renderfx) == 0x34, "entity_state_t.renderfx offset mismatch");
  _Static_assert(offsetof(entity_state_t, solid) == 0x38, "entity_state_t.solid offset mismatch");
  _Static_assert(offsetof(entity_state_t, sound) == 0x3C, "entity_state_t.sound offset mismatch");
  _Static_assert(offsetof(entity_state_t, sound_data) == 0x40, "entity_state_t.sound_data offset mismatch");
  _Static_assert(offsetof(entity_state_t, event) == 0x44, "entity_state_t.event offset mismatch");
  _Static_assert(offsetof(entity_state_t, event2) == 0x48, "entity_state_t.event2 offset mismatch");
  _Static_assert(offsetof(entity_state_t, event3) == 0x4C, "entity_state_t.event3 offset mismatch");
  _Static_assert(offsetof(entity_state_t, data) == 0x50, "entity_state_t.data offset mismatch");
  _Static_assert(offsetof(entity_state_t, data2) == 0x54, "entity_state_t.data2 offset mismatch");
  _Static_assert(offsetof(entity_state_t, data3) == 0x58, "entity_state_t.data3 offset mismatch");
  _Static_assert(offsetof(entity_state_t, effectData) == 0x5C, "entity_state_t.effectData offset mismatch");
#else
  typedef char assert_entity_number[(offsetof(entity_state_t, number) == 0x00) ? 1 : -1];
  typedef char assert_entity_origin[(offsetof(entity_state_t, origin) == 0x04) ? 1 : -1];
  typedef char assert_entity_angles[(offsetof(entity_state_t, angles) == 0x10) ? 1 : -1];
  typedef char assert_entity_angle_diff[(offsetof(entity_state_t, angle_diff) == 0x1C) ? 1 : -1];
  typedef char assert_entity_modelindex[(offsetof(entity_state_t, modelindex) == 0x20) ? 1 : -1];
  typedef char assert_entity_renderindex[(offsetof(entity_state_t, renderindex) == 0x24) ? 1 : -1];
  typedef char assert_entity_frame[(offsetof(entity_state_t, frame) == 0x28) ? 1 : -1];
  typedef char assert_entity_skinnum[(offsetof(entity_state_t, skinnum) == 0x2C) ? 1 : -1];
  typedef char assert_entity_effects[(offsetof(entity_state_t, effects) == 0x30) ? 1 : -1];
  typedef char assert_entity_renderfx[(offsetof(entity_state_t, renderfx) == 0x34) ? 1 : -1];
  typedef char assert_entity_solid[(offsetof(entity_state_t, solid) == 0x38) ? 1 : -1];
  typedef char assert_entity_sound[(offsetof(entity_state_t, sound) == 0x3C) ? 1 : -1];
  typedef char assert_entity_sound_data[(offsetof(entity_state_t, sound_data) == 0x40) ? 1 : -1];
  typedef char assert_entity_event[(offsetof(entity_state_t, event) == 0x44) ? 1 : -1];
  typedef char assert_entity_event2[(offsetof(entity_state_t, event2) == 0x48) ? 1 : -1];
  typedef char assert_entity_event3[(offsetof(entity_state_t, event3) == 0x4C) ? 1 : -1];
  typedef char assert_entity_data[(offsetof(entity_state_t, data) == 0x50) ? 1 : -1];
  typedef char assert_entity_data2[(offsetof(entity_state_t, data2) == 0x54) ? 1 : -1];
  typedef char assert_entity_data3[(offsetof(entity_state_t, data3) == 0x58) ? 1 : -1];
  typedef char assert_entity_effectData[(offsetof(entity_state_t, effectData) == 0x5C) ? 1 : -1];
#endif




typedef struct
{
	entity_state_t	baseline;		// delta from this if not from a previous frame
	entity_state_t	current; //0x74
	entity_state_t	prev;			// 0xE8will always be valid, but might just be a copy of current

   //0x30 - 0x18 = 0x18 missing.
	int			serverframe;		// 0x15C if not current, this ent isn't in the frame
   float		lerp_origin[3];		// for trails (variable hz)

	int			trailcount;			// for diminishing grenade trails
	
	int			fly_stoptime;
} centity_t;

#endif // SOF_COMPAT_H