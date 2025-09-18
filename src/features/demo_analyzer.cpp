#include <windows.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "sof_compat.h"
#include "features.h"

#include "ighoul.h"
#include <unordered_map>
#include <utility>

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

// core-managed frame cvars moved to src/core.cpp

// External state/functions we reuse
extern int current_vid_w;
extern int current_vid_h;
// core initialization (hooks) implemented in src/core.cpp
extern "C" void core_init(void);

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

// frame/playerstate types are provided by hdr/sof_compat.h
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
        orig_Com_Printf("[DA] gun fire note (spec %u) serverframe=%d num_entities=%d\n", (unsigned)spec, serverframe, num_entities);
        return true;
    }
};

static DA_GunFireCallback g_daGunFireCb;
static IGhoulInst * g_lastRegisteredGun = NULL;
static byte g_lastSpectatorId = 0;
// Cache mapping gunUUID -> (resolved IGhoulInst*, callbackRegistered)
static std::unordered_map<short, std::pair<IGhoulInst*, bool>> g_gunInstCache;
// Track ghoulmain to detect reloads and invalidate cache
static decltype(ghoulmain) g_prevGhoulMain = NULL;


/*
    These note callbacks only work if the client is not predicting the server. (cl_predict_weapon 0)
    Not sure why this is, maybe the ghoul system is in a intermittent state when the client is predicting.
    Well, you'd have to use another function to get the cloned instance, thats all.
*/
static void da_try_register_fire_note_on_current_gun()
{
    ghoulmain = orig_GetGhoul(1,0);

    if (!cl_frame) {
        orig_Com_Printf("\x02[DA] no cl_frame\n");
        return;
    }
    player_state_t &ps = cl_frame->playerstate;

    // Track spectator change
    byte spec = ps.spectatorId;

    // Invalidate cache if ghoulmain changed (ghoul objects reloaded)
    if (g_prevGhoulMain != ghoulmain) {
        g_gunInstCache.clear();
        g_prevGhoulMain = ghoulmain;
    }

    #if 0
    // Try cache first (cache stores {inst, callbackRegistered})
    auto it = g_gunInstCache.find(ps.gunUUID);
    bool callbackAlready = false;
    IGhoulInst * resolved = NULL;
    if (it != g_gunInstCache.end()) {
        resolved = it->second.first;
        callbackAlready = it->second.second;
        if (resolved) clientinst = (unsigned int*)resolved;
    }
    if (!resolved) {
        // Resolve GHoul instance from gun UUID so clientinst is set
        if (!GhoulInstFromID(ps.gunUUID)) {
            // orig_Com_Printf("\x02gun inst null\n");
            return;
        }
        resolved = (IGhoulInst*)clientinst;
        if (resolved) {
            g_gunInstCache[ps.gunUUID] = std::make_pair(resolved, false);
        }
    }
    
    if (clientinst == NULL) return;
    #else
    if (!GhoulInstFromID(ps.gunUUID)) {
        orig_Com_Printf("\x02gun inst null\n");
        return;
    }
    #endif
    #if 0
    // If we've already applied our callback to this inst, skip
    if (callbackAlready || (resolved && resolved == g_lastRegisteredGun)) {
        orig_Com_Printf("\x02[DA] gun already registered (spec %u)\n", (unsigned)spec);
        return;
    }
    #endif
    // Resolve token and register callback
    if ( GhoulGetObject()) {
        orig_Com_Printf("Calling GhoulFindNoteToken\n");
        GhoulID tok = GhoulFindNoteToken("fire");
        orig_Com_Printf("[DA DBG] GhoulFindNoteToken returned %p\n", (void*)tok);
        if (tok) {
            GhoulAddNoteCallBack(&g_daGunFireCb, tok);
            g_lastRegisteredGun = (IGhoulInst*)clientinst;
            g_lastSpectatorId = spec;
            // mark cache entry as having our callback
            g_gunInstCache[ps.gunUUID].second = true;
            orig_Com_Printf("\x03[DA] registered fire note cb (spec %u)\n", (unsigned)spec);
        } else {
            orig_Com_Printf("\x02[DA] GhoulFindNoteToken failed (spec %u)\n", (unsigned)spec);
        }
    } else {
        orig_Com_Printf("\x02[DA] GhoulGetObject failed (spec %u)\n", (unsigned)spec);
    }
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
// Called from ref_fixes.cpp during team icon draw begin for each player
//   see: ref_fixes.cpp around demoAnalyzer_begin/End usage
void demoAnalyzer_begin(const float * targetOrigin, const char * playerName);
void demoAnalyzer_onVertex2f(float x, float y);
void demoAnalyzer_end(void);
// Called once per frame via SCR_UpdateScreen detour (see core.cpp)
void demoAnalyzer_tick(void)
{
    // Minimal per-frame processing: reset per-frame shot fired flag and
    // perform light housekeeping. Keep implementation small to satisfy
    // linker and avoid depending on other init ordering.
    g_da_shotFiredThisFrame = 0;
    // If we have a pending shot that has been logged, clear pending state
    if (g_shot_pending && g_shot_logged) {
        g_shot_pending = 0;
        g_shot_logged = 0;
    }
}

// Shot CSV logging
static const int MAX_SHOT_RECORDS = 65536;
static float g_shot_err_deg[MAX_SHOT_RECORDS];
static float g_shot_theta_deg[MAX_SHOT_RECORDS];
static float g_shot_sim[MAX_SHOT_RECORDS];
static DWORD g_shot_time_ms[MAX_SHOT_RECORDS];
static int   g_shot_count = 0;
static void da_log_shot(float err_deg, float theta_deg, float sim)
{
    orig_Com_Printf("[DA DBG] da_log_shot err=%.3f theta=%.3f sim=%.6f count=%d\n", err_deg, theta_deg, sim, g_shot_count);
    if (g_shot_count >= MAX_SHOT_RECORDS) return;
    g_shot_err_deg[g_shot_count] = err_deg;
    g_shot_theta_deg[g_shot_count] = theta_deg;
    g_shot_sim[g_shot_count] = sim;
    // use system tick as fallback so function doesn't depend on g_time_ms declaration order
    g_shot_time_ms[g_shot_count] = GetTickCount();
    g_shot_count++;
}
static void da_flush_csv(void)
{
    if (!_sofbuddy_da_csv || !_sofbuddy_da_csv->string || g_shot_count <= 0) return;
    const char * path = _sofbuddy_da_csv->string;
    FILE * f = fopen(path, "w");
    if (!f) {
        orig_Com_Printf("da_flush_csv: failed to open %s\n", path);
        return;
    }
    fprintf(f, "time_ms,err_deg,theta_deg,sim\n");
    for (int i=0; i<g_shot_count; ++i) {
        fprintf(f, "%lu,%.4f,%.4f,%.6f\n", (unsigned long)g_shot_time_ms[i], g_shot_err_deg[i], g_shot_theta_deg[i], g_shot_sim[i]);
    }
    fclose(f);
    orig_Com_Printf("da_flush_csv: wrote %d records to %s\n", g_shot_count, path);
    g_shot_count = 0;
}
static void da_enable_change(cvar_t * cvar)
{
    // On disable, flush CSV
    if (cvar && cvar->value == 0.0f) {
        da_flush_csv();
    }
}

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
    // orig_Com_Printf("[DA DBG] demoAnalyzer_onVertex2f x=%.2f y=%.2f capturing=%d\n", x, y, g_isCapturing?1:0);
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
void demoAnalyzer_cvars_init(void)
{
#ifdef DISABLE_DEMO_ANALYZER_TEST
    // Temporarily disabled for crash triage
    return;
#endif
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

void demoAnalyzer_init(void)
{
#ifdef DISABLE_DEMO_ANALYZER_TEST
    // Temporarily disabled for crash triage
    return;
#endif
    // Install FireClient detour (sharedEdict_s &, edict_s &, inven_c &)
    if (!orig_FireClient) {
        orig_FireClient = (decltype(orig_FireClient))DetourCreate((void*)0x200126E0, (void*)&da_my_FireClient, DETOUR_TYPE_JMP, 5);
    }
    // core_init() is called from sof_buddy after WSA startup; nothing to do here
}

// helper to set cvar value (string) using orig_Cvar_Set2 if available
static void set_cvar_from_str(cvar_t **slot, const char *name, const char *valstr)
{
    // ensure_cvar is now in core; fallback to calling orig_Cvar_Get directly
    if (!*slot && orig_Cvar_Get) {
        *slot = orig_Cvar_Get(name, "0", CVAR_INTERNAL, NULL);
    }
    if (orig_Cvar_Set2) {
        orig_Cvar_Set2((char*)name, (char*)valstr, 1);
        return;
    }
    if (*slot) {
        (*slot)->value = (float)atof(valstr);
    }
}


