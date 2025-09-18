#include <windows.h>
#include "sof_compat.h"
#include "features.h"
#include "DetourXS/detourxs.h"
#include <stdio.h>
#include <string.h>
#include "util.h"

// frame/playerstate types are provided by hdr/sof_compat.h
static frame_t * const cl_frame_local = (frame_t*)0x201E7E60;
// previous frame snapshot for change detection
// previous player and pmove snapshots (zero-initialized statically)
static player_state_t prev_playerstate;
static pmove_state_t prev_pmove;
static int prev_serverframe = -1;
static int prev_servertime = -1;
static int prev_deltaframe = -1;
static float prev_ghoultime = -1.0f;
static int prev_num_entities = -1;
static int prev_valid = -1;
static unsigned char prev_areabits[256/8];
static int prev_parse_entities = -1;

// Global cvar handles for frame_* values (initialized in core_cvars_init)
static cvar_t *cv_frame_valid = NULL;
static cvar_t *cv_frame_serverframe = NULL;
static cvar_t *cv_frame_servertime = NULL;
static cvar_t *cv_frame_deltaframe = NULL;
static cvar_t *cv_frame_ghoultime = NULL;
static cvar_t *cv_frame_num_entities = NULL;
static cvar_t *cv_frame_playerstate_pmove_pm_type = NULL;
static cvar_t *cv_frame_playerstate_pmove_origin0 = NULL;
static cvar_t *cv_frame_playerstate_pmove_origin1 = NULL;
static cvar_t *cv_frame_playerstate_pmove_origin2 = NULL;
static cvar_t *cv_frame_playerstate_dmRank = NULL;
static cvar_t *cv_frame_playerstate_spectatorId = NULL;
// additional player_state cvars
static cvar_t *cv_frame_player_viewangles0 = NULL;
static cvar_t *cv_frame_player_viewangles1 = NULL;
static cvar_t *cv_frame_player_viewangles2 = NULL;
static cvar_t *cv_frame_player_viewoffset0 = NULL;
static cvar_t *cv_frame_player_viewoffset1 = NULL;
static cvar_t *cv_frame_player_viewoffset2 = NULL;
static cvar_t *cv_frame_player_fov = NULL;
static cvar_t *cv_frame_player_rdflags = NULL;
static cvar_t *cv_frame_player_soundID = NULL;
static cvar_t *cv_frame_player_musicID = NULL;
static cvar_t *cv_frame_player_damageLoc = NULL;
static cvar_t *cv_frame_player_damageDir = NULL;
// stats 0..(MAX_STATS-1) handled by cv_frame_player_stats[] array
// pmove fields
static cvar_t *cv_frame_pmove_pm_type = NULL;
static cvar_t *cv_frame_pmove_origin0 = NULL;
static cvar_t *cv_frame_pmove_origin1 = NULL;
static cvar_t *cv_frame_pmove_origin2 = NULL;
static cvar_t *cv_frame_pmove_velocity0 = NULL;
static cvar_t *cv_frame_pmove_velocity1 = NULL;
static cvar_t *cv_frame_pmove_velocity2 = NULL;
static cvar_t *cv_frame_pmove_pm_flags = NULL;
static cvar_t *cv_frame_pmove_pm_time = NULL;
static cvar_t *cv_frame_pmove_gravity = NULL;
static cvar_t *cv_frame_pmove_delta_angles0 = NULL;
static cvar_t *cv_frame_pmove_delta_angles1 = NULL;
static cvar_t *cv_frame_pmove_delta_angles2 = NULL;
// player remote/gun/bod
static cvar_t *cv_frame_player_remote_id = NULL;
static cvar_t *cv_frame_player_remote_type = NULL;
static cvar_t *cv_frame_player_gun_ptr = NULL;
static cvar_t *cv_frame_player_gunUUID = NULL;
static cvar_t *cv_frame_player_gunType = NULL;
static cvar_t *cv_frame_player_gunClip = NULL;
static cvar_t *cv_frame_player_gunAmmo = NULL;
static cvar_t *cv_frame_player_gunReload = NULL;
static cvar_t *cv_frame_player_restart_count = NULL;
static cvar_t *cv_frame_player_buttons_inhibit = NULL;
static cvar_t *cv_frame_player_bod_ptr = NULL;
static cvar_t *cv_frame_player_bodUUID = NULL;
// blend[4]
static cvar_t *cv_frame_player_blend0 = NULL;
static cvar_t *cv_frame_player_blend1 = NULL;
static cvar_t *cv_frame_player_blend2 = NULL;
static cvar_t *cv_frame_player_blend3 = NULL;
// areabits 32 bytes
static cvar_t *cv_frame_areabits[32];
// stats
static cvar_t *cv_frame_player_stats[MAX_STATS];
static cvar_t *cv_frame_player_kick_angles0 = NULL;
static cvar_t *cv_frame_player_kick_angles1 = NULL;
static cvar_t *cv_frame_player_kick_angles2 = NULL;
static cvar_t *cv_frame_player_weaponkick_angles0 = NULL;
static cvar_t *cv_frame_player_weaponkick_angles1 = NULL;
static cvar_t *cv_frame_player_weaponkick_angles2 = NULL;
static cvar_t *cv_frame_player_remote_vieworigin0 = NULL;
static cvar_t *cv_frame_player_remote_vieworigin1 = NULL;
static cvar_t *cv_frame_player_remote_vieworigin2 = NULL;
static cvar_t *cv_frame_player_remote_viewangles0 = NULL;
static cvar_t *cv_frame_player_remote_viewangles1 = NULL;
static cvar_t *cv_frame_player_remote_viewangles2 = NULL;
static cvar_t *cv_frame_player_dmRankedPlyrs = NULL;
static cvar_t *cv_frame_player_cinematicfreeze = NULL;
static cvar_t *cv_frame_parse_entities = NULL;
// additional arrays/fields
// SCR_UpdateScreen trampoline
typedef void (*SCR_UpdateScreen_type)(BYTE, BYTE, BYTE, BYTE);
static SCR_UpdateScreen_type orig_SCR_UpdateScreen_local = NULL;

void SCR_UpdateScreen_local(BYTE arg00, BYTE arg01, BYTE arg02, BYTE arg03)
{
#ifdef FEATURE_DEMO_ANALYZER
    demoAnalyzer_tick();
#endif
    orig_SCR_UpdateScreen_local(arg00, arg01, arg02, arg03);
}

// Install SCR_UpdateScreen detour (called from core_early)
static void install_scr_update_detour(void)
{
    if (!orig_SCR_UpdateScreen_local) {
        orig_SCR_UpdateScreen_local = (SCR_UpdateScreen_type)DetourCreate((void*)0x20015FA0, (void*)&SCR_UpdateScreen_local, DETOUR_TYPE_JMP, 5);
    }
}

// Weapon and inventory name lookups (keeps code independent of enum symbols)
static const char *weapon_names[] = {
    "SFW_EMPTYSLOT",
    "SFW_KNIFE",
    "SFW_PISTOL2",
    "SFW_PISTOL1",
    "SFW_MACHINEPISTOL",
    "SFW_ASSAULTRIFLE",
    "SFW_SNIPER",
    "SFW_AUTOSHOTGUN",
    "SFW_SHOTGUN",
    "SFW_MACHINEGUN",
    "SFW_ROCKET",
    "SFW_MICROWAVEPULSE",
    "SFW_FLAMEGUN",
    "SFW_HURTHAND",
    "SFW_THROWHAND",
};
static const int weapon_names_count = sizeof(weapon_names) / sizeof(weapon_names[0]);

static const char *inv_names[] = {
    "SFE_EMPTYSLOT",
    "SFE_FLASHPACK",
    "SFE_C4",
    "SFE_LIGHT_GOGGLES",
    "SFE_CLAYMORE",
    "SFE_MEDKIT",
    "SFE_GRENADE",
    "SFE_CTFFLAG",
};
static const int inv_names_count = sizeof(inv_names) / sizeof(inv_names[0]);

static const char *weapon_index_to_string(int idx)
{
    if (idx >= 0 && idx < weapon_names_count) return weapon_names[idx];
    return "UNKNOWN_WEAPON";
}

static const char *invtype_index_to_string(int idx)
{
    if (idx >= 0 && idx < inv_names_count) return inv_names[idx];
    return "UNKNOWN_INVTYPE";
}

// Damage location and direction name helpers
static const char *damage_loc_names[] = {
    "CL_HEAD",
    "CL_L_ARM",
    "CL_L_LEG",
    "CL_R_ARM",
    "CL_R_LEG",
    "CL_TORSO",
};
static const int damage_loc_names_count = sizeof(damage_loc_names) / sizeof(damage_loc_names[0]);

static const char *damage_loc_to_string(int v)
{
    if (v >= 0 && v < damage_loc_names_count) return damage_loc_names[v];
    return "UNKNOWN_DAMAGELoc";
}

static const char *damage_dir_to_string(int v)
{
    static char tmp[32];
    if (v == 0) return "NONE";
    if (v == CLDAM_T_BRACKET) return "TOP_BRACKET";
    if (v == CLDAM_B_BRACKET) return "BOTTOM_BRACKET";
    if (v == CLDAM_R_BRACKET) return "RIGHT_BRACKET";
    if (v == CLDAM_L_BRACKET) return "LEFT_BRACKET";
    if (v == CLDAM_ALL_BRACKET) return "ALL_BRACKETS";
    snprintf(tmp, sizeof(tmp), "0x%X", v);
    return tmp;
}

// NOTE: removed local cvar wrapper functions - use orig_Cvar_Get / orig_Cvar_Set2 directly

// CL_ParseFrame detour handler
/*
 when in spectator mode (pm_type 7):
    pmove origin changes when in spectator mode, because you are rotating around the player you are speccing.
    pmove_delta_angles change..(server changes these to move your view angle, local view angle added to it)
    player_viewangles0 (pitch) and player_viewangles1 (yaw) change too
    pmove_flags = PMF_NO_PREDICTION
    velocity does not update as you move
    player_viewoffset is 0,0,0

when not in spectator mode (pm_type 0):
    pmove origin changes
    player_viewoffset is 0,0,35.0f when standing, when crouching 0,0,0
    delta angles only change on respawn
    pmove_flags = PMF_ON_GROUND
    velocity updates as you move
    

#define	PMF_DUCKED			1
#define	PMF_JUMP_HELD		2
#define	PMF_ON_GROUND		4
#define	PMF_TIME_WATERJUMP	8	// pm_time is waterjump
#define	PMF_TIME_LAND		16	// pm_time is time before rejump
#define	PMF_TIME_TELEPORT	32	// pm_time is non-moving time
#define	PMF_FATIGUED		64	// player is fatigued and cannot run
#define PMF_NO_PREDICTION	128	// temporarily disables prediction (used for grappling hook)

// player_state_t->refdef flags
#define	RDF_UNDERWATER		0x01		// warp the screen as apropriate
#define RDF_NOWORLDMODEL	0x02		// used for player configuration screen
#define RDF_GOGGLES			0x04		// Low light goggles.

player_restartcount increases each time you respawn
player_remote* is related to remote camera views, it could be interesting to use them in the future.
  remote_id is the edict number of the remote camera
  remote_type is the type of remote camera

#define REMOTE_TYPE_FPS			0x00		// normal fps view -- WOW, it sucks that this is zero...
#define REMOTE_TYPE_TPS			0x01		// third person view
#define REMOTE_TYPE_LETTERBOX	0x02		// show's black boxes above and below (letterbox)
#define REMOTE_TYPE_CAMERA		0x04		// displays a camera graphic border
#define REMOTE_TYPE_NORMAL		0x08		// remote view, nothing fancy
#define REMOTE_TYPE_SNIPER		0x10		// sniper scope
*/
static void my_CL_ParseFrame_impl(void)
{

    frame_t *f = cl_frame_local;
    if (!f) return;
    if (!f->valid) return; // don't overwrite frame cvars when frame is not valid

    // set values directly using global cvar handles (minimal path)
    player_state_t *ps = &f->playerstate;


    if (ps->stats[STAT_DAMAGELOC] != 0 ) 
    {
        orig_Com_Printf("\005[Stats]Damage location: %d\n", ps->stats[STAT_DAMAGELOC]);
    }
    #if 0
    //Only this one seems to change
    //direction of the damage bracket on interface
    if (ps->stats[STAT_DAMAGEDIR] != 0 ) 
    {
        orig_Com_Printf("\005[Stats]Damage direction: %d\n", ps->stats[STAT_DAMAGEDIR]);
    }
    #endif
    if (ps->damageLoc != 0 ) 
    {
        orig_Com_Printf("\005[Frame]Damage location: %d\n", ps->damageLoc);
    }
    if (ps->damageDir != 0 ) 
    {
        orig_Com_Printf("\005[Frame]Damage direction: %d\n", ps->damageDir);
    }

    // frame
    if (prev_valid != (int)f->valid) setCvarInt(cv_frame_valid, (int)f->valid);
    if (prev_serverframe != f->serverframe) setCvarInt(cv_frame_serverframe, f->serverframe);
    if (prev_servertime != f->servertime) setCvarInt(cv_frame_servertime, f->servertime);
    if (prev_deltaframe != f->deltaframe) setCvarInt(cv_frame_deltaframe, f->deltaframe);
    if (prev_ghoultime != f->ghoultime) setCvarFloat(cv_frame_ghoultime, f->ghoultime);
    if (prev_num_entities != f->num_entities) setCvarInt(cv_frame_num_entities, f->num_entities);

    // playerstate pmove
    if (prev_pmove.pm_type != ps->pmove.pm_type) setCvarInt(cv_frame_playerstate_pmove_pm_type, (int)ps->pmove.pm_type);
    if (prev_pmove.origin[0] != ps->pmove.origin[0]) setCvarInt(cv_frame_playerstate_pmove_origin0, (int)ps->pmove.origin[0]);
    if (prev_pmove.origin[1] != ps->pmove.origin[1]) setCvarInt(cv_frame_playerstate_pmove_origin1, (int)ps->pmove.origin[1]);
    if (prev_pmove.origin[2] != ps->pmove.origin[2]) setCvarInt(cv_frame_playerstate_pmove_origin2, (int)ps->pmove.origin[2]);
    if (prev_playerstate.dmRank != ps->dmRank) setCvarByte(cv_frame_playerstate_dmRank, (unsigned char)ps->dmRank);
    if (prev_playerstate.spectatorId != ps->spectatorId) setCvarByte(cv_frame_playerstate_spectatorId, (unsigned char)ps->spectatorId);

    // additional player_state fields
    if (prev_playerstate.viewangles[0] != ps->viewangles[0]) setCvarFloat(cv_frame_player_viewangles0, ps->viewangles[0]);
    if (prev_playerstate.viewangles[1] != ps->viewangles[1]) setCvarFloat(cv_frame_player_viewangles1, ps->viewangles[1]);
    if (prev_playerstate.viewangles[2] != ps->viewangles[2]) setCvarFloat(cv_frame_player_viewangles2, ps->viewangles[2]);
    if (prev_playerstate.viewoffset[0] != ps->viewoffset[0]) setCvarFloat(cv_frame_player_viewoffset0, ps->viewoffset[0]);
    if (prev_playerstate.viewoffset[1] != ps->viewoffset[1]) setCvarFloat(cv_frame_player_viewoffset1, ps->viewoffset[1]);
    if (prev_playerstate.viewoffset[2] != ps->viewoffset[2]) setCvarFloat(cv_frame_player_viewoffset2, ps->viewoffset[2]);
    if (prev_playerstate.fov != ps->fov) setCvarFloat(cv_frame_player_fov, ps->fov);
    if (prev_playerstate.rdflags != ps->rdflags) setCvarInt(cv_frame_player_rdflags, ps->rdflags);
    if (prev_playerstate.soundID != ps->soundID) setCvarInt(cv_frame_player_soundID, ps->soundID);
    if (prev_playerstate.musicID != ps->musicID) setCvarByte(cv_frame_player_musicID, ps->musicID);
    if (prev_playerstate.damageLoc != ps->damageLoc) setCvarInt(cv_frame_player_damageLoc, ps->damageLoc);
    if (prev_playerstate.damageDir != ps->damageDir) setCvarInt(cv_frame_player_damageDir, ps->damageDir);
    // all stats
    for (int i=0;i<MAX_STATS;++i) {
        if (prev_playerstate.stats[i] == ps->stats[i]) continue;
        switch (i) {
            case STAT_WEAPON: {
                const char *wname = weapon_index_to_string(ps->stats[i]);
                setCvarString(cv_frame_player_stats[i], (char*)wname);
            } break;
            case STAT_INV_TYPE: {
                const char *invname = invtype_index_to_string(ps->stats[i]);
                setCvarString(cv_frame_player_stats[i], (char*)invname);
            } break;
            case STAT_FLAGS: {
                int v = ps->stats[i];
                char flagsbuf[128];
                int pos = 0;
                bool any = false;
                if (v & STAT_FLAG_SCOPE) { pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "scope"); any = true; }
                if (v & STAT_FLAG_INFOTICKER) { if (any) pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "|"); pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "infoticker"); any = true; }
                if (v & STAT_FLAG_WIND) { if (any) pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "|"); pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "wind"); any = true; }
                if (v & STAT_FLAG_MISSIONFAIL) { if (any) pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "|"); pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "missionfail"); any = true; }
                if (v & STAT_FLAG_HIT) { if (any) pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "|"); pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "hit"); any = true; }
                if (v & STAT_FLAG_TEAM) { if (any) pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "|"); pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "team"); any = true; }
                if (v & STAT_FLAG_COUNTDOWN) { if (any) pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "|"); pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "countdown"); any = true; }
                if (v & STAT_FLAG_MISSIONACC) { if (any) pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "|"); pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "missionacc"); any = true; }
                if (v & STAT_FLAG_MISSIONEXIT) { if (any) pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "|"); pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "missionexit"); any = true; }
                if (v & STAT_FLAG_SHOW_STEALTH) { if (any) pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "|"); pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "show_stealth"); any = true; }
                if (v & STAT_FLAG_OBJECTIVES) { if (any) pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "|"); pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "objectives"); any = true; }
                if (v & STAT_FLAG_HIDECROSSHAIR) { if (any) pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "|"); pos += snprintf(flagsbuf+pos, sizeof(flagsbuf)-pos, "hide_crosshair"); any = true; }
                if (!any) snprintf(flagsbuf, sizeof(flagsbuf), "none");
                setCvarString(cv_frame_player_stats[i], flagsbuf);
            } break;
            case STAT_LAYOUTS: {
                int v = ps->stats[i];
                char layoutbuf[32];
                bool s = (v & LAYOUT_SCOREBOARD) != 0;
                bool d = (v & LAYOUT_DMRANKING) != 0;
                if (s && d) snprintf(layoutbuf, sizeof(layoutbuf), "score|dm");
                else if (s) snprintf(layoutbuf, sizeof(layoutbuf), "score");
                else if (d) snprintf(layoutbuf, sizeof(layoutbuf), "dm");
                else snprintf(layoutbuf, sizeof(layoutbuf), "none");
                setCvarString(cv_frame_player_stats[i], layoutbuf);
            } break;
            case STAT_DAMAGELOC: {
                setCvarInt(cv_frame_player_stats[i], ps->stats[i]);
            } break;
            case STAT_DAMAGEDIR: {
                const char *ddir = damage_dir_to_string(ps->stats[i]);
                setCvarString(cv_frame_player_stats[i], (char*)ddir);
            } break;
            default:
                setCvarInt(cv_frame_player_stats[i], ps->stats[i]);
                break;
        }
    }
    // kick and weapon kick
    if (prev_playerstate.kick_angles[0] != ps->kick_angles[0]) setCvarFloat(cv_frame_player_kick_angles0, ps->kick_angles[0]);
    if (prev_playerstate.kick_angles[1] != ps->kick_angles[1]) setCvarFloat(cv_frame_player_kick_angles1, ps->kick_angles[1]);
    if (prev_playerstate.kick_angles[2] != ps->kick_angles[2]) setCvarFloat(cv_frame_player_kick_angles2, ps->kick_angles[2]);
    if (prev_playerstate.weaponkick_angles[0] != ps->weaponkick_angles[0]) setCvarFloat(cv_frame_player_weaponkick_angles0, ps->weaponkick_angles[0]);
    if (prev_playerstate.weaponkick_angles[1] != ps->weaponkick_angles[1]) setCvarFloat(cv_frame_player_weaponkick_angles1, ps->weaponkick_angles[1]);
    if (prev_playerstate.weaponkick_angles[2] != ps->weaponkick_angles[2]) setCvarFloat(cv_frame_player_weaponkick_angles2, ps->weaponkick_angles[2]);
    // remote view origin/angles
    if (prev_playerstate.remote_vieworigin[0] != ps->remote_vieworigin[0]) setCvarFloat(cv_frame_player_remote_vieworigin0, ps->remote_vieworigin[0]);
    if (prev_playerstate.remote_vieworigin[1] != ps->remote_vieworigin[1]) setCvarFloat(cv_frame_player_remote_vieworigin1, ps->remote_vieworigin[1]);
    if (prev_playerstate.remote_vieworigin[2] != ps->remote_vieworigin[2]) setCvarFloat(cv_frame_player_remote_vieworigin2, ps->remote_vieworigin[2]);
    if (prev_playerstate.remote_viewangles[0] != ps->remote_viewangles[0]) setCvarFloat(cv_frame_player_remote_viewangles0, ps->remote_viewangles[0]);
    if (prev_playerstate.remote_viewangles[1] != ps->remote_viewangles[1]) setCvarFloat(cv_frame_player_remote_viewangles1, ps->remote_viewangles[1]);
    if (prev_playerstate.remote_viewangles[2] != ps->remote_viewangles[2]) setCvarFloat(cv_frame_player_remote_viewangles2, ps->remote_viewangles[2]);
    if (prev_playerstate.dmRankedPlyrs != ps->dmRankedPlyrs) setCvarInt(cv_frame_player_dmRankedPlyrs, ps->dmRankedPlyrs);
    if (prev_playerstate.cinematicfreeze != ps->cinematicfreeze) setCvarByte(cv_frame_player_cinematicfreeze, ps->cinematicfreeze);
    // pmove fields
    if (prev_pmove.pm_type != ps->pmove.pm_type) setCvarInt(cv_frame_pmove_pm_type, (int)ps->pmove.pm_type);
    if (prev_pmove.origin[0] != ps->pmove.origin[0]) setCvarInt(cv_frame_pmove_origin0, (int)ps->pmove.origin[0]);
    if (prev_pmove.origin[1] != ps->pmove.origin[1]) setCvarInt(cv_frame_pmove_origin1, (int)ps->pmove.origin[1]);
    if (prev_pmove.origin[2] != ps->pmove.origin[2]) setCvarInt(cv_frame_pmove_origin2, (int)ps->pmove.origin[2]);
    if (prev_pmove.velocity[0] != ps->pmove.velocity[0]) setCvarInt(cv_frame_pmove_velocity0, (int)ps->pmove.velocity[0]);
    if (prev_pmove.velocity[1] != ps->pmove.velocity[1]) setCvarInt(cv_frame_pmove_velocity1, (int)ps->pmove.velocity[1]);
    if (prev_pmove.velocity[2] != ps->pmove.velocity[2]) setCvarInt(cv_frame_pmove_velocity2, (int)ps->pmove.velocity[2]);
    if (prev_pmove.pm_flags != ps->pmove.pm_flags) setCvarInt(cv_frame_pmove_pm_flags, (int)ps->pmove.pm_flags);
    if (prev_pmove.pm_time != ps->pmove.pm_time) setCvarInt(cv_frame_pmove_pm_time, (int)ps->pmove.pm_time);
    if (prev_pmove.gravity != ps->pmove.gravity) setCvarInt(cv_frame_pmove_gravity, (int)ps->pmove.gravity);
    if (prev_pmove.delta_angles[0] != ps->pmove.delta_angles[0]) setCvarInt(cv_frame_pmove_delta_angles0, (int)ps->pmove.delta_angles[0]);
    if (prev_pmove.delta_angles[1] != ps->pmove.delta_angles[1]) setCvarInt(cv_frame_pmove_delta_angles1, (int)ps->pmove.delta_angles[1]);
    if (prev_pmove.delta_angles[2] != ps->pmove.delta_angles[2]) setCvarInt(cv_frame_pmove_delta_angles2, (int)ps->pmove.delta_angles[2]);
    // remote/gun/bod
    if (prev_playerstate.remote_id != ps->remote_id) setCvarInt(cv_frame_player_remote_id, ps->remote_id);
    if (prev_playerstate.remote_type != ps->remote_type) {
        // map remote_type to descriptive string
        const char *rtype = "UNKNOWN";
        switch (ps->remote_type) {
            case 0: rtype = "REMOTE_TYPE_FPS"; break;
            case 1: rtype = "REMOTE_TYPE_TPS"; break;
            case 2: rtype = "REMOTE_TYPE_LETTERBOX"; break;
            case 4: rtype = "REMOTE_TYPE_CAMERA"; break;
            case 8: rtype = "REMOTE_TYPE_NORMAL"; break;
            case 16: rtype = "REMOTE_TYPE_SNIPER"; break;
            default: {
                static char tmp[16];
                snprintf(tmp,sizeof(tmp),"0x%X", ps->remote_type);
                rtype = tmp;
            }
        }
        setCvarString(cv_frame_player_remote_type, (char*)rtype);
    }
    if ((uintptr_t)prev_playerstate.gun != (uintptr_t)ps->gun) setCvarUnsignedInt(cv_frame_player_gun_ptr, (unsigned int)(uintptr_t)ps->gun);
    if (prev_playerstate.gunUUID != ps->gunUUID) setCvarInt(cv_frame_player_gunUUID, ps->gunUUID);
    if (prev_playerstate.gunType != ps->gunType) {
        // expose gunType as descriptive string
        const char *gtype = "UNKNOWN";
        if (ps->gunType >= 0 && ps->gunType < GUN_NUM_TYPES) {
            switch (ps->gunType) {
                case GUN_NONE: gtype = "GUN_NONE"; break;
                case GUN_KNIFE: gtype = "GUN_KNIFE"; break;
                case GUN_PISTOL2: gtype = "GUN_PISTOL2"; break;
                case GUN_PISTOL1: gtype = "GUN_PISTOL1"; break;
                case GUN_MACHINEPISTOL: gtype = "GUN_MACHINEPISTOL"; break;
                case GUN_ASSAULTRIFLE: gtype = "GUN_ASSAULTRIFLE"; break;
                case GUN_SNIPER: gtype = "GUN_SNIPER"; break;
                case GUN_SLUGGER: gtype = "GUN_SLUGGER"; break;
                case GUN_SHOTGUN: gtype = "GUN_SHOTGUN"; break;
                case GUN_MACHINEGUN: gtype = "GUN_MACHINEGUN"; break;
                case GUN_ROCKET: gtype = "GUN_ROCKET"; break;
                case GUN_MPG: gtype = "GUN_MPG"; break;
                case GUN_FLAMEGUN: gtype = "GUN_FLAMEGUN"; break;
                default: gtype = "UNKNOWN"; break;
            }
        }
        setCvarString(cv_frame_player_gunType, (char*)gtype);
    }
    if (prev_playerstate.gunClip != ps->gunClip) setCvarInt(cv_frame_player_gunClip, ps->gunClip);
    if (prev_playerstate.gunAmmo != ps->gunAmmo) setCvarInt(cv_frame_player_gunAmmo, ps->gunAmmo);
    if (prev_playerstate.gunReload != ps->gunReload) setCvarByte(cv_frame_player_gunReload, ps->gunReload);
    if (prev_playerstate.restart_count != ps->restart_count) setCvarByte(cv_frame_player_restart_count, ps->restart_count);
    if (prev_playerstate.buttons_inhibit != ps->buttons_inhibit) setCvarByte(cv_frame_player_buttons_inhibit, ps->buttons_inhibit);
    if ((uintptr_t)prev_playerstate.bod != (uintptr_t)ps->bod) setCvarUnsignedInt(cv_frame_player_bod_ptr, (unsigned int)(uintptr_t)ps->bod);
    if (prev_playerstate.bodUUID != ps->bodUUID) setCvarInt(cv_frame_player_bodUUID, ps->bodUUID);
    // blend
    if (prev_playerstate.blend[0] != ps->blend[0]) setCvarFloat(cv_frame_player_blend0, ps->blend[0]);
    if (prev_playerstate.blend[1] != ps->blend[1]) setCvarFloat(cv_frame_player_blend1, ps->blend[1]);
    if (prev_playerstate.blend[2] != ps->blend[2]) setCvarFloat(cv_frame_player_blend2, ps->blend[2]);
    if (prev_playerstate.blend[3] != ps->blend[3]) setCvarFloat(cv_frame_player_blend3, ps->blend[3]);
    // areabits (32 bytes -> expose as ints 0/1)
    for (int i=0;i<32;++i) {
        int bit = (f->areabits[i/8] & (1 << (i%8))) ? 1 : 0;
        int prevbit = (prev_areabits[i/8] & (1 << (i%8))) ? 1 : 0;
        if (bit != prevbit) setCvarInt(cv_frame_areabits[i], bit);
    }
    // parse_entities
    if (prev_parse_entities != f->parse_entities) setCvarInt(cv_frame_parse_entities, f->parse_entities);

    // save snapshots for change detection next frame: copy playerstate and pmove only
    memcpy(&prev_playerstate, ps, sizeof(player_state_t));
    memcpy(&prev_pmove, &ps->pmove, sizeof(pmove_state_t));
    memcpy(prev_areabits, f->areabits, sizeof(prev_areabits));
    prev_serverframe = f->serverframe;
    prev_servertime = f->servertime;
    prev_deltaframe = f->deltaframe;
    prev_ghoultime = f->ghoultime;
    prev_num_entities = f->num_entities;
    prev_parse_entities = f->parse_entities;
    prev_valid = (int)f->valid;
}

// pointer to original CL_ParseFrame trampoline returned by DetourCreate
static void (*orig_CL_ParseFrame)(void) = NULL;

// normal (non-naked) detour target
static void my_CL_ParseFrame(void)
{
    // call original first if available
    orig_CL_ParseFrame();
    // then run our implementation
    my_CL_ParseFrame_impl();
}

// CL_ReadPackets detour handler implementation
static void my_CL_ReadPackets_impl(void)
{
    // currently no-op: placeholder for future packet inspection
}

// pointer to original CL_ReadPackets trampoline returned by DetourCreate
static void (*orig_CL_ReadPackets)(void) = NULL;

// normal (non-naked) detour target for CL_ReadPackets
static void my_CL_ReadPackets(void)
{
    // call original first if available
    orig_CL_ReadPackets();
    // then run our implementation
    my_CL_ReadPackets_impl();
}

// Create core cvars early (called from my_FS_InitFilesystem)
void core_cvars_init(void)
{
    // ensure the commonly used frame cvars exist so config.cfg can set them
    cv_frame_valid = orig_Cvar_Get("frame_valid", "0", NULL, NULL);
    cv_frame_serverframe = orig_Cvar_Get("frame_serverframe", "0", NULL, NULL);
    cv_frame_servertime = orig_Cvar_Get("frame_servertime", "0", NULL, NULL);
    cv_frame_deltaframe = orig_Cvar_Get("frame_deltaframe", "0", NULL, NULL);
    cv_frame_ghoultime = orig_Cvar_Get("frame_ghoultime", "0", NULL, NULL);
    cv_frame_num_entities = orig_Cvar_Get("frame_num_entities", "0", NULL, NULL);
    cv_frame_playerstate_pmove_pm_type = orig_Cvar_Get("frame_playerstate_pmove_pm_type", "0", NULL, NULL);
    cv_frame_playerstate_pmove_origin0 = orig_Cvar_Get("frame_playerstate_pmove_origin0", "0", NULL, NULL);
    cv_frame_playerstate_pmove_origin1 = orig_Cvar_Get("frame_playerstate_pmove_origin1", "0", NULL, NULL);
    cv_frame_playerstate_pmove_origin2 = orig_Cvar_Get("frame_playerstate_pmove_origin2", "0", NULL, NULL);
    cv_frame_playerstate_dmRank = orig_Cvar_Get("frame_playerstate_dmRank", "0", NULL, NULL);
    cv_frame_playerstate_spectatorId = orig_Cvar_Get("frame_playerstate_spectatorId", "0", NULL, NULL);
    // additional player_state cvars
    cv_frame_player_viewangles0 = orig_Cvar_Get("frame_player_viewangles0", "0", NULL, NULL);
    cv_frame_player_viewangles1 = orig_Cvar_Get("frame_player_viewangles1", "0", NULL, NULL);
    cv_frame_player_viewangles2 = orig_Cvar_Get("frame_player_viewangles2", "0", NULL, NULL);
    cv_frame_player_viewoffset0 = orig_Cvar_Get("frame_player_viewoffset0", "0", NULL, NULL);
    cv_frame_player_viewoffset1 = orig_Cvar_Get("frame_player_viewoffset1", "0", NULL, NULL);
    cv_frame_player_viewoffset2 = orig_Cvar_Get("frame_player_viewoffset2", "0", NULL, NULL);
    cv_frame_player_fov = orig_Cvar_Get("frame_player_fov", "0", NULL, NULL);
    cv_frame_player_rdflags = orig_Cvar_Get("frame_player_rdflags", "0", NULL, NULL);
    cv_frame_player_soundID = orig_Cvar_Get("frame_player_soundID", "0", NULL, NULL);
    cv_frame_player_musicID = orig_Cvar_Get("frame_player_musicID", "0", NULL, NULL);
    cv_frame_player_damageLoc = orig_Cvar_Get("frame_player_damageLoc", "0", NULL, NULL);
    cv_frame_player_damageDir = orig_Cvar_Get("frame_player_damageDir", "0", NULL, NULL);
    
    // pmove fields
    cv_frame_pmove_pm_type = orig_Cvar_Get("frame_pmove_pm_type", "0", NULL, NULL);
    cv_frame_pmove_origin0 = orig_Cvar_Get("frame_pmove_origin0", "0", NULL, NULL);
    cv_frame_pmove_origin1 = orig_Cvar_Get("frame_pmove_origin1", "0", NULL, NULL);
    cv_frame_pmove_origin2 = orig_Cvar_Get("frame_pmove_origin2", "0", NULL, NULL);
    cv_frame_pmove_velocity0 = orig_Cvar_Get("frame_pmove_velocity0", "0", NULL, NULL);
    cv_frame_pmove_velocity1 = orig_Cvar_Get("frame_pmove_velocity1", "0", NULL, NULL);
    cv_frame_pmove_velocity2 = orig_Cvar_Get("frame_pmove_velocity2", "0", NULL, NULL);
    cv_frame_pmove_pm_flags = orig_Cvar_Get("frame_pmove_pm_flags", "0", NULL, NULL);
    cv_frame_pmove_pm_time = orig_Cvar_Get("frame_pmove_pm_time", "0", NULL, NULL);
    cv_frame_pmove_gravity = orig_Cvar_Get("frame_pmove_gravity", "0", NULL, NULL);
    cv_frame_pmove_delta_angles0 = orig_Cvar_Get("frame_pmove_delta_angles0", "0", NULL, NULL);
    cv_frame_pmove_delta_angles1 = orig_Cvar_Get("frame_pmove_delta_angles1", "0", NULL, NULL);
    cv_frame_pmove_delta_angles2 = orig_Cvar_Get("frame_pmove_delta_angles2", "0", NULL, NULL);
    // player remote/gun/bod
    cv_frame_player_remote_id = orig_Cvar_Get("frame_player_remote_id", "0", NULL, NULL);
    cv_frame_player_remote_type = orig_Cvar_Get("frame_player_remote_type", "0", NULL, NULL);
    cv_frame_player_gun_ptr = orig_Cvar_Get("frame_player_gun_ptr", "0", NULL, NULL);
    cv_frame_player_gunUUID = orig_Cvar_Get("frame_player_gunUUID", "0", NULL, NULL);
    cv_frame_player_gunType = orig_Cvar_Get("frame_player_gunType", "0", NULL, NULL);
    cv_frame_player_gunClip = orig_Cvar_Get("frame_player_gunClip", "0", NULL, NULL);
    cv_frame_player_gunAmmo = orig_Cvar_Get("frame_player_gunAmmo", "0", NULL, NULL);
    cv_frame_player_gunReload = orig_Cvar_Get("frame_player_gunReload", "0", NULL, NULL);
    cv_frame_player_restart_count = orig_Cvar_Get("frame_player_restart_count", "0", NULL, NULL);
    cv_frame_player_buttons_inhibit = orig_Cvar_Get("frame_player_buttons_inhibit", "0", NULL, NULL);
    cv_frame_player_bod_ptr = orig_Cvar_Get("frame_player_bod_ptr", "0", NULL, NULL);
    cv_frame_player_bodUUID = orig_Cvar_Get("frame_player_bodUUID", "0", NULL, NULL);
    // blend
    cv_frame_player_blend0 = orig_Cvar_Get("frame_player_blend0", "0", NULL, NULL);
    cv_frame_player_blend1 = orig_Cvar_Get("frame_player_blend1", "0", NULL, NULL);
    cv_frame_player_blend2 = orig_Cvar_Get("frame_player_blend2", "0", NULL, NULL);
    cv_frame_player_blend3 = orig_Cvar_Get("frame_player_blend3", "0", NULL, NULL);
    // areabits
    for (int i=0;i<32;++i) {
        char name[32];
        snprintf(name,sizeof(name),"frame_areabits%d",i);
        cv_frame_areabits[i] = orig_Cvar_Get(name, "0", NULL, NULL);
    }
    // stats
    // Map known STAT_* indices to descriptive cvar names for easier use
    for (int i=0;i<MAX_STATS;++i) {
        char name[48];
        switch (i) {
            case STAT_INV_TYPE:    snprintf(name,sizeof(name),"frame_player_stats_inv_type"); break;
            case STAT_HEALTH:      snprintf(name,sizeof(name),"frame_player_stats_health"); break;
            case STAT_CLIP_AMMO:   snprintf(name,sizeof(name),"frame_player_stats_clip_ammo"); break;
            case STAT_AMMO:        snprintf(name,sizeof(name),"frame_player_stats_ammo"); break;
            case STAT_CLIP_MAX:    snprintf(name,sizeof(name),"frame_player_stats_clip_max"); break;
            case STAT_ARMOR:       snprintf(name,sizeof(name),"frame_player_stats_armor"); break;
            case STAT_WEAPON:      snprintf(name,sizeof(name),"frame_player_stats_weapon"); break;
            case STAT_INV_COUNT:   snprintf(name,sizeof(name),"frame_player_stats_inv_count"); break;
            case STAT_FLAGS:       snprintf(name,sizeof(name),"frame_player_stats_flags"); break;
            case STAT_LAYOUTS:     snprintf(name,sizeof(name),"frame_player_stats_layouts"); break;
            case STAT_FRAGS:       snprintf(name,sizeof(name),"frame_player_stats_frags"); break;
            case STAT_STEALTH:     snprintf(name,sizeof(name),"frame_player_stats_stealth"); break;
            case STAT_FORCEHUD:    snprintf(name,sizeof(name),"frame_player_stats_forcehud"); break;
            case STAT_CASH3:       snprintf(name,sizeof(name),"frame_player_stats_cash3"); break;
            case STAT_DAMAGELOC:   snprintf(name,sizeof(name),"frame_player_stats_damageloc"); break;
            case STAT_DAMAGEDIR:   snprintf(name,sizeof(name),"frame_player_stats_damagedir"); break;
            default:               snprintf(name,sizeof(name),"frame_player_stats%d",i); break;
        }
        cv_frame_player_stats[i] = orig_Cvar_Get(name, "0", NULL, NULL);
    }
    // additional cvars
    cv_frame_player_kick_angles0 = orig_Cvar_Get("frame_player_kick_angles0","0",NULL,NULL);
    cv_frame_player_kick_angles1 = orig_Cvar_Get("frame_player_kick_angles1","0",NULL,NULL);
    cv_frame_player_kick_angles2 = orig_Cvar_Get("frame_player_kick_angles2","0",NULL,NULL);
    cv_frame_player_weaponkick_angles0 = orig_Cvar_Get("frame_player_weaponkick_angles0","0",NULL,NULL);
    cv_frame_player_weaponkick_angles1 = orig_Cvar_Get("frame_player_weaponkick_angles1","0",NULL,NULL);
    cv_frame_player_weaponkick_angles2 = orig_Cvar_Get("frame_player_weaponkick_angles2","0",NULL,NULL);
    cv_frame_player_remote_vieworigin0 = orig_Cvar_Get("frame_player_remote_vieworigin0","0",NULL,NULL);
    cv_frame_player_remote_vieworigin1 = orig_Cvar_Get("frame_player_remote_vieworigin1","0",NULL,NULL);
    cv_frame_player_remote_vieworigin2 = orig_Cvar_Get("frame_player_remote_vieworigin2","0",NULL,NULL);
    cv_frame_player_remote_viewangles0 = orig_Cvar_Get("frame_player_remote_viewangles0","0",NULL,NULL);
    cv_frame_player_remote_viewangles1 = orig_Cvar_Get("frame_player_remote_viewangles1","0",NULL,NULL);
    cv_frame_player_remote_viewangles2 = orig_Cvar_Get("frame_player_remote_viewangles2","0",NULL,NULL);
    cv_frame_player_dmRankedPlyrs = orig_Cvar_Get("frame_player_dmRankedPlyrs","0",NULL,NULL);
    cv_frame_player_cinematicfreeze = orig_Cvar_Get("frame_player_cinematicfreeze","0",NULL,NULL);
    cv_frame_parse_entities = orig_Cvar_Get("frame_parse_entities","0",NULL,NULL);
}

// Called from sof_buddy early init to install hooks
void core_early(void)
{
    if (!orig_CL_ParseFrame) {
        orig_CL_ParseFrame = (decltype(orig_CL_ParseFrame))DetourCreate((void*)0x20002E90, (void*)&my_CL_ParseFrame, DETOUR_TYPE_JMP, 8);
    }
    if (!orig_CL_ReadPackets) {
        orig_CL_ReadPackets = (decltype(orig_CL_ReadPackets))DetourCreate((void*)0x2000C5C0, (void*)&my_CL_ReadPackets, DETOUR_TYPE_JMP, 8);
    }
    // install SCR_UpdateScreen detour as well
    install_scr_update_detour();
}


