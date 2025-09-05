#include <windows.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "sof_compat.h"
#include "features.h"

#include "ighoul.h"

#include "../DetourXS/detourxs.h"

#include "util.h"

/*
  Demo analyzer overlay:
  - Captures team icon draw calls to infer on-screen position of each player icon
  - Estimates horizontal yaw delta from center using fovX and screen position
  - Draws a similarity score near each icon (cosine of yaw delta)

  Notes:
  - We piggyback on the existing glVertex2f detour (scaled_font.cpp) by exposing
    demoAnalyzer_onVertex2f(), which is a cheap no-op unless we're inside a
    team icon draw.
  - We hook via my_drawTeamIcons in ref_fixes.cpp to delineate begin/end of a
    team icon draw for each player.
*/

#define DEG2RAD(x) ((x) * 0.01745329251994329576f)

// Shot flag global (C linkage)
extern "C" int g_da_shotFiredThisFrame = 0;

// Cvar toggle
static cvar_t * _sofbuddy_demo_analyzer = NULL;
static cvar_t * _sofbuddy_da_mode = NULL;        // 0=dot, 1=angle
static cvar_t * _sofbuddy_da_thresh_deg = NULL;  // threshold degrees
static cvar_t * _sofbuddy_da_shot_only = NULL;   // 0/1
static cvar_t * _sofbuddy_da_pre_ms = NULL;      // window before shot
static cvar_t * _sofbuddy_da_post_ms = NULL;     // window after shot
static cvar_t * _sofbuddy_da_log = NULL;         // 0/1
static cvar_t * _sofbuddy_da_hist_bin_deg = NULL;// histogram bin size
static cvar_t * _sofbuddy_da_csv = NULL;         // output csv path

// Per-frame cvars (lazily created)
static cvar_t * frame_valid_cvar = NULL;
static cvar_t * frame_serverframe_cvar = NULL;
static cvar_t * frame_servertime_cvar = NULL;
static cvar_t * frame_deltaframe_cvar = NULL;
static cvar_t * frame_ghoultime_cvar = NULL;
static cvar_t * frame_num_entities_cvar = NULL;

// playerstate / pmove
static cvar_t * frame_playerstate_pmove_pm_type_cvar = NULL;
static cvar_t * frame_playerstate_pmove_origin0_cvar = NULL;
static cvar_t * frame_playerstate_pmove_origin1_cvar = NULL;
static cvar_t * frame_playerstate_pmove_origin2_cvar = NULL;
static cvar_t * frame_playerstate_pmove_velocity0_cvar = NULL;
static cvar_t * frame_playerstate_pmove_velocity1_cvar = NULL;
static cvar_t * frame_playerstate_pmove_velocity2_cvar = NULL;
static cvar_t * frame_playerstate_pmove_pm_flags_cvar = NULL;
static cvar_t * frame_playerstate_pmove_pm_time_cvar = NULL;
static cvar_t * frame_playerstate_pmove_gravity_cvar = NULL;
static cvar_t * frame_playerstate_pmove_delta0_cvar = NULL;
static cvar_t * frame_playerstate_pmove_delta1_cvar = NULL;
static cvar_t * frame_playerstate_pmove_delta2_cvar = NULL;

static cvar_t * frame_playerstate_viewangles0_cvar = NULL;
static cvar_t * frame_playerstate_viewangles1_cvar = NULL;
static cvar_t * frame_playerstate_viewangles2_cvar = NULL;
static cvar_t * frame_playerstate_viewoffset0_cvar = NULL;
static cvar_t * frame_playerstate_viewoffset1_cvar = NULL;
static cvar_t * frame_playerstate_viewoffset2_cvar = NULL;

static cvar_t * frame_playerstate_remote_id_cvar = NULL;
static cvar_t * frame_playerstate_remote_type_cvar = NULL;

static cvar_t * frame_playerstate_gunptr_cvar = NULL;
static cvar_t * frame_playerstate_gunUUID_cvar = NULL;
static cvar_t * frame_playerstate_gunType_cvar = NULL;
static cvar_t * frame_playerstate_gunClip_cvar = NULL;
static cvar_t * frame_playerstate_gunAmmo_cvar = NULL;

static cvar_t * frame_playerstate_bodptr_cvar = NULL;
static cvar_t * frame_playerstate_bodUUID_cvar = NULL;

static cvar_t * frame_playerstate_fov_cvar = NULL;
static cvar_t * frame_playerstate_rdflags_cvar = NULL;
static cvar_t * frame_playerstate_soundID_cvar = NULL;
static cvar_t * frame_playerstate_musicID_cvar = NULL;
static cvar_t * frame_playerstate_damageLoc_cvar = NULL;
static cvar_t * frame_playerstate_damageDir_cvar = NULL;

static cvar_t * frame_playerstate_dmRank_cvar = NULL;
static cvar_t * frame_playerstate_dmRankedPlyrs_cvar = NULL;
static cvar_t * frame_playerstate_spectatorId_cvar = NULL;
static cvar_t * frame_playerstate_cinematicfreeze_cvar = NULL;

// helper to lazily create cvars
static cvar_t * ensure_cvar(const char *name, cvar_t **slot)
{
    if (*slot) return *slot;
    if (!orig_Cvar_Get) return NULL;
    *slot = orig_Cvar_Get(name, "0", CVAR_INTERNAL, NULL);
    return *slot;
}

// External state/functions we reuse
extern int current_vid_w;
extern int current_vid_h;

// Option A: original Draw_String_Color (may expect old std string/palette)
extern void ( * orig_Draw_String_Color)(int, int, char const *, int, int);
// Option B: raw glyph drawing, safe in all contexts
static void draw_string_raw(int x, int y, const char * text, int colorIdx);

// Ref fov_x address observed in ref_fixes.cpp usage
static float * const ref_fov_x = (float*)0x3008FCFC; // degrees
static short * const scurrent_gun = (short*)0x201E7F06; // 8 == shotgun

// ======================================================================================
// Client frame/playerstate access for spectate gun IGhoulInst* and spectatorId
// ======================================================================================

// Minimal Quake2 pmove_state_t used by player_state_t
typedef enum {
    PM_NORMAL,
    PM_SPECTATOR,
    PM_DEAD,
    PM_GIB,
    PM_FREEZE
} pmtype_t;

typedef struct
{
    pmtype_t pm_type;
    short    origin[3];
    short    velocity[3];
    byte     pm_flags;
    byte     pm_time;
    short    gravity;
    short    delta_angles[3];
} pmove_state_t;

#ifndef MAX_STATS
#define MAX_STATS 32
#endif

#ifndef MAX_MAP_AREAS
#define MAX_MAP_AREAS 256
#endif

typedef struct
{
    pmove_state_t    pmove;             // for prediction 0x4
    vec3_t           viewangles;        // for fixed views 0x10
    vec3_t           viewoffset;        // add to pmovestate->origin 0x1C
    vec3_t           kick_angles;       // add to view direction to get render angles 0x28
    vec3_t           weaponkick_angles; // add to view direction to get render angles 0x34
    vec3_t           remote_vieworigin; //0x40
    vec3_t           remote_viewangles; //0x4C
    int              remote_id; //0x58
    byte             remote_type;
    IGhoulInst*      gun;               // view weapon instance 6C
    short            gunUUID;
    short            gunType;
    short            gunClip;
    short            gunAmmo;
    byte             gunReload;
    byte             restart_count;
    byte             buttons_inhibit;
    IGhoulInst*      bod;
    short            bodUUID;
    float            blend[4];
    float            fov; //94
    int              rdflags; //98
    short            soundID; //9C
    byte             musicID; //9E
    short            damageLoc; //A0
    short            damageDir; //A2
    byte             stats[MAX_STATS]; //mb changing this from short to byte fixed it.
    byte             dmRank; //C4 in-game: 48, here: 80, 32 too many
    byte             dmRankedPlyrs; //C5
    byte             spectatorId;       // Index [1-(MAXCLIENTS-1)] //C6
    byte             cinematicfreeze; //C7
} player_state_t;
//actual player_state_t size 200
//compiler thinks its 232
typedef unsigned int fakebyte;
typedef struct frame_s
{
    fakebyte            valid; //byte
    int             serverframe; //0x4
    int             servertime; //0x8
    int             deltaframe; //0xC
    float           ghoultime; //0x10
    byte            areabits[MAX_MAP_AREAS/8]; //0x10 0x14 0x18 0x1C 0x20 0x24 0x28 0x2C
    
    player_state_t  playerstate; //0x34
    int             num_entities;
    int             parse_entities;
} frame_t;

static frame_t * const cl_frame = (frame_t*)0x201E7E60;

// ======================================================================================
// Ghoul note callback for weapon "fire" events
// ======================================================================================

class DA_GunFireCallback : public IGhoulCallBack
{
public:
    virtual bool Execute(IGhoulInst * /*me*/, void * /*user*/, float /*now*/, const void * /*data*/)
    {
        byte spec = 0;
        int serverframe = -1;
        int num_entities = -1;
        if (cl_frame) {
            spec = cl_frame->playerstate.spectatorId;
            serverframe = cl_frame->serverframe;
            num_entities = cl_frame->num_entities;
        }
        if (orig_Com_Printf) {
            orig_Com_Printf("[DA] gun fire note (spec %u) serverframe=%d num_entities=%d\n", (unsigned)spec, serverframe, num_entities);
        }
        return true;
    }
};

static DA_GunFireCallback g_daGunFireCb;
static IGhoulInst * g_lastRegisteredGun = NULL;
static byte g_lastSpectatorId = 0;

static void da_try_register_fire_note_on_current_gun()
{
    if (!cl_frame) {
        if (orig_Com_Printf) orig_Com_Printf("\x02[DA] no cl_frame\n");
        return;
    }
    player_state_t &ps = cl_frame->playerstate;

    long ps_offset = (long)((unsigned char*)&ps - (unsigned char*)cl_frame);
    if (orig_Com_Printf) orig_Com_Printf("[DA] playerstate offset=%ld bytes\n", ps_offset);
    size_t off_fov = (size_t)((unsigned char*)&ps.fov - (unsigned char*)&ps);
    size_t off_dm = (size_t)((unsigned char*)&ps.dmRank - (unsigned char*)&ps);
    if (orig_Com_Printf) orig_Com_Printf("[DA] dmRank-fov offset = %ld bytes\n", (long)(off_dm - off_fov));

    // Track spectator change
    byte spec = ps.spectatorId;
    IGhoulInst * gun = ps.gun;

    
    short gunUUID = ps.gunUUID;
    float fov = ps.fov;
    
    if (orig_Com_Printf) orig_Com_Printf("[DA] current gunUUID=%d fov=%.1f\n", (int)gunUUID, fov);
    short da0 = ps.pmove.delta_angles[0];
    short da1 = ps.pmove.delta_angles[1];
    short da2 = ps.pmove.delta_angles[2];
    if (orig_Com_Printf) orig_Com_Printf("[DA] pmove.delta_angles = %d, %d, %d\n", (int)da0, (int)da1, (int)da2);
    short o0 = ps.pmove.origin[0];
    short o1 = ps.pmove.origin[1];
    short o2 = ps.pmove.origin[2];
    if (orig_Com_Printf) orig_Com_Printf("[DA] pmove.origin = %d, %d, %d\n", (int)o0, (int)o1, (int)o2);
    pmtype_t pmtype = ps.pmove.pm_type;
    if (orig_Com_Printf) orig_Com_Printf("[DA] pmove.pm_type = %d\n", (int)pmtype);
    unsigned char *ps_bytes = (unsigned char*)&ps;
    char ps_hex[16*3 + 1];
    int ps_hx = 0;
    for (int i = 0; i < 16; ++i) {
        ps_hx += snprintf(ps_hex + ps_hx, sizeof(ps_hex) - ps_hx, "%02X ", ps_bytes[i]);
    }
    if (ps_hx > 0) ps_hex[ps_hx-1] = '\0';
    if (orig_Com_Printf) orig_Com_Printf("[DA] ps[0..15] = %s\n", ps_hex);
    

    if (cl_frame) {
        int sf = cl_frame->serverframe;
        int st = cl_frame->servertime;
        float gt = cl_frame->ghoultime;
        if (orig_Com_Printf) orig_Com_Printf("[DA] frame: serverframe=%d servertime=%d ghoultime=%.3f\n", sf, st, gt);
        int dmRank = (int)ps.dmRank;
        int spectator = (int)ps.spectatorId;
        int num_entities = cl_frame->num_entities;
        if (orig_Com_Printf) orig_Com_Printf("[DA] dmRank=%d spectatorId=%d num_entities=%d\n", dmRank, spectator, num_entities);
    }

    if (!gun) {
        // if (orig_Com_Printf) orig_Com_Printf("\x02[DA] no gun (spec %u %i)\n", (unsigned)spec, sizeof(player_state_t));
        return;
    }

    // Avoid duplicate registrations on same gun pointer
    if (gun == g_lastRegisteredGun ) {
        if (orig_Com_Printf) orig_Com_Printf("\x02[DA] gun already registered (spec %u)\n", (unsigned)spec);
        return;
    }
    // Point ghoul helper at this IGhoulInst
    // Prefer resolving clientinst via GhoulInstFromID(gunUUID). Fall back to using
    // the raw gun pointer only if it looks plausible. This avoids assigning
    // clientinst to small/invalid pointers which cause page faults.
    bool gotClientInst = false;
    if (gunUUID != 0) {
        if (GhoulInstFromID(gunUUID)) {
            gotClientInst = true;
        } else {
            if (orig_Com_Printf) orig_Com_Printf("\x02[DA] GhoulInstFromID(%d) failed\n", (int)gunUUID);
        }
    }
    if (!gotClientInst) {
        unsigned int gunAddr = (unsigned int)gun;
        if (gunAddr > 0x1000) {
            clientinst = (unsigned int*)gun;
            gotClientInst = true;
        } else {
            if (orig_Com_Printf) orig_Com_Printf("\x02[DA] gun pointer invalid: %p\n", gun);
        }
    }

    if (orig_Com_Printf) orig_Com_Printf("[DA] current gunUUID=%d fov=%.1f\n", (int)gunUUID, fov);

    // Resolve token and register callback
    if (gotClientInst && GhoulGetObject()) {
        orig_Com_Printf("Calling GhoulFindNoteToken\n");
        GhoulID tok = GhoulFindNoteToken("fire");
        if (tok) {
            GhoulAddNoteCallBack(&g_daGunFireCb, tok);
            g_lastRegisteredGun = gun;
            g_lastSpectatorId = spec;
            if (orig_Com_Printf) orig_Com_Printf("\x03[DA] registered fire note cb (spec %u)\n", (unsigned)spec);
        } else {
            if (orig_Com_Printf) orig_Com_Printf("\x02[DA] GhoulFindNoteToken failed (spec %u)\n", (unsigned)spec);
        }
    } else {
        if (orig_Com_Printf) orig_Com_Printf("\x02[DA] GhoulGetObject failed (spec %u)\n", (unsigned)spec);
    }
    orig_Com_Printf("end of da_try_register_fire_note_on_current_gun\n");
}

// Capture state for the current team icon being drawn
static bool g_isCapturing = false;
static bool g_haveV1 = false;
static bool g_haveV2 = false;
static float g_v1x = 0.0f, g_v1y = 0.0f;
static float g_v2x = 0.0f, g_v2y = 0.0f;
static char  g_playerName[64];
static DWORD g_lastBeginMs = 0;
static float g_bestAbsDeltaDeg = 1e9f;
static float g_bestThetaDeg = 0.0f;
static float g_minx = 1e9f;
static float g_maxx = -1e9f;
static float g_miny = 1e9f;
static float g_last_frame_best_deg = 0.0f;
static int   g_shot_pending = 0;
static int   g_shot_logged = 0;
static DWORD g_shot_pending_ms = 0;

// Forward declarations used by ref_fixes.cpp and scaled_font.cpp
extern "C" {
    // Called from ref_fixes.cpp during team icon draw begin for each player
    //   see: ref_fixes.cpp around demoAnalyzer_begin/End usage
    void demoAnalyzer_begin(const float * targetOrigin, const char * playerName);
    void demoAnalyzer_onVertex2f(float x, float y);
    void demoAnalyzer_end(void);
    // Called once per frame via SCR_UpdateScreen detour (see sof_buddy.cpp),
    // originally intended as a general per-frame hook (e.g., via Qcommon_Frame)
    void demoAnalyzer_tick(void);
}

// Shot CSV logging
static const int MAX_SHOT_RECORDS = 65536;
static float g_shot_err_deg[MAX_SHOT_RECORDS];
static float g_shot_theta_deg[MAX_SHOT_RECORDS];
static float g_shot_sim[MAX_SHOT_RECORDS];
static DWORD g_shot_time_ms[MAX_SHOT_RECORDS];
static int   g_shot_count = 0;
static void da_log_shot(float err_deg, float theta_deg, float sim);
static void da_flush_csv(void);
static void da_enable_change(cvar_t * cvar);

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

/*

    Draw Team Icon hook _BEFORE_ the team-icon has been drawn
*/
void demoAnalyzer_begin(const float * /*targetOrigin*/, const char * playerName)
{
    if (!_sofbuddy_demo_analyzer || _sofbuddy_demo_analyzer->value == 0.0f) return;
    // Ensure fire callback is registered for current spectated player's gun
    da_try_register_fire_note_on_current_gun();
    // Start capture for this player's icon draw
    DWORD now = GetTickCount();
    if (now - g_lastBeginMs > 5) {
        g_bestAbsDeltaDeg = 1e9f;
        g_bestThetaDeg = 0.0f;
        g_lastBeginMs = now;
    }
    g_isCapturing = true;
    g_haveV1 = false;
    g_haveV2 = false;
    g_minx = 1e9f;
    g_maxx = -1e9f;
    g_miny = 1e9f;
    if (playerName) {
        strncpy(g_playerName, playerName, sizeof(g_playerName) - 1);
        g_playerName[sizeof(g_playerName) - 1] = '\0';
    } else {
        g_playerName[0] = '\0';
    }
}

void demoAnalyzer_onVertex2f(float x, float y)
{
    if (!g_isCapturing) return;
    if (!g_haveV1) {
        g_v1x = x; g_v1y = y; g_haveV1 = true; return;
    }
    if (!g_haveV2) {
        g_v2x = x; g_v2y = y; g_haveV2 = true; return;
    }
    // Track bounds across all vertices while capturing
    if (x < g_minx) g_minx = x;
    if (x > g_maxx) g_maxx = x;
    if (y < g_miny) g_miny = y;
}

/*
    Draw Team Icon hook _AFTER_ the team-icon has been drawn
*/
void demoAnalyzer_end(void)
{
    if (!g_isCapturing) return;
    g_isCapturing = false;

    if (!_sofbuddy_demo_analyzer || _sofbuddy_demo_analyzer->value == 0.0f) return;
    if (!orig_Draw_String_Color) return;

    // Estimate the on-screen horizontal center of the icon
    float icon_cx;
    float icon_top;
    // Use bounds if we observed any beyond the first two
    float minx_obs = g_minx;
    float maxx_obs = g_maxx;
    float miny_obs = g_miny;
    if (!(minx_obs <= maxx_obs)) {
        // Fallback to first two vertices if bounds invalid
        if (g_haveV1 && g_haveV2) {
            float minx = (g_v1x < g_v2x) ? g_v1x : g_v2x;
            float maxx = (g_v1x > g_v2x) ? g_v1x : g_v2x;
            icon_cx = 0.5f * (minx + maxx);
            icon_top = (g_v1y < g_v2y) ? g_v1y : g_v2y;
        } else if (g_haveV1) {
            // Best-effort: assume 32px width
            icon_cx = g_v1x + 16.0f;
            icon_top = g_v1y;
        } else {
            return;
        }
    } else {
        icon_cx = 0.5f * (minx_obs + maxx_obs);
        icon_top = miny_obs;
    }

    if (current_vid_w <= 0 || !ref_fov_x) return;
    float fovX_deg = *ref_fov_x;
    if (fovX_deg <= 0.0f) fovX_deg = 90.0f;

    // Horizontal angular offset estimate from center using perspective mapping
    float midX = 0.5f * (float)current_vid_w;
    float x_ndc = (icon_cx - midX) / midX; // -1..1
    float halfFovRad = DEG2RAD(0.5f * fovX_deg);
    float thetaRad = atanf(x_ndc * tanf(halfFovRad));
    float thetaDeg = thetaRad * (180.0f / 3.14159265358979323846f);

    // Similarity score: 1 at center, falls off with horizontal angle (cosine)
    float sim = cosf(thetaRad);
    sim = clamp01(sim);

    // Track best absolute yaw delta this frame
    float absDelta = (thetaDeg < 0 ? -thetaDeg : thetaDeg);
    if (absDelta < g_bestAbsDeltaDeg) {
        g_bestAbsDeltaDeg = absDelta;
        g_bestThetaDeg = thetaDeg;
    }

    // Color mapping by similarity (user-specified mapping)
    // red (most similar), yellow, green, blue (least similar)
    // Define by angular thresholds
    int colorIdx = 0; // white fallback
    if (absDelta <= 2.0f) {
        colorIdx = 1; // red
    } else if (absDelta <= 5.0f) {
        colorIdx = 3; // yellow
    } else if (absDelta <= 12.0f) {
        colorIdx = 2; // green
    } else {
        colorIdx = 4; // blue
    }

    // Draw angle text above the icon
    char buf[64];
    if (g_playerName[0] != '\0') {
        int n = snprintf(buf, sizeof(buf), "%.1f\xB0 %s", thetaDeg, g_playerName);
        (void)n;
    } else {
        int n = snprintf(buf, sizeof(buf), "%.1f\xB0", thetaDeg);
        (void)n;
    }

    int text_x = (int)icon_cx;
    int text_y = (int)(icon_top - 12.0f);
    if (text_x < 0) text_x = 0;
    if (text_x > current_vid_w - 8) text_x = current_vid_w - 8;
    if (text_y < 0) text_y = 0;

    // Center the label horizontally above the icon
    int label_px = (int)strlen(buf) * 8;
    draw_string_raw(text_x - label_px / 2, text_y, buf, colorIdx);

    // Draw current angle info at screen center above crosshair
    // Using approximate centering with 8px per character
    char centerBuf[64];
    int cn = snprintf(centerBuf, sizeof(centerBuf), "CUR 0.0\xB0 | BEST %+.1f\xB0", g_bestThetaDeg);
    (void)cn;
    int center_x = current_vid_w / 2;
    int center_y = current_vid_h / 2 - 40;
    int half_px = (int)(4 * (float)strlen(centerBuf));
    draw_string_raw(center_x - half_px, center_y, centerBuf, 0);

    // Update per-frame best for shot logging
    g_last_frame_best_deg = g_bestThetaDeg;

    // If a shot is pending, log using the freshly updated best
    if (g_shot_pending && !g_shot_logged) {
        da_log_shot(fabsf(g_last_frame_best_deg), g_last_frame_best_deg, sim);
        g_shot_logged = 1;
        g_shot_pending = 0;
    }
}

// ======================================================================================
// Raw text drawing using Draw_Char with paletteRGBA, avoids old std::string ABI issues
// ======================================================================================
static void draw_string_raw(int x, int y, const char * text, int colorIdx)
{
    if (!text) return;
    // Resolve function pointer once
    typedef void (*t_draw_char)(int, int, int, paletteRGBA_t *);
    static t_draw_char pDrawChar = NULL;
    if (!pDrawChar) {
        pDrawChar = *(t_draw_char*)0x204035C8;
        if (!pDrawChar) return;
    }

    // Map color indices to RGBA
    paletteRGBA_t pal;
    switch (colorIdx) {
        case 1: pal.r = 255; pal.g = 0;   pal.b = 0;   pal.a = 255; break; // red
        case 3: pal.r = 255; pal.g = 255; pal.b = 0;   pal.a = 255; break; // yellow
        case 2: pal.r = 0;   pal.g = 255; pal.b = 0;   pal.a = 255; break; // green
        case 4: pal.r = 32;  pal.g = 128; pal.b = 255; pal.a = 255; break; // blue
        default: pal.r = 255; pal.g = 255; pal.b = 255; pal.a = 255; break; // white
    }

    // Draw each glyph (8px advance)
    int cx = x;
    for (const char *p = text; *p; ++p) {
        unsigned char ch = (unsigned char)*p;
        pDrawChar(cx, y, ch, &pal);
        cx += 8;
    }
}

// Public init APIs
void create_demo_analyzer_cvars(void)
{
    _sofbuddy_demo_analyzer = orig_Cvar_Get("_sofbuddy_demo_analyzer", "1", CVAR_ARCHIVE, &da_enable_change);
    _sofbuddy_da_mode = orig_Cvar_Get("_sofbuddy_da_mode", "1", CVAR_ARCHIVE, NULL);
    _sofbuddy_da_thresh_deg = orig_Cvar_Get("_sofbuddy_da_thresh_deg", "1.0", CVAR_ARCHIVE, NULL);
    _sofbuddy_da_shot_only = orig_Cvar_Get("_sofbuddy_da_shot_only", "0", CVAR_ARCHIVE, NULL);
    _sofbuddy_da_pre_ms = orig_Cvar_Get("_sofbuddy_da_pre_ms", "250", CVAR_ARCHIVE, NULL);
    _sofbuddy_da_post_ms = orig_Cvar_Get("_sofbuddy_da_post_ms", "150", CVAR_ARCHIVE, NULL);
    _sofbuddy_da_log = orig_Cvar_Get("_sofbuddy_da_log", "1", CVAR_ARCHIVE, NULL);
    _sofbuddy_da_hist_bin_deg = orig_Cvar_Get("_sofbuddy_da_hist_bin_deg", "0.25", CVAR_ARCHIVE, NULL);
    _sofbuddy_da_csv = orig_Cvar_Get("_sofbuddy_da_csv", "shot_accuracy.csv", CVAR_ARCHIVE, NULL);
}

// FireClient hook storage and body
static void (__cdecl *orig_FireClient)(void*,void*,void*) = NULL;
static void __cdecl da_my_FireClient(void* a, void* b, void* c)
{
    // Only measure shotgun fires (weapon id 8)
    short wpn = scurrent_gun ? *scurrent_gun : -1;
    if (wpn == 8) {
        byte spec = 0;
        int serverframe = -1;
        int num_entities = -1;
        if (cl_frame) {
            spec = cl_frame->playerstate.spectatorId;
            serverframe = cl_frame->serverframe;
            num_entities = cl_frame->num_entities;
        }
        orig_Com_Printf("da_my_FireClient: shot fired (spec %u) serverframe=%d num_entities=%d\n", (unsigned)spec, serverframe, num_entities);
        g_da_shotFiredThisFrame = 1;
        g_shot_pending = 1;
        g_shot_logged = 0;
        g_shot_pending_ms = GetTickCount();
    }
    if (orig_FireClient) orig_FireClient(a,b,c);
}

void demoAnalyzer_early(void)
{
    // Install FireClient detour (sharedEdict_s &, edict_s &, inven_c &)
    if (!orig_FireClient) {
        orig_FireClient = (decltype(orig_FireClient))DetourCreate((void*)0x200126E0, (void*)&da_my_FireClient, DETOUR_TYPE_JMP, 5);
    }
    // Install CL_ParseFrame detour to populate frame cvars after parsing
    static void (*orig_CL_ParseFrame)(void) = NULL;
    // handler is defined in src/core.cpp; forward declaration only
    extern void my_CL_ParseFrame(void);
    if (!orig_CL_ParseFrame) {
        orig_CL_ParseFrame = (decltype(orig_CL_ParseFrame))DetourCreate((void*)0x20002E90, (void*)&my_CL_ParseFrame, DETOUR_TYPE_JMP, 8);
    }
}

// helper to set cvar value (string) using orig_Cvar_Set2 if available
static void set_cvar_from_str(cvar_t **slot, const char *name, const char *valstr)
{
    ensure_cvar(name, slot);
    if (orig_Cvar_Set2) {
        orig_Cvar_Set2((char*)name, (char*)valstr, 1);
        return;
    }
    if (*slot) {
        (*slot)->value = (float)atof(valstr);
    }
}


