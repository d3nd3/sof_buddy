#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "ev_internal.h"
#include "generated_detours.h"
#include "util.h"

namespace ev {

namespace detail {

// Canonical identity for wireframe dedupe after EvFinalizeStudyBboxForWireframe.
// Use **world** AABB corners (origin+rel), not origin and rel separately — cache snapshots vs live edict reads can
// differ slightly in float bits for the same physical box, so mill-quantized origin|mins|maxs produced two keys
// and cache+live both drew (seen on ctf_flag_*, info_player_team*). 1/8-unit quantization matches typical grid.
static std::string WireframeDedupeKey(char const* classname, float const* origin, float const* mins,
                                      float const* maxs) {
    auto q8 = [](float f) -> long long {
        return static_cast<long long>(std::llround(static_cast<double>(f) * 8.0));
    };
    float const wmin0 = origin[0] + mins[0];
    float const wmin1 = origin[1] + mins[1];
    float const wmin2 = origin[2] + mins[2];
    float const wmax0 = origin[0] + maxs[0];
    float const wmax1 = origin[1] + maxs[1];
    float const wmax2 = origin[2] + maxs[2];
    char buf[640];
    std::snprintf(buf, sizeof buf, "%s|%lld,%lld,%lld|%lld,%lld,%lld", classname ? classname : "", q8(wmin0),
                  q8(wmin1), q8(wmin2), q8(wmax0), q8(wmax1), q8(wmax2));
    return std::string(buf);
}

static const float kPaletteRgb[] = {
    0, 0, 0,         // 0 func_* (black)
    255, 0, 127,     // 1
    255, 0, 255,     // 2
    127, 0, 255,     // 3
    0, 0, 255,       // 4
    0, 127, 255,     // 5
    0, 255, 255,     // 6 item_* (cyan)
    0, 255, 127,     // 7
    0, 255, 0,       // 8
    127, 255, 0,     // 9
    255, 255, 255,   // 10 misc_* (white)
    255, 127, 0,     // 11 (reserved / legacy; not used for class map)
    255, 255, 0,     // 12 light* (yellow)
    72,  170, 255,   // 13 info_player_team1 (CTF blue-forward)
    255, 72,  72     // 14 info_player_team2 (CTF red-forward)
};

static inline unsigned PackPaletteRgb(int nColor) {
    if (nColor < 0) {
        nColor = 0;
    }
    if (nColor > 14) {
        nColor = 14;
    }
    const int i = nColor * 3;
    const unsigned char r = static_cast<unsigned char>(kPaletteRgb[i]);
    const unsigned char g = static_cast<unsigned char>(kPaletteRgb[i + 1]);
    const unsigned char b = static_cast<unsigned char>(kPaletteRgb[i + 2]);
    const unsigned char a = 255;
    return static_cast<unsigned>(r) | (static_cast<unsigned>(g) << 8u) |
           (static_cast<unsigned>(b) << 16u) | (static_cast<unsigned>(a) << 24u);
}

static bool FxApisOk() {
    return detour_FX_AllocNewQuad::oFX_AllocNewQuad && detour_FX_SetQuadAsLine::oFX_SetQuadAsLine &&
           detour_FX_SetQuadLineGenericInfo::oFX_SetQuadLineGenericInfo;
}

static void EvFxClearWireframePoolBeforeRedraw() {
    if (detour_FX_ClearQuads::oFX_ClearQuads) {
        detour_FX_ClearQuads::oFX_ClearQuads();
    }
}

static bool EntitiesDrawVerbose() {
    return _sofbuddy_entities_draw_verbose && _sofbuddy_entities_draw_verbose->value != 0.f;
}

static void EvFmtClassnameForLog(char const* cn, char* out, size_t outsz) {
    if (!outsz) {
        return;
    }
    out[0] = '\0';
    if (cn && cn[0]) {
        std::strncpy(out, cn, outsz - 1u);
        out[outsz - 1u] = '\0';
    } else {
        std::strncpy(out, "<none>", outsz - 1u);
        out[outsz - 1u] = '\0';
    }
}

static void LogWireframeSkip(char const* trace_kind, int trace_index, char const* reason, char const* cn,
                             float ox, float oy, float oz, float const* rel_mins, float const* rel_maxs,
                             char const* extra) {
    if (!EntitiesDrawVerbose() || !trace_kind) {
        return;
    }
    char cbuf[72];
    EvFmtClassnameForLog(cn, cbuf, sizeof(cbuf));
    PrintOut(PRINT_DEV,
             "[entity_visualizer] wireframe SKIP %s[%d] reason=%s cn=%s o=(%.2f,%.2f,%.2f) rel_mins=(%.2f,%.2f,%.2f) "
             "rel_maxs=(%.2f,%.2f,%.2f) %s\n",
             trace_kind, trace_index, reason, cbuf, static_cast<double>(ox), static_cast<double>(oy),
             static_cast<double>(oz), static_cast<double>(rel_mins[0]), static_cast<double>(rel_mins[1]),
             static_cast<double>(rel_mins[2]), static_cast<double>(rel_maxs[0]), static_cast<double>(rel_maxs[1]),
             static_cast<double>(rel_maxs[2]), extra ? extra : "");
}

static void LogWireframeOk(char const* trace_kind, int trace_index, char const* cn, float const* origin,
                           float const* rel_mins, float const* rel_maxs, char const* draw_desc) {
    if (!EntitiesDrawVerbose() || !trace_kind) {
        return;
    }
    char cbuf[72];
    EvFmtClassnameForLog(cn, cbuf, sizeof(cbuf));
    float const wmin0 = origin[0] + rel_mins[0];
    float const wmin1 = origin[1] + rel_mins[1];
    float const wmin2 = origin[2] + rel_mins[2];
    float const wmax0 = origin[0] + rel_maxs[0];
    float const wmax1 = origin[1] + rel_maxs[1];
    float const wmax2 = origin[2] + rel_maxs[2];
    PrintOut(PRINT_DEV,
             "[entity_visualizer] wireframe OK %s[%d] cn=%s o=(%.2f,%.2f,%.2f) wmin=(%.2f,%.2f,%.2f) "
             "wmax=(%.2f,%.2f,%.2f) draw=%s\n",
             trace_kind, trace_index, cbuf, static_cast<double>(origin[0]), static_cast<double>(origin[1]),
             static_cast<double>(origin[2]), static_cast<double>(wmin0), static_cast<double>(wmin1),
             static_cast<double>(wmin2), static_cast<double>(wmax0), static_cast<double>(wmax1),
             static_cast<double>(wmax2), draw_desc ? draw_desc : "?");
}

// FX line quads: lifetime arg to SetQuadLineGenericInfo / +6C expire (milliseconds → seconds in-engine).
static constexpr int kEvFxWireLifetimeMs = 600000; // 10 minutes

// cquad+34 is a char* into ref_gl (GL_FindImage). Use a path + imagetype that actually loads so ref_gl
// does not spam "Can't load" once per quad (conchars.pcx is often absent in SoF retail).
static char s_evFxLineImageName[80] = "pics/interface2/health";
static bool s_evFxLineImageResolved = false;

static char* EvFxLineImageNameForQuad() {
    if (s_evFxLineImageResolved) {
        return s_evFxLineImageName;
    }
    if (!detour_GL_FindImage::oGL_FindImage) {
        return s_evFxLineImageName;
    }
    struct Try {
        char const* path;
        int type;
    };
    // HUD paths are loaded early in SoF; try several types. Then Q2-style fallbacks.
    static Try const kTries[] = {
        {"pics/interface2/health", kGlImagePic},
        {"pics/interface2/health", kGlImageWall},
        {"pics/interface2/armor", kGlImagePic},
        {"pics/interface2/armor", kGlImageWall},
        {"pics/interface2/frame_health", kGlImagePic},
        {"pics/interface2/frame_health", kGlImageWall},
        {"pics/conchars.pcx", kGlImagePic},
        {"pics/conchars.pcx", kGlImageWall},
        {"pics/conchars.pcx", kGlImageSprite},
        {"pics/conchars.pcx", kGlImageSkin},
        {"pics/conchars.pcx", kGlImageFont},
        {"pics/chasers.pcx", kGlImagePic},
        {"pics/chasers.pcx", kGlImageWall},
    };
    char buf[80];
    for (Try const& t : kTries) {
        std::strncpy(buf, t.path, sizeof(buf) - 1u);
        buf[sizeof(buf) - 1u] = '\0';
        if (detour_GL_FindImage::oGL_FindImage(buf, t.type, 1, 0)) {
            std::strncpy(s_evFxLineImageName, t.path, sizeof(s_evFxLineImageName) - 1u);
            s_evFxLineImageName[sizeof(s_evFxLineImageName) - 1u] = '\0';
            s_evFxLineImageResolved = true;
            return s_evFxLineImageName;
        }
    }
    std::strncpy(s_evFxLineImageName, "pics/interface2/health", sizeof(s_evFxLineImageName) - 1u);
    s_evFxLineImageName[sizeof(s_evFxLineImageName) - 1u] = '\0';
    s_evFxLineImageResolved = true;
    return s_evFxLineImageName;
}

// Live FX clock for quad expire: alloc puts &fxCurTime at +8C and +68; line quads often leave +8C as the pointer
// but repurpose +68 as a float — dereferencing +68 faults (e.g. fadds (%ecx), ecx=0x410adeed float bits).
static float ReadFxCurTimeForExpire(unsigned char* qb) {
    auto try_via_ptr = [](uint32_t raw, float* out) -> bool {
        if (raw >= kSofExeLoadBase && raw < kSofExeLoadEndExclusive && (raw & 3u) == 0u) {
            *out = *reinterpret_cast<float*>(static_cast<uintptr_t>(raw));
            return true;
        }
        return false;
    };
    uint32_t raw8c = 0;
    std::memcpy(&raw8c, qb + kCQuadFxCurTimePtrDupOffset, sizeof(raw8c));
    float t = 0.f;
    if (try_via_ptr(raw8c, &t)) {
        return t;
    }
    uint32_t raw68 = 0;
    std::memcpy(&raw68, qb + kCQuadFxCurTimePtrOffset, sizeof(raw68));
    if (try_via_ptr(raw68, &t)) {
        return t;
    }
    std::memcpy(&t, &raw68, sizeof(t));
    return t;
}

static char const* FxNowSourceLabel(uint32_t raw8c, uint32_t raw68) {
    if (raw8c >= kSofExeLoadBase && raw8c < kSofExeLoadEndExclusive && (raw8c & 3u) == 0u) {
        return "deref+8C";
    }
    if (raw68 >= kSofExeLoadBase && raw68 < kSofExeLoadEndExclusive && (raw68 & 3u) == 0u) {
        return "deref+68";
    }
    return "float@68";
}

// Same FX sequence as client HandleTestLine (TE_TEST_LINE), without net_message.
// IDA SoF.exe HandleTestLine__Fi: FX_SetQuadLineGenericInfo gets p2=p3=null, tail int=0; only the low
// byte is written to cquad+0xD4 — passing lifetime as that int corrupts state (e.g. 10000 -> 0x10).
static void EmitFxTestLine(float const* c1, float const* c2, unsigned packedColor, int lifetime,
                           char* shader_name, bool ev_dev_log_line) {
    void* quad = nullptr;
    if (!detour_FX_AllocNewQuad::oFX_AllocNewQuad(&quad) || !quad) {
        if (ev_dev_log_line) {
            PrintOut(PRINT_DEV, "ev_fx: FX_AllocNewQuad failed (null or 0)\n");
        }
        return;
    }
    unsigned char r = static_cast<unsigned char>(packedColor & 0xFFu);
    unsigned char g = static_cast<unsigned char>((packedColor >> 8) & 0xFFu);
    unsigned char b = static_cast<unsigned char>((packedColor >> 16) & 0xFFu);
    unsigned char const a = static_cast<unsigned char>((packedColor >> 24) & 0xFFu);
    // ref_gl multiplies vertex color with the line texture sample (GL_MODULATE). pics/conchars.pcx / UI
    // textures are often dark; saturated colors (e.g. cyan 0,255,255) become muddy. Blend ~30% toward white
    // before SetQuadAsLine so class hues stay readable without changing palette indices. Skip lift for pure
    // black (func_* palette 0) so it stays black instead of gray.
    if (r != 0 || g != 0 || b != 0) {
        float const t = 0.30f;
        auto lift = [t](unsigned char c) -> unsigned char {
            float const x = static_cast<float>(c) * (1.f - t) + 255.f * t;
            int o = static_cast<int>(x + 0.5f);
            if (o > 255) {
                o = 255;
            }
            return static_cast<unsigned char>(o);
        };
        r = lift(r);
        g = lift(g);
        b = lift(b);
    }
    float pa[3];
    float pb[3];
    std::memcpy(pa, c1, sizeof(pa));
    std::memcpy(pb, c2, sizeof(pb));
    detour_FX_SetQuadAsLine::oFX_SetQuadAsLine(quad, pa, pb, r, g, b, a, 1.0f);
    // Lifetime from FX_MakeLine is a long; engine scales it for the third float (see HandleTestLine FPU).
    float const lifeF = static_cast<float>(lifetime) * (1.0f / 1000.0f);
    detour_FX_SetQuadLineGenericInfo::oFX_SetQuadLineGenericInfo(quad, pa, pb, nullptr, nullptr, 1.0f, 1.0f,
                                                                   lifeF, 0);

    // FX_AddQuads: fld [quad+6Ch]; fcomp live fxCurTime — expire must be > fxCurTime or the quad is dropped.
    auto* qb = reinterpret_cast<unsigned char*>(quad);
    float const now = ReadFxCurTimeForExpire(qb);
    float const durSec = static_cast<float>(lifetime) * (1.0f / 1000.0f);
    float const expire = now + (durSec > 0.f ? durSec : 1.f);
    *reinterpret_cast<float*>(qb + kCQuadExpireTimeOffset) = expire;
    // FX_GenericQuadLineUpdate (installed at +70 by SetQuadLineGenericInfo) clears +6C on several paths before
    // the next FX_AddQuads, so quads never reach V_AddQuad. Alloc leaves +70 NULL; drop think for static lines.
    *reinterpret_cast<void**>(qb + kCQuadThinkFnOffset) = nullptr;
    // FX_AddQuads skips V_AddQuad if [quad+34]==0; ref_gl passes this to GL_FindImage (see README).
    *reinterpret_cast<char**>(qb + kCQuadShaderNamePtrOffset) = EvFxLineImageNameForQuad();
    if (ev_dev_log_line) {
        uint32_t raw8c = 0;
        uint32_t raw68 = 0;
        std::memcpy(&raw8c, qb + kCQuadFxCurTimePtrDupOffset, sizeof(raw8c));
        std::memcpy(&raw68, qb + kCQuadFxCurTimePtrOffset, sizeof(raw68));
        PrintOut(PRINT_DEV,
                 "ev_fx line[0]: quad=%p src=%s raw8c=0x%08X raw68=0x%08X now=%.4f expire=%.4f dur=%.4fs "
                 "life_ms=%d seg=(%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f)\n",
                 quad, FxNowSourceLabel(raw8c, raw68), static_cast<unsigned>(raw8c),
                 static_cast<unsigned>(raw68), static_cast<double>(now), static_cast<double>(expire),
                 static_cast<double>(durSec), lifetime, static_cast<double>(c1[0]), static_cast<double>(c1[1]),
                 static_cast<double>(c1[2]), static_cast<double>(c2[0]), static_cast<double>(c2[1]),
                 static_cast<double>(c2[2]));
    }
}

static void DebugDrawBox2(void* self, float const* vOrigin, float const* vMins, float const* vMaxs, int nColor,
                          int exceptWho, bool ev_dev_log = false) {
    (void)exceptWho;
    if (!FxApisOk()) {
        if (ev_dev_log) {
            PrintOut(PRINT_DEV, "ev_fx box: FX detours not ready\n");
        }
        return;
    }
    float mns[3];
    float mxs[3];
    unsigned char* ent = reinterpret_cast<unsigned char*>(self);
    if (self) {
        std::memcpy(mns, ent + kEdictMinsOffset, sizeof(mns));
        std::memcpy(mxs, ent + kEdictMaxsOffset, sizeof(mxs));
    } else {
        if (!vMins || !vMaxs) {
            return;
        }
        std::memcpy(mns, vMins, sizeof(mns));
        std::memcpy(mxs, vMaxs, sizeof(mxs));
    }

    float corners[8][3] = {
        {mns[0], mns[1], mns[2]},
        {mns[0], mns[1], mxs[2]},
        {mns[0], mxs[1], mns[2]},
        {mns[0], mxs[1], mxs[2]},
        {mxs[0], mns[1], mns[2]},
        {mxs[0], mns[1], mxs[2]},
        {mxs[0], mxs[1], mns[2]},
        {mxs[0], mxs[1], mxs[2]},
    };

    float vO[3];
    if (self) {
        std::memcpy(vO, ent + kEdictSOriginOffset, sizeof(vO));
    } else {
        if (!vOrigin) {
            return;
        }
        vO[0] = vOrigin[0];
        vO[1] = vOrigin[1];
        vO[2] = vOrigin[2];
    }

    unsigned const packed = PackPaletteRgb(nColor);

    if (ev_dev_log) {
        PrintOut(PRINT_DEV,
                 "ev_fx box: color=%d origin=(%.2f %.2f %.2f) mins=(%.2f %.2f %.2f) maxs=(%.2f %.2f %.2f) "
                 "self_ent=%p\n",
                 nColor, static_cast<double>(vO[0]), static_cast<double>(vO[1]), static_cast<double>(vO[2]),
                 static_cast<double>(mns[0]), static_cast<double>(mns[1]), static_cast<double>(mns[2]),
                 static_cast<double>(mxs[0]), static_cast<double>(mxs[1]), static_cast<double>(mxs[2]), self);
    }

    float wc[8][3];
    for (int k = 0; k < 8; ++k) {
        wc[k][0] = vO[0] + corners[k][0];
        wc[k][1] = vO[1] + corners[k][1];
        wc[k][2] = vO[2] + corners[k][2];
    }

    // Four space diagonals (opposite corners); all meet at the 3D box center — 4 line quads vs 12 cube edges.
    static constexpr int kCubeSpaceDiagonals[4][2] = {
        {0, 7},
        {1, 6},
        {2, 5},
        {3, 4},
    };
    char* const line_shader = EvFxLineImageNameForQuad();
    for (int e = 0; e < 4; ++e) {
        int const ia = kCubeSpaceDiagonals[e][0];
        int const ib = kCubeSpaceDiagonals[e][1];

        EmitFxTestLine(wc[ia], wc[ib], packed, kEvFxWireLifetimeMs, line_shader, ev_dev_log && e == 0);
    }
    if (ev_dev_log) {
        PrintOut(PRINT_DEV, "ev_fx box: emitted 4 TE_TEST_LINE-style quads (space diagonals through box center)\n");
    }
}

static int GetPlayerSlotFromEnt(void* ent) {
    unsigned const base = stget_u32(kEdictsBasePtrAddr);
    if (!base || !ent) {
        return -1;
    }
    ptrdiff_t const diff =
        reinterpret_cast<unsigned char*>(ent) - reinterpret_cast<unsigned char*>(base);
    if (diff < 0 || static_cast<unsigned>(diff) % kSizeOfEdict != 0) {
        return -1;
    }
    unsigned const idx = static_cast<unsigned>(diff / kSizeOfEdict);
    if (idx == 0) {
        return -1;
    }
    void* gcl = *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(ent) + kEdictGClientOffset);
    if (!gcl) {
        return -1;
    }
    return static_cast<int>(idx) - 1;
}

static int MaxClients() {
    if (!detour_Cvar_Get::oCvar_Get) {
        return 16;
    }
    static cvar_t* s_mc = nullptr;
    if (!s_mc) {
        s_mc = detour_Cvar_Get::oCvar_Get("maxclients", "8", 0, nullptr);
    }
    return s_mc ? static_cast<int>(s_mc->value) : 16;
}

// Palette roles (kPaletteRgb 0..14): 0 func_* (black) 1 world/path/script/_region + dm_KOTH_* + test_*
// 2 environ_* 3 spawner* 4 player* 5 target_* 6 item_* (cyan) 7 info_* (except team spawns below) 8 trigger_* 9 m_* 10 misc_* (white) 12 light* (yellow) 13–14 CTF team spawns
//
// Map SoF classnames to those indices. Prefix order matters: misc_ before m_*; all `light*` share slot 12.
// Covers typical ents: info_/player spawns, environ_ FX, trigger_/func_ brush, item_/misc_, target_ helpers,
// light / light_generic_* / any light* (see g_obj.cpp strncmp "light",5), spawner*, m_* monsters, map glue.
static bool ColorForSoFClassname(char const* cn, int self_player_slot, int* out_color, int* out_except_who) {
    if (!cn || !cn[0] || !out_color || !out_except_who) {
        return false;
    }
    *out_except_who = 0;

    if (std::strncmp(cn, "player", 6) == 0) {
        *out_color = 4;
        *out_except_who = (self_player_slot >= 0) ? (self_player_slot + 1) : 0;
        return true;
    }
    if (std::strcmp(cn, "info_player_team1") == 0) {
        *out_color = 13;
        return true;
    }
    if (std::strcmp(cn, "info_player_team2") == 0) {
        *out_color = 14;
        return true;
    }
    if (std::strncmp(cn, "info_", 5) == 0) {
        *out_color = 7;
        return true;
    }
    if (std::strncmp(cn, "environ_", 8) == 0) {
        *out_color = 2;
        return true;
    }
    if (std::strncmp(cn, "light", 5) == 0) {
        *out_color = 12;
        return true;
    }
    if (std::strncmp(cn, "trigger_", 8) == 0) {
        *out_color = 8;
        return true;
    }
    if (std::strncmp(cn, "func_", 5) == 0) {
        *out_color = 0;
        return true;
    }
    if (std::strncmp(cn, "item_", 5) == 0) {
        *out_color = 6;
        return true;
    }
    if (std::strncmp(cn, "misc_", 5) == 0) {
        *out_color = 10;
        return true;
    }
    if (std::strncmp(cn, "target_", 7) == 0) {
        *out_color = 5;
        return true;
    }
    if (std::strncmp(cn, "spawner", 7) == 0) {
        *out_color = 3;
        return true;
    }
    if (cn[0] == 'm' && cn[1] == '_') {
        *out_color = 9;
        return true;
    }
    if (std::strcmp(cn, "worldspawn") == 0 || std::strcmp(cn, "path_corner") == 0 ||
        std::strcmp(cn, "point_combat") == 0 || std::strcmp(cn, "script_runner") == 0 ||
        std::strcmp(cn, "_region") == 0) {
        *out_color = 1;
        return true;
    }
    if (std::strncmp(cn, "dm_KOTH_", 8) == 0) {
        *out_color = 1;
        return true;
    }
    if (std::strncmp(cn, "test_", 5) == 0) {
        *out_color = 1;
        return true;
    }

    return false;
}

// Geometry + classname + owner (for slot tint). self_player_slot is GetPlayerSlotFromEnt(ent) when classname is player*; else -1.
// trace_kind/trace_index: for PRINT_DEV when _sofbuddy_entities_draw_verbose (e.g. "cache", i; "live", edict_idx).
static void DrawBoxFromGeometry(float const* origin, float const* mins, float const* maxs, char const* classname,
                                uintptr_t owner_uint, int self_player_slot, char const* trace_kind, int trace_index) {
    bool const v = EntitiesDrawVerbose() && trace_kind;
    if (!FxApisOk()) {
        if (v) {
            LogWireframeSkip(trace_kind, trace_index, "fx_detours_not_ready", classname, origin[0], origin[1], origin[2],
                             mins, maxs, "");
        }
        return;
    }
    float mns[3];
    float mxs[3];
    std::memcpy(mns, mins, sizeof(mns));
    std::memcpy(mxs, maxs, sizeof(mxs));
    EvFinalizeStudyBboxForWireframe(origin, mns, mxs, classname);
    if (EvIsZeroBounds(origin, mns, mxs)) {
        if (v) {
            LogWireframeSkip(trace_kind, trace_index, "zero_bounds_after_finalize", classname, origin[0], origin[1],
                             origin[2], mns, mxs, "");
        }
        return;
    }

    int const maxc = MaxClients();
    void* const owner = owner_uint ? reinterpret_cast<void*>(owner_uint) : nullptr;
    char desc[48];

    if (classname && classname[0]) {
        int pal = 0;
        int exw = 0;
        if (ColorForSoFClassname(classname, self_player_slot, &pal, &exw)) {
            std::snprintf(desc, sizeof(desc), "classname_map pal=%d exw=%d", pal, exw);
            LogWireframeOk(trace_kind, trace_index, classname, origin, mns, mxs, desc);
            DebugDrawBox2(nullptr, origin, mns, mxs, pal, exw);
            return;
        }
        if (owner) {
            int const slot = GetPlayerSlotFromEnt(owner);
            if (slot != -1 && slot < maxc) {
                std::snprintf(desc, sizeof(desc), "owner_slot slot=%d", slot);
                LogWireframeOk(trace_kind, trace_index, classname, origin, mns, mxs, desc);
                DebugDrawBox2(nullptr, origin, mns, mxs, 1, slot + 1);
                return;
            }
        }
        LogWireframeOk(trace_kind, trace_index, classname, origin, mns, mxs, "default pal=1 exw=0");
        DebugDrawBox2(nullptr, origin, mns, mxs, 1, 0);
        return;
    }
    // No classname: owner must still draw — GetPlayerSlotFromEnt fails for non-player owners (world, monsters).
    if (owner) {
        int const slot = GetPlayerSlotFromEnt(owner);
        if (slot != -1 && slot < maxc) {
            std::snprintf(desc, sizeof(desc), "no_classname owner_slot=%d", slot);
            LogWireframeOk(trace_kind, trace_index, nullptr, origin, mns, mxs, desc);
            DebugDrawBox2(nullptr, origin, mns, mxs, 1, slot + 1);
        } else {
            LogWireframeOk(trace_kind, trace_index, nullptr, origin, mns, mxs, "no_classname default pal=1 exw=0");
            DebugDrawBox2(nullptr, origin, mns, mxs, 1, 0);
        }
    } else {
        LogWireframeOk(trace_kind, trace_index, nullptr, origin, mns, mxs, "no_classname no_owner pal=1 exw=0");
        DebugDrawBox2(nullptr, origin, mns, mxs, 1, 0);
    }
}

struct SpawnFrame {
    void* ent;
    bool have_freed_snapshot;
    std::size_t level_cache_slot;
    float origin[3];
    float mins[3];
    float maxs[3];
    char classname[256];
    uintptr_t owner;
    int self_player_slot;
};

// Snapshots from ED_CallSpawn during ss_loading (see FinalizeSpawnDraw). Draw on demand via
// sofbuddy_entities_draw.
struct CachedLevelEnt {
    float origin[3];
    float mins[3];
    float maxs[3];
    char classname[256];
    uintptr_t owner;
    int self_player_slot;
};

// Merged wireframe list: std::map iteration order is stable for a fixed set of keys → identical FX output each run.
struct WireframeDrawCmd {
    float origin[3];
    float mins[3];
    float maxs[3];
    char classname[256];
    uintptr_t owner{};
    int self_player_slot{};
    int trace_index{};
    bool from_cache{};
};

static std::vector<SpawnFrame> s_spawn_stack;

static std::vector<CachedLevelEnt> s_level_entity_cache;

// While true, ED_CallSpawn must not append to s_level_entity_cache — SpawnEntities already filled it from
// CM_EntityString (BSP lump), which includes light* and every other map ent regardless of edict lifetime.
static bool s_skip_edcall_level_cache = false;

static bool KeyEqualsAsciiLower(char const* key, char const* lit) {
    for (; *lit; ++key, ++lit) {
        char a = *key;
        char b = *lit;
        if (a >= 'A' && a <= 'Z') {
            a += static_cast<char>('a' - 'A');
        }
        if (b >= 'A' && b <= 'Z') {
            b += static_cast<char>('a' - 'A');
        }
        if (a != b) {
            return false;
        }
    }
    return *key == 0;
}

enum { kComTokenMaxEnt = 1024 };
static char       com_token_ent[kComTokenMaxEnt];
static char* ComParseEnt(char** data_p) {
    int      c;
    int      len = 0;
    char*    data = *data_p;
    com_token_ent[0] = 0;
    if (!data) {
        *data_p = nullptr;
        return com_token_ent;
    }
skipwhite:
    while ((c = static_cast<unsigned char>(*data)) <= ' ') {
        if (c == 0) {
            *data_p = nullptr;
            return com_token_ent;
        }
        data++;
    }
    if (c == '/' && data[1] == '/') {
        while (*data && *data != '\n') {
            data++;
        }
        goto skipwhite;
    }
    if (c == '\"') {
        data++;
        while (1) {
            c = static_cast<unsigned char>(*data++);
            if (c == '\"' || c == 0) {
                com_token_ent[len] = 0;
                *data_p            = data;
                return com_token_ent;
            }
            if (len < kComTokenMaxEnt - 1) {
                com_token_ent[len++] = static_cast<char>(c);
            }
        }
    }
    do {
        if (len < kComTokenMaxEnt - 1) {
            com_token_ent[len++] = static_cast<char>(c);
        }
        data++;
        c = static_cast<unsigned char>(*data);
    } while (c > 32);
    com_token_ent[len] = 0;
    *data_p            = data;
    return com_token_ent;
}

static void ParseMapEntitiesIntoCache(char* entities) {
    if (!entities || !entities[0]) {
        return;
    }
    std::vector<char> buf(std::strlen(entities) + 1u);
    std::memcpy(buf.data(), entities, buf.size());
    char* data  = buf.data();
    char** datap = &data;

    while (1) {
        char* tok = ComParseEnt(datap);
        if (!*datap) {
            break;
        }
        if (!tok[0]) {
            break;
        }
        if (tok[0] != '{') {
            continue;
        }

        char  classname[256]{};
        float origin[3]{};
        float mins[3]{};
        float maxs[3]{};
        bool  has_origin = false;
        bool  has_mins   = false;
        bool  has_maxs   = false;

        while (1) {
            tok = ComParseEnt(datap);
            if (!*datap) {
                return;
            }
            if (tok[0] == '}') {
                break;
            }
            char keyname[256];
            std::strncpy(keyname, tok, sizeof(keyname) - 1u);
            keyname[sizeof(keyname) - 1u] = '\0';

            tok = ComParseEnt(datap);
            if (!*datap) {
                return;
            }
            if (tok[0] == '}') {
                break;
            }

            if (keyname[0] == '_') {
                continue;
            }
            if (KeyEqualsAsciiLower(keyname, "classname")) {
                std::strncpy(classname, tok, sizeof(classname) - 1u);
                classname[sizeof(classname) - 1u] = '\0';
            } else if (KeyEqualsAsciiLower(keyname, "origin")) {
                if (std::sscanf(tok, "%f %f %f", &origin[0], &origin[1], &origin[2]) == 3) {
                    has_origin = true;
                }
            } else if (KeyEqualsAsciiLower(keyname, "mins")) {
                if (std::sscanf(tok, "%f %f %f", &mins[0], &mins[1], &mins[2]) == 3) {
                    has_mins = true;
                }
            } else if (KeyEqualsAsciiLower(keyname, "maxs")) {
                if (std::sscanf(tok, "%f %f %f", &maxs[0], &maxs[1], &maxs[2]) == 3) {
                    has_maxs = true;
                }
            }
        }

        if (!classname[0]) {
            continue;
        }
        CachedLevelEnt c{};
        if (has_origin) {
            std::memcpy(c.origin, origin, sizeof(origin));
        }
        if (has_mins) {
            std::memcpy(c.mins, mins, sizeof(mins));
        }
        if (has_maxs) {
            std::memcpy(c.maxs, maxs, sizeof(maxs));
        }
        std::strncpy(c.classname, classname, sizeof(c.classname) - 1u);
        c.classname[sizeof(c.classname) - 1u] = '\0';
        c.owner                               = 0;
        c.self_player_slot                    = -1;
        EvFinalizeStudyBboxForWireframe(c.origin, c.mins, c.maxs, c.classname[0] ? c.classname : nullptr);
        s_level_entity_cache.push_back(c);
    }
}

void OnSpawnEntitiesPrologue(char* entities) {
    s_skip_edcall_level_cache = true;
    s_level_entity_cache.clear();
    if (IsEnabled() && entities) {
        ParseMapEntitiesIntoCache(entities);
    }
}

void OnSpawnEntitiesEpilogue() {
    s_skip_edcall_level_cache = false;
}

// After sv.state hits ss_game, map ents can still run ED_CallSpawn. We used to clear linger on the first
// consecutive ss_game pair — but ClientThink runs Tick every frame and cleared linger before the spawn burst
// finished (reconnect / warm load), so the cache stayed empty. Use a wall-clock window and a spawn-count cap.
static bool s_map_spawn_linger = false;
static unsigned s_map_spawn_linger_deadline_ms = 0;
static unsigned s_spawns_after_worldspawn = 0;
// Last-seen sv.state for edge detection (must be shared with PrepareLevelEntityCacheForMapStudyLoad).
static int s_prev_sv_state = 0x7FFFFFFF;

static constexpr unsigned kMapSpawnLingerWindowMs = 8000;
static constexpr unsigned kMapSpawnLingerSpawnCap = 20000;
// After SCR_BeginLoadingPlaque: record spawns until worldspawn without trusting client sv.state (see NotifyLoadingPlaque).
static bool s_plaque_bootstrap_capture = false;
static unsigned s_plaque_bootstrap_deadline_ms = 0;
static constexpr unsigned kPlaqueBootstrapMaxMs = 120000;

static unsigned MapSpawnLingerNowMs() {
    if (!detour_Sys_Milliseconds::oSys_Milliseconds) {
        return 0u;
    }
    int const t = detour_Sys_Milliseconds::oSys_Milliseconds();
    return t < 0 ? 0u : static_cast<unsigned>(t);
}

static bool PlaqueBootstrapActiveForCache() {
    if (!s_plaque_bootstrap_capture) {
        return false;
    }
    if (!detour_Sys_Milliseconds::oSys_Milliseconds || s_plaque_bootstrap_deadline_ms == 0u) {
        return true;
    }
    unsigned const now = MapSpawnLingerNowMs();
    return now < s_plaque_bootstrap_deadline_ms;
}

static void TickLevelCacheServerStateImpl() {
    int const st = static_cast<int>(stget_u32(kSvStateAddr));
    if (st == kSvStateLoading) {
        // Do not clear s_level_entity_cache here — worldspawn clears it at map start (PushSpawnStack). Clearing on
        // every transition into ss_loading dropped the spawn snapshot on brief sv.state glitches during ss_game,
        // which removed cache-only ents (e.g. light* freed after bake) until remapping.
        if (s_prev_sv_state != kSvStateLoading) {
            s_spawn_stack.clear();
        }
        // Refresh linger while still loading so a long ss_loading phase cannot expire the post-load window before
        // late spawns (and so the deadline slides from the last loading tick, not only the first edge).
        s_map_spawn_linger = true;
        if (detour_Sys_Milliseconds::oSys_Milliseconds) {
            unsigned const now = MapSpawnLingerNowMs();
            s_map_spawn_linger_deadline_ms = now + kMapSpawnLingerWindowMs;
        }
    }
    if (s_map_spawn_linger) {
        unsigned const now = MapSpawnLingerNowMs();
        if (detour_Sys_Milliseconds::oSys_Milliseconds && s_map_spawn_linger_deadline_ms != 0u &&
            now >= s_map_spawn_linger_deadline_ms) {
            s_map_spawn_linger = false;
        }
        if (s_spawns_after_worldspawn > kMapSpawnLingerSpawnCap) {
            s_map_spawn_linger = false;
        }
    }
    if (s_plaque_bootstrap_capture && detour_Sys_Milliseconds::oSys_Milliseconds &&
        s_plaque_bootstrap_deadline_ms != 0u) {
        unsigned const now = MapSpawnLingerNowMs();
        if (now >= s_plaque_bootstrap_deadline_ms) {
            s_plaque_bootstrap_capture = false;
        }
    }
    s_prev_sv_state = st;
}

static void PrepareLevelEntityCacheForMapStudyLoadImpl() {
    ClearClientThinkAttackLatchState();
    s_level_entity_cache.clear();
    s_spawn_stack.clear();
    s_map_spawn_linger = false;
    s_map_spawn_linger_deadline_ms = 0;
    s_spawns_after_worldspawn = 0;
    s_plaque_bootstrap_capture = false;
    s_plaque_bootstrap_deadline_ms = 0;
    // Next map load should hit ss_loading with a non-loading previous state so cache repopulates (second `map` fix).
    s_prev_sv_state = kSvStateGame;
}

bool MapSpawnLingerActiveForCache() {
    return s_map_spawn_linger;
}

// g_spawn.cpp: ED_CallSpawn calls s->spawn(ent); SP_info_player_deathmatch (p_client.cpp) may G_FreeEdict(self)
// when !dm->isDM(). Seed geometry at push (before spawn) and refresh on G_FreeEdict so freed ents still cache.
static void FillSpawnFrameFromEdict(SpawnFrame* f, void* ent) {
    if (!f || !ent) {
        return;
    }
    unsigned char* e = reinterpret_cast<unsigned char*>(ent);
    std::memcpy(f->origin, e + kEdictSOriginOffset, sizeof(f->origin));
    std::memcpy(f->mins, e + kEdictMinsOffset, sizeof(f->mins));
    std::memcpy(f->maxs, e + kEdictMaxsOffset, sizeof(f->maxs));
    char* cn = *reinterpret_cast<char**>(e + kEdictClassnameOffset);
    if (cn) {
        std::strncpy(f->classname, cn, sizeof(f->classname) - 1u);
        f->classname[sizeof(f->classname) - 1u] = '\0';
    } else {
        f->classname[0] = '\0';
    }
    f->owner = stget(reinterpret_cast<uintptr_t>(e), kEdictOwnerOffset);
    f->self_player_slot = -1;
    if (f->classname[0] && std::strncmp(f->classname, "player", 6) == 0) {
        f->self_player_slot = GetPlayerSlotFromEnt(ent);
    }
}

static void FillCachedLevelEntFromSpawn(void* ent, SpawnFrame const& top, CachedLevelEnt* c) {
    if (!c) {
        return;
    }
    unsigned char* e = ent ? reinterpret_cast<unsigned char*>(ent) : nullptr;
    bool const in_use = e && *reinterpret_cast<qboolean*>(e + kEdictInUseOffset);
    bool const use_frame = top.have_freed_snapshot || !in_use;
    if (use_frame) {
        std::memcpy(c->origin, top.origin, sizeof(c->origin));
        std::memcpy(c->mins, top.mins, sizeof(c->mins));
        std::memcpy(c->maxs, top.maxs, sizeof(c->maxs));
        std::memcpy(c->classname, top.classname, sizeof(c->classname));
        c->owner = top.owner;
        c->self_player_slot = top.self_player_slot;
    } else if (e) {
        std::memcpy(c->origin, e + kEdictSOriginOffset, sizeof(c->origin));
        std::memcpy(c->mins, e + kEdictMinsOffset, sizeof(c->mins));
        std::memcpy(c->maxs, e + kEdictMaxsOffset, sizeof(c->maxs));
        char* cn = *reinterpret_cast<char**>(e + kEdictClassnameOffset);
        if (cn) {
            std::strncpy(c->classname, cn, sizeof(c->classname) - 1u);
            c->classname[sizeof(c->classname) - 1u] = '\0';
        } else {
            c->classname[0] = '\0';
        }
        c->owner = stget(reinterpret_cast<uintptr_t>(e), kEdictOwnerOffset);
        c->self_player_slot = -1;
        if (c->classname[0] && std::strncmp(c->classname, "player", 6) == 0) {
            c->self_player_slot = GetPlayerSlotFromEnt(ent);
        }
    } else {
        std::memset(c, 0, sizeof(*c));
        return;
    }
    EvFinalizeStudyBboxForWireframe(c->origin, c->mins, c->maxs, c->classname[0] ? c->classname : nullptr);
}

static constexpr std::size_t kNoLevelCacheSlot = static_cast<std::size_t>(-1);

// One cache row per ED_CallSpawn, reserved at push (before checkItemSpawn / I_Spawn) so early-freed pickups are not lost.
static std::size_t AppendPreSpawnToLevelCache(SpawnFrame const& f) {
    CachedLevelEnt c{};
    std::memcpy(c.origin, f.origin, sizeof(c.origin));
    std::memcpy(c.mins, f.mins, sizeof(c.mins));
    std::memcpy(c.maxs, f.maxs, sizeof(c.maxs));
    std::memcpy(c.classname, f.classname, sizeof(c.classname));
    c.owner = f.owner;
    c.self_player_slot = f.self_player_slot;
    EvFinalizeStudyBboxForWireframe(c.origin, c.mins, c.maxs, c.classname[0] ? c.classname : nullptr);
    s_level_entity_cache.push_back(c);
    return s_level_entity_cache.size() - 1u;
}

static void RefreshLevelCacheSlot(void* ent, SpawnFrame const& top) {
    if (top.level_cache_slot == kNoLevelCacheSlot || top.level_cache_slot >= s_level_entity_cache.size()) {
        return;
    }
    CachedLevelEnt c{};
    FillCachedLevelEntFromSpawn(ent, top, &c);
    s_level_entity_cache[top.level_cache_slot] = c;
}

static void TrySnapshotFreedSpawnTarget(void* ed) {
    if (detail::s_skip_edcall_level_cache || !ev::ShouldTrackMapSpawnsForCache()) {
        return;
    }
    if (s_spawn_stack.empty()) {
        return;
    }
    SpawnFrame& top = s_spawn_stack.back();
    if (top.ent != ed) {
        return;
    }
    SpawnFrame const seed = top;
    FillSpawnFrameFromEdict(&top, ed);
    if (EvIsZeroBounds(top.origin, top.mins, top.maxs)) {
        std::memcpy(top.origin, seed.origin, sizeof(top.origin));
        std::memcpy(top.mins, seed.mins, sizeof(top.mins));
        std::memcpy(top.maxs, seed.maxs, sizeof(top.maxs));
        top.owner = seed.owner;
        top.self_player_slot = seed.self_player_slot;
    }
    if (!top.classname[0] && seed.classname[0]) {
        std::memcpy(top.classname, seed.classname, sizeof(top.classname));
    }
    top.have_freed_snapshot = true;
}

// First in-use edict with a game client and classname starting with "player" (not always edict 1).
static unsigned char* FindFirstPlayerEdict(unsigned base, unsigned num_edicts) {
    if (!base || !num_edicts) {
        return nullptr;
    }
    unsigned const limit = base + kSizeOfEdict * num_edicts;
    for (unsigned addr = base + kSizeOfEdict; addr < limit; addr += kSizeOfEdict) {
        auto* e = reinterpret_cast<unsigned char*>(addr);
        if (!*reinterpret_cast<qboolean*>(e + kEdictInUseOffset)) {
            continue;
        }
        if (!*reinterpret_cast<void**>(e + kEdictGClientOffset)) {
            continue;
        }
        char* cn = *reinterpret_cast<char**>(e + kEdictClassnameOffset);
        if (cn && std::strncmp(cn, "player", 6) == 0) {
            return e;
        }
    }
    return nullptr;
}

// Console helper: draw a test box at a player edict's position (sanity-check FX path).
// If `report` is true, PrintOut(PRINT_DEV) why we bail or where we drew (ev_debugbox runs as xcommand_t;
// avoid Com_Printf there — see util.cpp PRINT_DEV -> Com_DPrintf).
static void DebugDrawBoxAtPlayer(bool report) {
    if (!FxApisOk()) {
        if (report) {
            PrintOut(PRINT_DEV, "[entity_visualizer] ev_debugbox: FX detours missing\n");
        }
        return;
    }
    unsigned const base = stget_u32(kEdictsBasePtrAddr);
    unsigned const num = stget_u32(kNumEdictsPtrAddr);
    if (!base) {
        if (report) {
            PrintOut(PRINT_DEV, "[entity_visualizer] ev_debugbox: g_edicts is NULL\n");
        }
        return;
    }
    unsigned char* player = FindFirstPlayerEdict(base, num);
    if (!player) {
        if (report) {
            PrintOut(PRINT_DEV, "[entity_visualizer] ev_debugbox: no player edict (num_edicts=%u)\n", num);
        }
        return;
    }

    float* origin = reinterpret_cast<float*>(player + kEdictSOriginOffset);

    // If the edict already has mins/maxs, prefer those. Otherwise, use a reasonable default player box.
    float* mins_ptr = reinterpret_cast<float*>(player + kEdictMinsOffset);
    float* maxs_ptr = reinterpret_cast<float*>(player + kEdictMaxsOffset);
    float mins[3] = {-16.0f, -16.0f, -24.0f};
    float maxs[3] = {16.0f, 16.0f, 32.0f};
    if (!(mins_ptr[0] == 0.0f && mins_ptr[1] == 0.0f && mins_ptr[2] == 0.0f &&
          maxs_ptr[0] == 0.0f && maxs_ptr[1] == 0.0f && maxs_ptr[2] == 0.0f)) {
        std::memcpy(mins, mins_ptr, sizeof(mins));
        std::memcpy(maxs, maxs_ptr, sizeof(maxs));
    }

    unsigned const idx = static_cast<unsigned>(player - reinterpret_cast<unsigned char*>(base)) / kSizeOfEdict;
    DebugDrawBox2(nullptr, origin, mins, maxs, /*nColor=*/4, /*exceptWho=*/0, /*ev_dev_log=*/report);
    if (report) {
        PrintOut(PRINT_DEV, "[entity_visualizer] ev_debugbox: edict[%u] origin=(%.2f %.2f %.2f)\n", idx,
                 static_cast<double>(origin[0]), static_cast<double>(origin[1]),
                 static_cast<double>(origin[2]));
    }
}

// Adds live edicts whose dedupe key is not already in uniq (spawn cache wins). No drawing here.
// (IDA: no separate client entity list symbol in this IDB — server g_edicts + spawn cache merge is the stable model.)
static void MergeLiveEdictsIntoWireframeMap(std::map<std::string, WireframeDrawCmd>* uniq, unsigned char* player) {
    if (!uniq) {
        return;
    }
    unsigned const base = stget_u32(kEdictsBasePtrAddr);
    unsigned const num = stget_u32(kNumEdictsPtrAddr);
    if (!base || !num) {
        return;
    }
    void* const player_void = player ? reinterpret_cast<void*>(player) : nullptr;
    unsigned const limit = base + kSizeOfEdict * num;
    size_t skip_not_inuse = 0;
    bool const verb = EntitiesDrawVerbose();
    for (unsigned addr = base; addr < limit; addr += kSizeOfEdict) {
        unsigned char* ent = reinterpret_cast<unsigned char*>(addr);
        unsigned const edict_idx = static_cast<unsigned>((addr - base) / kSizeOfEdict);

        if (addr == base) {
            if (verb) {
                float o[3];
                float mn[3];
                float mx[3];
                std::memcpy(o, ent + kEdictSOriginOffset, sizeof(o));
                std::memcpy(mn, ent + kEdictMinsOffset, sizeof(mn));
                std::memcpy(mx, ent + kEdictMaxsOffset, sizeof(mx));
                char* cn = *reinterpret_cast<char**>(ent + kEdictClassnameOffset);
                LogWireframeSkip("live", static_cast<int>(edict_idx), "world_edict", cn, o[0], o[1], o[2], mn, mx, "");
            }
            continue;
        }
        if (!*reinterpret_cast<qboolean*>(ent + kEdictInUseOffset)) {
            ++skip_not_inuse;
            continue;
        }
        if (player && ent == player) {
            if (verb) {
                float o[3];
                float mn[3];
                float mx[3];
                std::memcpy(o, ent + kEdictSOriginOffset, sizeof(o));
                std::memcpy(mn, ent + kEdictMinsOffset, sizeof(mn));
                std::memcpy(mx, ent + kEdictMaxsOffset, sizeof(mx));
                char* cn = *reinterpret_cast<char**>(ent + kEdictClassnameOffset);
                LogWireframeSkip("live", static_cast<int>(edict_idx), "local_player_supplement", cn, o[0], o[1], o[2],
                                 mn, mx, "");
            }
            continue;
        }
        uintptr_t const owner_u = static_cast<uintptr_t>(stget(reinterpret_cast<uintptr_t>(ent), kEdictOwnerOffset));
        if (player_void && reinterpret_cast<void*>(owner_u) == player_void) {
            if (verb) {
                float o[3];
                float mn[3];
                float mx[3];
                std::memcpy(o, ent + kEdictSOriginOffset, sizeof(o));
                std::memcpy(mn, ent + kEdictMinsOffset, sizeof(mn));
                std::memcpy(mx, ent + kEdictMaxsOffset, sizeof(mx));
                char* cn = *reinterpret_cast<char**>(ent + kEdictClassnameOffset);
                LogWireframeSkip("live", static_cast<int>(edict_idx), "owner_is_view_player", cn, o[0], o[1], o[2], mn,
                                 mx, "");
            }
            continue;
        }

        char* cn = *reinterpret_cast<char**>(ent + kEdictClassnameOffset);
        float o[3];
        float mn[3];
        float mx[3];
        std::memcpy(o, ent + kEdictSOriginOffset, sizeof(o));
        std::memcpy(mn, ent + kEdictMinsOffset, sizeof(mn));
        std::memcpy(mx, ent + kEdictMaxsOffset, sizeof(mx));
        if (EvIsNonFiniteOrigin(o)) {
            if (verb) {
                LogWireframeSkip("live", static_cast<int>(edict_idx), "non_finite_origin", cn, o[0], o[1], o[2], mn,
                                 mx, "");
            }
            continue;
        }
        if (EvSuspiciousLivePickupOrigin(cn, o)) {
            if (verb) {
                LogWireframeSkip("live", static_cast<int>(edict_idx), "suspicious_pickup_origin_live", cn, o[0], o[1],
                                 o[2], mn, mx, "(use cache pass for spawn snapshot)");
            }
            continue;
        }
        EvFinalizeStudyBboxForWireframe(o, mn, mx, cn);
        if (EvIsZeroBounds(o, mn, mx)) {
            if (verb) {
                LogWireframeSkip("live", static_cast<int>(edict_idx), "zero_bounds_after_finalize", cn, o[0], o[1],
                                 o[2], mn, mx, "");
            }
            continue;
        }
        std::string const key = WireframeDedupeKey(cn, o, mn, mx);
        if (uniq->find(key) != uniq->end()) {
            if (verb) {
                LogWireframeSkip("live", static_cast<int>(edict_idx), "already_in_merge_map", cn, o[0], o[1], o[2], mn,
                                 mx, "(spawn cache or earlier row)");
            }
            continue;
        }
        WireframeDrawCmd cmd{};
        std::memcpy(cmd.origin, o, sizeof(cmd.origin));
        std::memcpy(cmd.mins, mn, sizeof(cmd.mins));
        std::memcpy(cmd.maxs, mx, sizeof(cmd.maxs));
        if (cn && cn[0]) {
            std::strncpy(cmd.classname, cn, sizeof(cmd.classname) - 1u);
            cmd.classname[sizeof(cmd.classname) - 1u] = '\0';
        }
        cmd.owner = owner_u;
        cmd.self_player_slot =
            (cn && std::strncmp(cn, "player", 6) == 0) ? GetPlayerSlotFromEnt(ent) : -1;
        cmd.trace_index = static_cast<int>(edict_idx);
        cmd.from_cache = false;
        (*uniq)[key] = cmd;
    }
    if (verb && skip_not_inuse > 0u) {
        PrintOut(PRINT_DEV,
                 "[entity_visualizer] wireframe live merge: %zu edict slot(s) skipped reason=not_inuse\n",
                 skip_not_inuse);
    }
}

static void DrawAllCachedLevelEntityBoxes(bool report) {
    bool const fx_ok = FxApisOk();
    if (report && !fx_ok) {
        PrintOut(PRINT_DEV, "[entity_visualizer] sofbuddy_entities_draw: FX detours not ready (per-entity SKIP lines "
                            "if _sofbuddy_entities_draw_verbose)\n");
    }
    if (fx_ok) {
        EvFxClearWireframePoolBeforeRedraw();
    }
    std::map<std::string, WireframeDrawCmd> uniq;
    bool const verb = EntitiesDrawVerbose();
    size_t const n = s_level_entity_cache.size();

    for (size_t i = 0; i < n; ++i) {
        CachedLevelEnt const& c = s_level_entity_cache[i];
        char const* cn = c.classname[0] ? c.classname : nullptr;
        float mn[3];
        float mx[3];
        std::memcpy(mn, c.mins, sizeof(mn));
        std::memcpy(mx, c.maxs, sizeof(mx));
        EvFinalizeStudyBboxForWireframe(c.origin, mn, mx, cn);
        if (EvIsZeroBounds(c.origin, mn, mx)) {
            if (verb) {
                LogWireframeSkip("cache", static_cast<int>(i), "zero_bounds_after_finalize", cn, c.origin[0],
                                 c.origin[1], c.origin[2], mn, mx, "");
            }
            continue;
        }
        std::string const key = WireframeDedupeKey(cn, c.origin, mn, mx);
        if (uniq.find(key) != uniq.end()) {
            if (verb) {
                LogWireframeSkip("cache", static_cast<int>(i), "duplicate_wireframe_key", cn, c.origin[0], c.origin[1],
                                 c.origin[2], mn, mx, "");
            }
            continue;
        }
        WireframeDrawCmd cmd{};
        std::memcpy(cmd.origin, c.origin, sizeof(cmd.origin));
        std::memcpy(cmd.mins, c.mins, sizeof(cmd.mins));
        std::memcpy(cmd.maxs, c.maxs, sizeof(cmd.maxs));
        if (c.classname[0]) {
            std::strncpy(cmd.classname, c.classname, sizeof(cmd.classname) - 1u);
            cmd.classname[sizeof(cmd.classname) - 1u] = '\0';
        }
        cmd.owner = c.owner;
        cmd.self_player_slot = c.self_player_slot;
        cmd.trace_index = static_cast<int>(i);
        cmd.from_cache = true;
        uniq[key] = cmd;
    }
    size_t const after_cache_merge = uniq.size();
    unsigned char* const player = FindFirstPlayerEdict(stget_u32(kEdictsBasePtrAddr), stget_u32(kNumEdictsPtrAddr));
    MergeLiveEdictsIntoWireframeMap(&uniq, player);
    size_t const live_only = uniq.size() - after_cache_merge;

    for (auto const& kv : uniq) {
        WireframeDrawCmd const& w = kv.second;
        char const* cn = w.classname[0] ? w.classname : nullptr;
        char const* tk = w.from_cache ? "cache" : "live";
        DrawBoxFromGeometry(w.origin, w.mins, w.maxs, cn, w.owner, w.self_player_slot, tk, w.trace_index);
    }
    if (report) {
        PrintOut(PRINT_DEV,
                 "[entity_visualizer] sofbuddy_entities_draw: %zu unique wireframe(s) "
                 "(stable map order; %zu from spawn cache merge, %zu live-only). %zu cache row(s) scanned.\n",
                 uniq.size(), after_cache_merge, live_only, n);
        if (uniq.empty() && n == 0u) {
            PrintOut(PRINT_DEV,
                     "[entity_visualizer] sofbuddy_entities_draw: hint — set _sofbuddy_entity_edit 1 and load a map; "
                     "spawn cache is filled from the BSP entity string in SpawnEntities (CM_EntityString)\n");
        }
    }
}

} // namespace detail

void IntersectMergeCachedLevelRayHits(void* player_ent, float ox, float oy, float oz, float dx, float dy, float dz,
                                      float* inout_best_t, unsigned char** io_hit_live_ent, char* out_cache_classname,
                                      size_t out_cache_classname_bytes) {
    if (!inout_best_t || !io_hit_live_ent || !out_cache_classname || out_cache_classname_bytes == 0) {
        return;
    }
    out_cache_classname[0] = '\0';
    uintptr_t const player_u = reinterpret_cast<uintptr_t>(player_ent);
    for (detail::CachedLevelEnt const& c : detail::s_level_entity_cache) {
        if (c.owner && c.owner == player_u) {
            continue;
        }
        float mn[3];
        float mx[3];
        std::memcpy(mn, c.mins, sizeof(mn));
        std::memcpy(mx, c.maxs, sizeof(mx));
        EvFinalizeStudyBboxForWireframe(c.origin, mn, mx, c.classname[0] ? c.classname : nullptr);
        if (EvIsZeroBounds(c.origin, mn, mx)) {
            continue;
        }
        float const wmin[3] = {c.origin[0] + mn[0], c.origin[1] + mn[1], c.origin[2] + mn[2]};
        float const wmax[3] = {c.origin[0] + mx[0], c.origin[1] + mx[1], c.origin[2] + mx[2]};
        float t = 0.f;
        if (!EvRayVsAabb(ox, oy, oz, dx, dy, dz, wmin, wmax, t)) {
            continue;
        }
        if (t > 0.f && t < kMinIntersectT) {
            continue;
        }
        if (t >= *inout_best_t) {
            continue;
        }
        *inout_best_t = t;
        *io_hit_live_ent = nullptr;
        if (c.classname[0]) {
            std::strncpy(out_cache_classname, c.classname, out_cache_classname_bytes - 1u);
            out_cache_classname[out_cache_classname_bytes - 1u] = '\0';
        }
    }
}

// Console command: ev_debugbox
// Draws a wireframe box around the first player edict using the same FX path as entity spawns.
void Cmd_EV_DebugBox_f() {
    if (!IsEnabled()) {
        if (detour_Com_Printf::oCom_Printf) {
            detour_Com_Printf::oCom_Printf("[entity_visualizer] ev_debugbox requires _sofbuddy_entity_edit 1\n");
        }
        return;
    }
    detail::DebugDrawBoxAtPlayer(/*report=*/true);
}

// Console command: sofbuddy_entities_draw — emit FX boxes for all ents cached during the last ss_loading pass.
void Cmd_EV_DrawCached_f() {
    if (!IsEnabled()) {
        if (detour_Com_Printf::oCom_Printf) {
            detour_Com_Printf::oCom_Printf("[entity_visualizer] sofbuddy_entities_draw requires _sofbuddy_entity_edit 1\n");
        }
        return;
    }
    detail::DrawAllCachedLevelEntityBoxes(/*report=*/true);
}

void Cmd_EV_IntersectOnce_f(void) {
    IntersectOnceFromCommand();
}

void register_entity_visualizer_commands(void) {
    if (!detour_Cmd_AddCommand::oCmd_AddCommand) {
        if (detour_Com_Printf::oCom_Printf)
            detour_Com_Printf::oCom_Printf("[entity_visualizer] commands not registered (Cmd_AddCommand is null)\n");
        return;
    }
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("ev_debugbox"), Cmd_EV_DebugBox_f);
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_entities_draw"), Cmd_EV_DrawCached_f);
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_intersect_once"), Cmd_EV_IntersectOnce_f);
}

void PushSpawnStack(void* ent) {
    detail::SpawnFrame f{};
    f.ent = ent;
    f.have_freed_snapshot = false;
    f.level_cache_slot = detail::kNoLevelCacheSlot;
    detail::FillSpawnFrameFromEdict(&f, ent);
    // BSP entity string (SpawnEntities prologue) fills s_level_entity_cache; skip duplicate ED_CallSpawn rows.
    if (!detail::s_skip_edcall_level_cache && ShouldTrackMapSpawnsForCache()) {
        f.level_cache_slot = detail::AppendPreSpawnToLevelCache(f);
        detail::s_spawns_after_worldspawn++;
    }
    detail::s_spawn_stack.push_back(f);
}

void PopSpawnStack(void* ent) {
    if (detail::s_spawn_stack.empty()) {
        return;
    }
    if (detail::s_spawn_stack.back().ent != ent) {
        return;
    }
    detail::s_spawn_stack.pop_back();
}

void FinalizeSpawnDraw(void* ent) {
    if (detail::s_skip_edcall_level_cache || !ShouldTrackMapSpawnsForCache()) {
        return;
    }
    if (detail::s_spawn_stack.empty() || detail::s_spawn_stack.back().ent != ent) {
        return;
    }
    detail::SpawnFrame const& top = detail::s_spawn_stack.back();
    detail::RefreshLevelCacheSlot(ent, top);
}

void OnFreeEdictBeforeOriginal(void* ed) {
    detail::TrySnapshotFreedSpawnTarget(ed);
}

void TickLevelCacheServerState() {
    detail::TickLevelCacheServerStateImpl();
}

void NotifyLoadingPlaque(void) {
    // PrepareLevelEntityCacheForMapStudyLoad / deferred map: same edge reset. Plaque-bootstrap: ED_CallSpawn runs in
    // gamex86 while client sv.state may already be ss_game on warm loads, so IsSsLoading() is false for light* etc.
    detail::s_prev_sv_state = kSvStateGame;
    detail::s_plaque_bootstrap_capture = true;
    if (detour_Sys_Milliseconds::oSys_Milliseconds) {
        detail::s_plaque_bootstrap_deadline_ms = detail::MapSpawnLingerNowMs() + detail::kPlaqueBootstrapMaxMs;
    } else {
        detail::s_plaque_bootstrap_deadline_ms = 0u;
    }
}

void ResetLevelEntityCacheOnWorldspawn(void* ent) {
    if (!ent) {
        return;
    }
    unsigned char* e = reinterpret_cast<unsigned char*>(ent);
    char* cn = *reinterpret_cast<char**>(e + kEdictClassnameOffset);
    if (cn && std::strcmp(cn, "worldspawn") == 0) {
        // Do not clear s_level_entity_cache here — it was populated from the BSP entity string in
        // SpawnEntities (InstallSpawnEntitiesWrapper) before this ED_CallSpawn ran.
        detail::s_spawn_stack.clear();
        detail::s_map_spawn_linger = true;
        detail::s_spawns_after_worldspawn = 0;
        unsigned const now = detail::MapSpawnLingerNowMs();
        if (detour_Sys_Milliseconds::oSys_Milliseconds) {
            detail::s_map_spawn_linger_deadline_ms = now + detail::kMapSpawnLingerWindowMs;
        } else {
            detail::s_map_spawn_linger_deadline_ms = 0;
        }
        detail::s_plaque_bootstrap_capture = false;
        detail::s_plaque_bootstrap_deadline_ms = 0u;
        ClearClientThinkAttackLatchState();
    }
}

void PrepareLevelEntityCacheForMapStudyLoad(void) {
    detail::PrepareLevelEntityCacheForMapStudyLoadImpl();
}

bool ShouldTrackMapSpawnsForCache() {
    return IsSsLoading() || detail::MapSpawnLingerActiveForCache() || detail::PlaqueBootstrapActiveForCache();
}

} // namespace ev

#endif // FEATURE_ENTITY_VISUALIZER
