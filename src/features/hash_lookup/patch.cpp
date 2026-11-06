#include "feature_config.h"

#if FEATURE_HASH_LOOKUP

#include "patch.h"
#include "engine.h"
#include "index.h"
#include "util.h"

#include <cstdint>
#include <cstring>
#include <windows.h>

extern "C" {
extern void* hl_t_varvalue_found;
extern void* hl_t_varvalue_miss;
extern void* hl_t_varstring_found;
extern void* hl_t_varstring_miss;
extern void* hl_t_cvarget_found;
extern void* hl_t_cvarget_miss;
extern void* hl_t_cvarset2_found;
extern void* hl_t_cvarset2_miss;
extern void* hl_t_fullset_found;
extern void* hl_t_fullset_miss;
extern void* hl_t_cvarcmd_found;
extern void* hl_t_cvarcmd_miss;
extern void* hl_t_exec_cmd;
extern void* hl_t_exec_alias;
extern void* hl_t_exec_none;
extern void* hl_t_cmdremove_resume;
extern void* hl_v_cmd_functions;

void hl_stub_varvalue(void);
void hl_stub_varstring(void);
void hl_stub_cvarget(void);
void hl_stub_cvarset2(void);
void hl_stub_fullset(void);
void hl_stub_cvarcmd(void);
void hl_stub_cmdexec(void);
void hl_stub_cmdremove(void);
}

namespace hashlookup {
namespace {

constexpr unsigned kMaxSiteLen = 8;

struct Site {
    const char*         name;
    unsigned            rva;
    const std::uint8_t* orig;
    unsigned            len;      // >= 5; the tail past the jmp becomes 0x90
    void              (*stub)(void);
};

const Site kSites[] = {
    {"Cvar_VariableValue",  kRvaVarValueLoop,  kOrigVarLoop,   sizeof(kOrigVarLoop),   hl_stub_varvalue},
    {"Cvar_VariableString", kRvaVarStringLoop, kOrigVarLoop,   sizeof(kOrigVarLoop),   hl_stub_varstring},
    {"Cvar_Get",            kRvaCvarGetLoop,   kOrigCvarGet,   sizeof(kOrigCvarGet),   hl_stub_cvarget},
    {"Cvar_Set2",           kRvaCvarSet2Loop,  kOrigSetLoop,   sizeof(kOrigSetLoop),   hl_stub_cvarset2},
    {"Cvar_FullSet",        kRvaFullSetLoop,   kOrigSetLoop,   sizeof(kOrigSetLoop),   hl_stub_fullset},
    {"Cvar_Command",        kRvaCvarCmdLoop,   kOrigCvarCmd,   sizeof(kOrigCvarCmd),   hl_stub_cvarcmd},
    {"Cmd_ExecuteString",   kRvaCmdExecLoop,   kOrigCmdExec,   sizeof(kOrigCmdExec),   hl_stub_cmdexec},
    {"Cmd_RemoveCommand",   kRvaCmdRemoveHead, kOrigCmdRemove, sizeof(kOrigCmdRemove), hl_stub_cmdremove},
};

constexpr unsigned kSiteCount = sizeof(kSites) / sizeof(kSites[0]);

std::uint8_t   g_saved[kSiteCount][kMaxSiteLen];
unsigned char* g_base = nullptr;
bool           g_installed = false;

/** The engine image. The retail client is SoF.exe, but the same layout is
 *  shared with the dedicated server images (SoF-spsv.exe, sof-spsv16.exe); in
 *  every case the engine *is* the process image, so the unnamed handle is the
 *  reliable answer. The named lookups only make the common cases obvious in a
 *  debugger. */
HMODULE EngineModule() {
    if (HMODULE h = GetModuleHandleA("SoF.exe"))
        return h;
    if (HMODULE h = GetModuleHandleA("SoF-spsv.exe"))
        return h;
    return GetModuleHandleA(nullptr);
}

/** True when [rva, rva+len) is inside the module's mapped image. */
bool RvaInImage(HMODULE mod, unsigned rva, unsigned len) {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(mod);
    if (!dos || IsBadReadPtr(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        reinterpret_cast<const char*>(dos) + dos->e_lfanew);
    if (IsBadReadPtr(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE)
        return false;
    const DWORD size = nt->OptionalHeader.SizeOfImage;
    return rva < size && len <= size - rva;
}

bool WriteCode(void* dst, const std::uint8_t* src, unsigned len) {
    DWORD old = 0;
    if (!VirtualProtect(dst, len, PAGE_EXECUTE_READWRITE, &old))
        return false;
    std::memcpy(dst, src, len);
    VirtualProtect(dst, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), dst, len);
    return true;
}

void BindStubTargets(unsigned char* base) {
    auto at = [base](unsigned rva) { return static_cast<void*>(base + rva); };

    hl_t_varvalue_found   = at(kRvaVarValueFound);
    hl_t_varvalue_miss    = at(kRvaVarValueMiss);
    hl_t_varstring_found  = at(kRvaVarStringFound);
    hl_t_varstring_miss   = at(kRvaVarStringMiss);
    hl_t_cvarget_found    = at(kRvaCvarGetFound);
    hl_t_cvarget_miss     = at(kRvaCvarGetMiss);
    hl_t_cvarset2_found   = at(kRvaCvarSet2Found);
    hl_t_cvarset2_miss    = at(kRvaCvarSet2Miss);
    hl_t_fullset_found    = at(kRvaFullSetFound);
    hl_t_fullset_miss     = at(kRvaFullSetMiss);
    hl_t_cvarcmd_found    = at(kRvaCvarCmdFound);
    hl_t_cvarcmd_miss     = at(kRvaCvarCmdMiss);
    hl_t_exec_cmd         = at(kRvaCmdExecCmd);
    hl_t_exec_alias       = at(kRvaCmdExecAlias);
    hl_t_exec_none        = at(kRvaCmdExecNone);
    hl_t_cmdremove_resume = at(kRvaCmdRemoveResume);
    hl_v_cmd_functions    = at(kRvaCmdFunctions);
}

}  // namespace

bool Install() {
    if (g_installed)
        return true;

    HMODULE mod = EngineModule();
    if (!mod) {
        PrintOut(PRINT_BAD, "[hash_lookup] engine module not found - disabled\n");
        return false;
    }
    auto* base = reinterpret_cast<unsigned char*>(mod);

    // Pass one: prove every site is the code we reverse-engineered, before
    // touching any of it. A partially patched engine would be far worse than
    // an unpatched one, so a single mismatch aborts the whole install.
    for (unsigned i = 0; i < kSiteCount; ++i) {
        const Site& s = kSites[i];
        if (s.len < 5 || s.len > kMaxSiteLen) {
            PrintOut(PRINT_BAD, "[hash_lookup] %s: bad site length %u - disabled\n", s.name, s.len);
            return false;
        }
        if (!RvaInImage(mod, s.rva, s.len)) {
            PrintOut(PRINT_BAD, "[hash_lookup] %s: RVA 0x%X outside engine image - disabled\n",
                     s.name, s.rva);
            return false;
        }
        if (std::memcmp(base + s.rva, s.orig, s.len) != 0) {
            // Either this is not the retail engine we mapped, or something
            // else patched the same bytes first.
            PrintOut(PRINT_BAD,
                     "[hash_lookup] %s @0x%X: unexpected bytes - engine build not recognised, disabled\n",
                     s.name, s.rva);
            return false;
        }
        if (!s.stub) {
            PrintOut(PRINT_BAD, "[hash_lookup] %s: stub missing - disabled\n", s.name);
            return false;
        }
    }

    // The index has to be live before the first patched call can reach it.
    IndexBind(base);
    BindStubTargets(base);

    // Pass two: redirect. E9 rel32 to the stub, 0x90 for whatever is left of
    // the instructions we swallowed (all of it dead code - the only way into
    // those loops was the head we just replaced).
    for (unsigned i = 0; i < kSiteCount; ++i) {
        const Site& s = kSites[i];
        unsigned char* target = base + s.rva;
        std::memcpy(g_saved[i], target, s.len);

        std::uint8_t buf[kMaxSiteLen];
        std::memset(buf, 0x90, sizeof(buf));
        const std::int32_t rel = static_cast<std::int32_t>(
            reinterpret_cast<std::uintptr_t>(s.stub) -
            (reinterpret_cast<std::uintptr_t>(target) + 5));
        buf[0] = 0xE9;
        std::memcpy(buf + 1, &rel, 4);

        if (!WriteCode(target, buf, s.len)) {
            PrintOut(PRINT_BAD, "[hash_lookup] %s: VirtualProtect failed - rolling back\n", s.name);
            for (unsigned j = 0; j < i; ++j)
                WriteCode(base + kSites[j].rva, g_saved[j], kSites[j].len);
            IndexReset();
            return false;
        }
    }

    g_base = base;
    g_installed = true;
    return true;
}

void Revert() {
    if (!g_installed || !g_base)
        return;
    for (unsigned i = 0; i < kSiteCount; ++i)
        WriteCode(g_base + kSites[i].rva, g_saved[i], kSites[i].len);
    g_installed = false;
    g_base = nullptr;
    IndexReset();
}

}  // namespace hashlookup

#endif  // FEATURE_HASH_LOOKUP
