#include <windows.h>
#include "sof_compat.h"
#include "features.h"
#include "DetourXS/detourxs.h"
#include <stdio.h>

// Minimal pmove/frame/playerstate copies (match demo_analyzer.cpp layout)
typedef enum { PM_NORMAL, PM_SPECTATOR, PM_DEAD, PM_GIB, PM_FREEZE } pmtype_t;
typedef struct {
    pmtype_t pm_type;
    short origin[3];
    short velocity[3];
    byte pm_flags;
    byte pm_time;
    short gravity;
    short delta_angles[3];
} pmove_state_t;

#ifndef MAX_STATS
#define MAX_STATS 32
#endif

typedef struct {
    pmove_state_t    pmove;
    float            viewangles[3];
    float            viewoffset[3];
    float            kick_angles[3];
    float            weaponkick_angles[3];
    float            remote_vieworigin[3];
    float            remote_viewangles[3];
    int              remote_id;
    byte             remote_type;
    void*            gun;
    short            gunUUID;
    short            gunType;
    short            gunClip;
    short            gunAmmo;
    byte             gunReload;
    byte             restart_count;
    byte             buttons_inhibit;
    void*            bod;
    short            bodUUID;
    float            blend[4];
    float            fov;
    int              rdflags;
    short            soundID;
    byte             musicID;
    short            damageLoc;
    short            damageDir;
    short            stats[MAX_STATS];
    byte             dmRank;
    byte             dmRankedPlyrs;
    byte             spectatorId;
    byte             cinematicfreeze;
} player_state_t;

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

static frame_t * const cl_frame_local = (frame_t*)0x201E7E60;

// cvar helper (local copies)
static cvar_t * ensure_cvar_local(const char *name)
{
    if (!orig_Cvar_Get) return NULL;
    return orig_Cvar_Get(name, "0", CVAR_INTERNAL, NULL);
}

static void set_cvar_value_local(const char *name, const char *val)
{
    cvar_t *c = ensure_cvar_local(name);
    if (!c) return;
    if (orig_Cvar_Set2) {
        orig_Cvar_Set2((char*)name, (char*)val, 1);
        return;
    }
    c->value = (float)atof(val);
}

// CL_ParseFrame detour handler
static void my_CL_ParseFrame_impl(void)
{
    // call original (best-effort trampoline)
    void (*orig)(void) = (void(*)(void))0x20002E90;
    if (orig) orig();

    frame_t *f = cl_frame_local;
    if (!f) return;

    char buf[64];
    snprintf(buf, sizeof(buf), "%d", (int)f->valid);
    set_cvar_value_local("frame_valid", buf);
    snprintf(buf, sizeof(buf), "%d", f->serverframe);
    set_cvar_value_local("frame_serverframe", buf);
    snprintf(buf, sizeof(buf), "%d", f->servertime);
    set_cvar_value_local("frame_servertime", buf);
    snprintf(buf, sizeof(buf), "%d", f->deltaframe);
    set_cvar_value_local("frame_deltaframe", buf);
    snprintf(buf, sizeof(buf), "%.3f", f->ghoultime);
    set_cvar_value_local("frame_ghoultime", buf);
    snprintf(buf, sizeof(buf), "%d", f->num_entities);
    set_cvar_value_local("frame_num_entities", buf);

    // playerstate pmove
    player_state_t *ps = &f->playerstate;
    snprintf(buf, sizeof(buf), "%d", (int)ps->pmove.pm_type);
    set_cvar_value_local("frame_playerstate_pmove_pm_type", buf);
    snprintf(buf, sizeof(buf), "%d", (int)ps->pmove.origin[0]);
    set_cvar_value_local("frame_playerstate_pmove_origin0", buf);
    snprintf(buf, sizeof(buf), "%d", (int)ps->pmove.origin[1]);
    set_cvar_value_local("frame_playerstate_pmove_origin1", buf);
    snprintf(buf, sizeof(buf), "%d", (int)ps->pmove.origin[2]);
    set_cvar_value_local("frame_playerstate_pmove_origin2", buf);

    // a few more examples
    snprintf(buf, sizeof(buf), "%d", (int)ps->dmRank);
    set_cvar_value_local("frame_playerstate_dmRank", buf);
    snprintf(buf, sizeof(buf), "%d", (int)ps->spectatorId);
    set_cvar_value_local("frame_playerstate_spectatorId", buf);
}

// pointer to original CL_ParseFrame trampoline returned by DetourCreate
static void (*orig_CL_ParseFrame)(void) = NULL;

// normal (non-naked) detour target
static void my_CL_ParseFrame(void)
{
    // call original first if available
    if (orig_CL_ParseFrame) orig_CL_ParseFrame();
    // then run our implementation
    my_CL_ParseFrame_impl();
}

// Called from demoAnalyzer_early to install hooks
extern "C" void core_init(void)
{
    if (!orig_CL_ParseFrame) {
        orig_CL_ParseFrame = (decltype(orig_CL_ParseFrame))DetourCreate((void*)0x20002E90, (void*)&my_CL_ParseFrame, DETOUR_TYPE_JMP, 8);
    }
}


