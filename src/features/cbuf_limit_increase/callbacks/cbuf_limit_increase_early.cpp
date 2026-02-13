#include "feature_config.h"

#if FEATURE_CBUF_LIMIT_INCREASE

#include <windows.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "detours.h"
#include "util.h"

namespace {

using Cbuf_AddText_t = void(__cdecl*)(const char* text);
Cbuf_AddText_t orig_Cbuf_AddText = nullptr;

using Cbuf_InsertText_t = void(__cdecl*)(const char* text);
Cbuf_InsertText_t orig_Cbuf_InsertText = nullptr;

using Cmd_Exec_f_t = void(__cdecl*)(void);
Cmd_Exec_f_t orig_Cmd_Exec_f = nullptr;

using Com_Printf_t = void(__cdecl*)(const char* fmt, ...);
Com_Printf_t orig_Com_Printf_trampoline = nullptr;

// Set these from IDA for your SoF.exe build.
constexpr uintptr_t kCbufAddTextRva = 0x00018180;
constexpr uintptr_t kCbufInsertTextRva = 0x000181D0;
constexpr uintptr_t kCmdExecfRva = 0x00018930;
constexpr uintptr_t kCmdExecuteStringRva = 0x000194F0;
constexpr uintptr_t kComPrintfRva = 0x0001C6E0;
constexpr uintptr_t kFsLoadFileRva = 0x00025370; // FS_LoadFile(char*, void**, bool)
constexpr uintptr_t kFsFreeFileRva = 0x00025420; // FS_FreeFile(void*)

// Recursion depth while inside Cmd_Exec_f. Some builds may feed exec'd content to the
// command buffer in smaller pieces, so we treat anything inserted during exec as
// "probably script" and stream it instead of buffering.
static int s_exec_depth = 0;
static bool s_virtual_draining = false;
static std::string s_virtual_cmd_text;

using FS_LoadFile_t = int(__cdecl*)(char* path, void** buffer, bool override_pak);
FS_LoadFile_t orig_FS_LoadFile = nullptr;

using FS_FreeFile_t = void(__cdecl*)(void* buffer);
FS_FreeFile_t orig_FS_FreeFile = nullptr;

static bool ascii_ieq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        unsigned char ca = static_cast<unsigned char>(*a++);
        unsigned char cb = static_cast<unsigned char>(*b++);
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb - 'A' + 'a');
        if (ca != cb) return false;
    }
    return *a == '\0' && *b == '\0';
}

static const char* path_basename(const char* p) {
    if (!p) return "";
    const char* base = p;
    for (const char* it = p; *it; ++it) {
        if (*it == '\\' || *it == '/') base = it + 1;
    }
    return base;
}

static bool is_config_cfg_path(const char* filename) {
    // Intentionally strict: only bypass/stabilize exec for config.cfg.
    return ascii_ieq(path_basename(filename), "config.cfg");
}

static bool load_script_text_fs(const char* filename, std::string& out) {
    if (!filename || !*filename) return false;
    if (!orig_FS_LoadFile || !orig_FS_FreeFile) return false;

    // Engine signature uses char*, so pass a mutable, NUL-terminated buffer.
    std::string tmp(filename);
    if (tmp.empty()) return false;
    tmp.push_back('\0');

    void* buf = nullptr;
    const int len = orig_FS_LoadFile(&tmp[0], &buf, false);
    if (len < 0) return false;

    out.clear();
    if (len > 0 && buf) {
        out.assign(static_cast<const char*>(buf), static_cast<const char*>(buf) + len);
    }

    if (buf) {
        orig_FS_FreeFile(buf);
    }

    return true;
}

bool is_script_context() {
    return s_exec_depth > 0 || s_virtual_draining;
}

void drain_virtual_cbuf(bool flush_trailing_command) {
    if (!orig_Cmd_ExecuteString) return;
    if (s_virtual_draining) return;

    s_virtual_draining = true;
    int executed = 0;

    while (!s_virtual_cmd_text.empty()) {
        size_t i = 0;
        int quotes = 0;

        for (; i < s_virtual_cmd_text.size(); ++i) {
            const char c = s_virtual_cmd_text[i];
            if (c == '"') quotes ^= 1;
            if (c == '\n') break;
            if (!(quotes & 1) && c == ';') break;
        }

        const bool found_separator = (i < s_virtual_cmd_text.size());
        if (!found_separator && !flush_trailing_command) break;

        const size_t end = found_separator ? i : s_virtual_cmd_text.size();
        std::string line = s_virtual_cmd_text.substr(0, end);

        if (found_separator) {
            s_virtual_cmd_text.erase(0, i + 1);
        } else {
            s_virtual_cmd_text.clear();
        }

        // Trim like Cbuf_Execute/Cmd_ExecuteString paths expect.
        size_t begin = 0;
        while (begin < line.size() && static_cast<unsigned char>(line[begin]) <= ' ') ++begin;
        size_t stop = line.size();
        while (stop > begin && static_cast<unsigned char>(line[stop - 1]) <= ' ') --stop;

        if (stop > begin) {
            std::string cmd = line.substr(begin, stop - begin);
            orig_Cmd_ExecuteString(cmd.c_str());
        }

        if (++executed > 50000) {
            PrintOut(PRINT_BAD, "cbuf_limit_increase: aborting virtual cbuf drain (too many commands)\n");
            s_virtual_cmd_text.clear();
            break;
        }
    }

    s_virtual_draining = false;
}

void __cdecl hk_Cbuf_AddText(const char* text) {
    static bool s_seen = false;
    if (!orig_Cbuf_AddText) return;

    if (!s_seen) {
        s_seen = true;
        PrintOut(PRINT_GOOD, "cbuf_limit_increase: Cbuf_AddText hook is active\n");
    }

    if (!text || !orig_Cmd_ExecuteString) {
        orig_Cbuf_AddText(text);
        return;
    }

    if (is_script_context()) {
        s_virtual_cmd_text.append(text);
        drain_virtual_cbuf(false);
        return;
    }

    orig_Cbuf_AddText(text);
}

void __cdecl hk_Cbuf_InsertText(const char* text) {
    static bool s_seen = false;
    if (!orig_Cbuf_InsertText) return;

    if (!s_seen) {
        s_seen = true;
        PrintOut(PRINT_GOOD, "cbuf_limit_increase: Cbuf_InsertText hook is active\n");
    }

    if (!text || !orig_Cmd_ExecuteString) {
        orig_Cbuf_InsertText(text);
        return;
    }

    if (is_script_context()) {
        // InsertText semantics: the inserted text should execute before anything already queued.
        s_virtual_cmd_text.insert(0, text);
        drain_virtual_cbuf(false);
        return;
    }

    orig_Cbuf_InsertText(text);
}

void __cdecl hk_Cmd_Exec_f(void) {
    if (!orig_Cmd_Exec_f) return;

    static bool s_seen = false;
    if (!s_seen) {
        s_seen = true;
        PrintOut(PRINT_GOOD, "cbuf_limit_increase: Cmd_Exec_f hook is active\n");
    }

    if (!orig_Cmd_Argc || !orig_Cmd_Argv || !orig_Cmd_ExecuteString) {
        orig_Cmd_Exec_f();
        return;
    }

    const bool is_config_exec = (orig_Cmd_Argc() == 2) && is_config_cfg_path(orig_Cmd_Argv(1));

    // Keep risk minimal: only bypass/stabilize exec for config.cfg.
    if (is_config_exec) {
        const char* fname = orig_Cmd_Argv(1);

        // If we can load config.cfg through the engine filesystem, bypass the fixed
        // command buffer entirely and execute via the virtual buffer.
        std::string script;
        if (load_script_text_fs(fname, script)) {
            PrintOut(PRINT_LOG, "cbuf_limit_increase: bypassing exec for %s via FS_LoadFile (%u bytes)\n",
                     fname,
                     static_cast<unsigned int>(script.size()));

            if (orig_Com_Printf) {
                orig_Com_Printf("execing %s\n", fname);
            }

            ++s_exec_depth;
            // Insert semantics: run this script before anything already queued in our virtual buffer.
            s_virtual_cmd_text.insert(0, script);
            if (!s_virtual_draining) {
                drain_virtual_cbuf(true);
            }
            --s_exec_depth;
            return;
        }

        // Fall back to original behavior for config.cfg if FS_LoadFile failed.
        // Keep exec_depth set so any Cbuf_* activity during this exec is streamed.
        PrintOut(PRINT_BAD, "cbuf_limit_increase: FS_LoadFile failed for %s; falling back to engine Cmd_Exec_f\n", fname);
        ++s_exec_depth;
        orig_Cmd_Exec_f();
        --s_exec_depth;
        drain_virtual_cbuf(true);
        return;
    }

    // Non-config exec: preserve original behavior.
    orig_Cmd_Exec_f();
}

void __cdecl hk_Com_Printf(const char* fmt, ...) {
    if (!orig_Com_Printf_trampoline) return;

    static bool s_in = false;
    // Avoid recursion when we log using PrintOut (which itself uses Com_Printf in-game).
    if (s_in) {
        char buf[2048];
        buf[0] = '\0';
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt ? fmt : "", args);
        va_end(args);
        orig_Com_Printf_trampoline("%s", buf);
        return;
    }

    char msg[4096];
    msg[0] = '\0';
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt ? fmt : "", args);
    va_end(args);

    if (std::strstr(msg, "Cbuf_AddText: overflow") != nullptr) {
        s_in = true;
        void* ra = __builtin_return_address(0);
        void* sof_base = GetModuleBase("SoF.exe");
        const uintptr_t rva = (sof_base && ra) ? (uintptr_t)ra - (uintptr_t)sof_base : 0;
        PrintOut(PRINT_BAD,
                 "cbuf_limit_increase: observed overflow print (caller=%p rva=0x%08X exec_depth=%d virtual_len=%u)\n",
                 ra,
                 static_cast<unsigned int>(rva),
                 s_exec_depth,
                 static_cast<unsigned int>(s_virtual_cmd_text.size()));
        s_in = false;
    }

    orig_Com_Printf_trampoline("%s", msg);
}

} // namespace

void cbuf_limit_increase_EarlyStartup(void) {
    if (kCmdExecuteStringRva == 0) {
        PrintOut(PRINT_BAD, "cbuf_limit_increase: kCmdExecuteStringRva is not set\n");
        return;
    }

    if (!orig_Cmd_ExecuteString) {
        orig_Cmd_ExecuteString = reinterpret_cast<void(*)(const char*)>(
            rvaToAbsExe(reinterpret_cast<void*>(kCmdExecuteStringRva)));
    }
    if (!orig_Cmd_ExecuteString) {
        PrintOut(PRINT_BAD, "cbuf_limit_increase: failed to resolve Cmd_ExecuteString\n");
        return;
    }

    if (kFsLoadFileRva != 0 && !orig_FS_LoadFile) {
        void* fs_load_addr = rvaToAbsExe(reinterpret_cast<void*>(kFsLoadFileRva));
        orig_FS_LoadFile = reinterpret_cast<FS_LoadFile_t>(fs_load_addr);
        PrintOut(PRINT_GOOD, "cbuf_limit_increase: resolved FS_LoadFile at %p\n", fs_load_addr);
    }
    if (kFsFreeFileRva != 0 && !orig_FS_FreeFile) {
        void* fs_free_addr = rvaToAbsExe(reinterpret_cast<void*>(kFsFreeFileRva));
        orig_FS_FreeFile = reinterpret_cast<FS_FreeFile_t>(fs_free_addr);
        PrintOut(PRINT_GOOD, "cbuf_limit_increase: resolved FS_FreeFile at %p\n", fs_free_addr);
    }

#ifndef NDEBUG
    if (kComPrintfRva != 0) {
        void* com_printf_addr = rvaToAbsExe(reinterpret_cast<void*>(kComPrintfRva));
        if (DetourSystem::Instance().ApplyDetourAtAddress(
                com_printf_addr,
                reinterpret_cast<void*>(&hk_Com_Printf),
                reinterpret_cast<void**>(&orig_Com_Printf_trampoline),
                "Com_Printf",
                0)) {
            PrintOut(PRINT_GOOD, "cbuf_limit_increase: detoured Com_Printf at %p\n", com_printf_addr);
        } else {
            PrintOut(PRINT_BAD, "cbuf_limit_increase: failed to detour Com_Printf at %p\n", com_printf_addr);
        }
    }
#endif

    if (kCbufAddTextRva == 0) {
        PrintOut(PRINT_BAD, "cbuf_limit_increase: kCbufAddTextRva is not set\n");
        return;
    }

    void* cbuf_addtext_addr = rvaToAbsExe(reinterpret_cast<void*>(kCbufAddTextRva));
    if (DetourSystem::Instance().ApplyDetourAtAddress(
            cbuf_addtext_addr,
            reinterpret_cast<void*>(&hk_Cbuf_AddText),
            reinterpret_cast<void**>(&orig_Cbuf_AddText),
            "Cbuf_AddText",
            0)) {
        PrintOut(PRINT_GOOD, "cbuf_limit_increase: detoured Cbuf_AddText at %p\n", cbuf_addtext_addr);
    } else {
        PrintOut(PRINT_BAD, "cbuf_limit_increase: failed to detour Cbuf_AddText at %p\n", cbuf_addtext_addr);
    }

    if (kCbufInsertTextRva == 0) {
        PrintOut(PRINT_BAD, "cbuf_limit_increase: kCbufInsertTextRva is not set\n");
        return;
    }

    void* cbuf_inserttext_addr = rvaToAbsExe(reinterpret_cast<void*>(kCbufInsertTextRva));
    if (DetourSystem::Instance().ApplyDetourAtAddress(
            cbuf_inserttext_addr,
            reinterpret_cast<void*>(&hk_Cbuf_InsertText),
            reinterpret_cast<void**>(&orig_Cbuf_InsertText),
            "Cbuf_InsertText",
            0)) {
        PrintOut(PRINT_GOOD, "cbuf_limit_increase: detoured Cbuf_InsertText at %p\n", cbuf_inserttext_addr);
    } else {
        PrintOut(PRINT_BAD, "cbuf_limit_increase: failed to detour Cbuf_InsertText at %p\n", cbuf_inserttext_addr);
    }

    if (kCmdExecfRva == 0) {
        PrintOut(PRINT_BAD, "cbuf_limit_increase: kCmdExecfRva is not set\n");
        return;
    }

    void* cmd_execf_addr = rvaToAbsExe(reinterpret_cast<void*>(kCmdExecfRva));
    if (DetourSystem::Instance().ApplyDetourAtAddress(
            cmd_execf_addr,
            reinterpret_cast<void*>(&hk_Cmd_Exec_f),
            reinterpret_cast<void**>(&orig_Cmd_Exec_f),
            "Cmd_Exec_f",
            0)) {
        PrintOut(PRINT_GOOD, "cbuf_limit_increase: detoured Cmd_Exec_f at %p\n", cmd_execf_addr);
    } else {
        PrintOut(PRINT_BAD, "cbuf_limit_increase: failed to detour Cmd_Exec_f at %p\n", cmd_execf_addr);
    }
}

#endif // FEATURE_CBUF_LIMIT_INCREASE
