#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include "features/entity_visualizer/ev_internal.h"

#include "generated_detours.h"
#include "util.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void set_runtime_cvar_str(const char* name, const char* value) {
    if (!detour_Cvar_Set2::oCvar_Set2 || !name || !value) {
        return;
    }
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(name), const_cast<char*>(value), true);
}

// Map stem or subfolder path (e.g. dm/jpntclx) for `map <name>`. Blocks Cbuf injection
// (`;`, quotes, backslashes) and ".." segments.
bool sanitize_map_study_name(const char* raw, char* out, size_t out_sz) {
    if (!raw || !out || out_sz < 2) {
        return false;
    }
    size_t j = 0;
    for (size_t i = 0; raw[i] != '\0' && j + 1 < out_sz; ++i) {
        unsigned char const c = static_cast<unsigned char>(raw[i]);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }
        if (c == ';' || c == '"' || c == '\'' || c == '\\') {
            return false;
        }
        if (std::isalnum(c) != 0 || c == '_' || c == '-') {
            out[j++] = static_cast<char>(c);
            continue;
        }
        if (c == '/') {
            if (j == 0 || out[j - 1] == '/') {
                return false;
            }
            out[j++] = '/';
            continue;
        }
        return false;
    }
    out[j] = '\0';
    if (j == 0 || out[j - 1] == '/') {
        return false;
    }
    if (std::strstr(out, "..") != nullptr) {
        return false;
    }
    return true;
}

void cmd_map_study_load_f() {
    if (!detour_Cvar_Get::oCvar_Get) {
        return;
    }
    cvar_t* map_c = detour_Cvar_Get::oCvar_Get("_sofbuddy_map_debug_map", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (!map_c || !map_c->string) {
        PrintOut(PRINT_BAD, "Map study: _sofbuddy_map_debug_map is not set.\n");
        return;
    }
    char name[128];
    if (!sanitize_map_study_name(map_c->string, name, sizeof(name))) {
        PrintOut(PRINT_BAD,
                 "Map study: invalid map name (use letters, digits, _, -, / for subfolders; no ..).\n");
        return;
    }
    if (detour_Cvar_Set2::oCvar_Set2) {
        cvar_t* ev = detour_Cvar_Get::oCvar_Get("_sofbuddy_entity_edit", "0", 0, nullptr);
        if (ev) {
            detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_entity_edit"), const_cast<char*>("1"), true);
        }
    }
    {
        cvar_t* dm_pref =
            detour_Cvar_Get::oCvar_Get("_sofbuddy_map_study_deathmatch", "4", CVAR_SOFBUDDY_ARCHIVE, nullptr);
        int dm = 4;
        if (dm_pref && dm_pref->string) {
            dm = static_cast<int>(std::strtol(dm_pref->string, nullptr, 10));
            if (dm < 0 || dm > 5) {
                dm = 4;
            }
        }
        char dmb[8];
        std::snprintf(dmb, sizeof(dmb), "%d", dm);
        set_runtime_cvar_str("deathmatch", dmb);
    }
    ev::PrepareLevelEntityCacheForMapStudyLoad();
    char buf[160];
    std::snprintf(buf, sizeof(buf), "map %s\n", name);
    if (detour_Cbuf_AddText::oCbuf_AddText) {
        detour_Cbuf_AddText::oCbuf_AddText(buf);
    } else if (detour_Cmd_ExecuteString::oCmd_ExecuteString) {
        detour_Cmd_ExecuteString::oCmd_ExecuteString(buf);
    }
}

void cmd_map_study_wireframes_f() {
    if (detour_Cvar_Get::oCvar_Get && detour_Cvar_Set2::oCvar_Set2) {
        cvar_t* ev = detour_Cvar_Get::oCvar_Get("_sofbuddy_entity_edit", "0", 0, nullptr);
        if (ev) {
            detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_entity_edit"), const_cast<char*>("1"), true);
        }
    }
    if (detour_Cbuf_AddText::oCbuf_AddText) {
        detour_Cbuf_AddText::oCbuf_AddText("sofbuddy_entities_draw\n");
    } else if (detour_Cmd_ExecuteString::oCmd_ExecuteString) {
        detour_Cmd_ExecuteString::oCmd_ExecuteString("sofbuddy_entities_draw\n");
    }
}

} // namespace

namespace ev {

void register_entity_visualizer_map_study_commands(void) {
    if (!detour_Cmd_AddCommand::oCmd_AddCommand) {
        return;
    }
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_map_study_load"), cmd_map_study_load_f);
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_map_study_wireframes"),
                                           cmd_map_study_wireframes_f);
}

} // namespace ev

#endif // FEATURE_ENTITY_VISUALIZER
