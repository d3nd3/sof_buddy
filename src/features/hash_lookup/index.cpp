#include "feature_config.h"

#if FEATURE_HASH_LOOKUP

#include "index.h"
#include "engine.h"
#include "util.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace hashlookup {
namespace {

// ---------------------------------------------------------------------------
// Open-addressed table. Keys are borrowed pointers into strings the engine
// owns for the life of the process - cvar names are CopyString'd by Cvar_Get,
// command names belong to whoever called Cmd_AddCommand (the engine's own
// scans read them too, so they already have to outlive the command), and alias
// names live inside the alias node. Nothing here copies or frees a string.
//
// The stored hash makes a collision cost one 32-bit compare instead of a
// strcmp, which matters because the whole point is to be cheaper than the
// linear scan we replaced.
// ---------------------------------------------------------------------------
constexpr std::size_t kInitialCap = 1024;   // power of two
constexpr std::size_t kMaxCap     = 1u << 20;

// A list longer than this is taken as corruption rather than a real game
// state, and drops the index into its linear fallback instead of spinning.
constexpr unsigned kMaxListWalk = 1u << 20;

inline char Fold(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

/** FNV-1a. `fold` mirrors the engine's stricmp (ASCII A-Z only, matching
 *  msvcrt's C locale). `high` reports any byte >= 0x80, where our folding is
 *  no longer guaranteed to agree with the CRT's - those keys take the linear
 *  path instead of risking a wrong miss. */
std::uint32_t HashKey(const char* s, bool fold, bool* high) {
    std::uint32_t h = 2166136261u;
    bool saw_high = false;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
        unsigned char c = *p;
        if (c >= 0x80)
            saw_high = true;
        if (fold)
            c = static_cast<unsigned char>(Fold(static_cast<char>(c)));
        h = (h ^ c) * 16777619u;
    }
    if (high)
        *high = saw_high;
    return h;
}

bool KeyEqual(const char* a, const char* b, bool fold) {
    if (!fold)
        return std::strcmp(a, b) == 0;
    for (;; ++a, ++b) {
        char ca = Fold(*a);
        char cb = Fold(*b);
        if (ca != cb)
            return false;
        if (!ca)
            return true;
    }
}

struct Slot {
    std::uint32_t hash;
    std::uint32_t stamp;
    const char*   key;
    void*         node;
};

class Table {
public:
    bool broken = false;

    void SetFold(bool fold) { fold_ = fold; }

    void Clear() {
        if (slots_)
            std::memset(slots_, 0, cap_ * sizeof(Slot));
        count_ = 0;
    }

    void Release() {
        std::free(slots_);
        slots_ = nullptr;
        cap_ = 0;
        count_ = 0;
        broken = false;
    }

    void* Find(const char* key, std::uint32_t h) const {
        if (!slots_)
            return nullptr;
        std::size_t mask = cap_ - 1;
        for (std::size_t i = h & mask; slots_[i].key; i = (i + 1) & mask) {
            if (slots_[i].hash == h && KeyEqual(slots_[i].key, key, fold_))
                return slots_[i].node;
        }
        return nullptr;
    }

    /** Inserts, or replaces an older node under the same key. A slot already
     *  written during this same sync (`stamp`) is left alone: the sync walks
     *  the list newest-first, so the entry that is already there came from a
     *  node the engine's own scan would have reached first. */
    bool Put(const char* key, std::uint32_t h, void* node, std::uint32_t stamp) {
        if (!slots_ && !Alloc(kInitialCap))
            return false;
        std::size_t mask = cap_ - 1;
        std::size_t i = h & mask;
        for (; slots_[i].key; i = (i + 1) & mask) {
            if (slots_[i].hash == h && KeyEqual(slots_[i].key, key, fold_)) {
                if (slots_[i].stamp != stamp) {
                    slots_[i].key = key;
                    slots_[i].node = node;
                    slots_[i].stamp = stamp;
                }
                return true;
            }
        }
        slots_[i].hash = h;
        slots_[i].stamp = stamp;
        slots_[i].key = key;
        slots_[i].node = node;
        ++count_;
        // Keep the load factor under 3/4 so probe runs stay short.
        if (count_ * 4 >= cap_ * 3)
            return Grow();
        return true;
    }

private:
    Slot*       slots_ = nullptr;
    std::size_t cap_ = 0;
    std::size_t count_ = 0;
    bool        fold_ = false;

    bool Alloc(std::size_t cap) {
        Slot* s = static_cast<Slot*>(std::calloc(cap, sizeof(Slot)));
        if (!s)
            return false;
        slots_ = s;
        cap_ = cap;
        count_ = 0;
        return true;
    }

    /** Rehash into a table twice the size. No key comparisons needed: every
     *  key in the old table is already unique, so probing to the first free
     *  slot is enough. */
    bool Grow() {
        if (cap_ >= kMaxCap)
            return true;  // stop growing; degraded probes beat failing here
        Slot*       old = slots_;
        std::size_t oldcap = cap_;
        if (!Alloc(oldcap * 2)) {
            slots_ = old;
            cap_ = oldcap;
            return false;
        }
        std::size_t mask = cap_ - 1;
        for (std::size_t j = 0; j < oldcap; ++j) {
            if (!old[j].key)
                continue;
            std::size_t i = old[j].hash & mask;
            while (slots_[i].key)
                i = (i + 1) & mask;
            slots_[i] = old[j];
            ++count_;
        }
        std::free(old);
        return true;
    }
};

// ---------------------------------------------------------------------------
// One index per engine list.
// ---------------------------------------------------------------------------
struct Index {
    void**        head = nullptr;   // &cvar_vars / &cmd_functions / &cmd_alias
    unsigned      next_ofs = 0;
    unsigned      name_ofs = 0;
    bool          name_inline = false;  // alias names are a char[32], not char*
    bool          fold = false;
    bool          skip_tilde = false;   // never index sofplus temporaries
    Table         tbl;
    void*         last_head = nullptr;  // raw head as of the last sync (fast path)
    void*         anchor = nullptr;     // first *indexed* node; the walk stops here
    std::uint32_t stamp = 0;

    void Bind(void** h, unsigned nofs, unsigned mofs, bool inl, bool ci, bool tilde) {
        head = h;
        next_ofs = nofs;
        name_ofs = mofs;
        name_inline = inl;
        fold = ci;
        skip_tilde = tilde;
        tbl.SetFold(ci);
        Invalidate();
    }

    void Release() {
        tbl.Release();
        head = nullptr;
        anchor = nullptr;
        last_head = nullptr;
    }

    void Invalidate() {
        tbl.Clear();
        tbl.broken = false;
        anchor = nullptr;
        last_head = nullptr;
    }

    void* NextOf(void* n) const {
        return *reinterpret_cast<void**>(static_cast<char*>(n) + next_ofs);
    }

    const char* KeyOf(void* n) const {
        char* p = static_cast<char*>(n) + name_ofs;
        return name_inline ? p : *reinterpret_cast<const char**>(p);
    }

    /** True for names the engine does not own for the life of the process.
     *
     *  sofplus's "temporary" cvars are the `~`-prefixed ones, and it destroys
     *  them itself: spsv.dll @0x10005750 walks cvar_vars and, for every node
     *  whose name starts with '~', Z_Frees the name, the string, the latched
     *  string and the node, unlinking it mid-list (@0x100057C3) - and
     *  @0x10005800 / @0x10005890 unlink and re-splice them for scoping. All of
     *  that is invisible to a head-pointer comparison, and it frees the very
     *  string this table borrows as a key. So these never enter the table at
     *  all; they are served by the exact linear walk instead, which is cheap
     *  because sofplus keeps temporaries at the front of the list. */
    bool Volatile(const char* key) const { return skip_tilde && key[0] == '~'; }

    /** Brings the table level with the list.
     *
     *  Fast path is one compare against the raw head: prepend-only additions
     *  are the only thing that has to be picked up. `anchor` is deliberately
     *  *not* the raw head but the first node actually indexed - a permanent
     *  cvar, which nothing ever frees, unlinks or reorders. Anchoring on the
     *  raw head would dangle as soon as sofplus freed a head temporary, and
     *  would then force a full re-index on every sync. */
    void Sync() {
        if (!head || tbl.broken)
            return;
        void* h = *head;
        if (h == last_head)
            return;

        ++stamp;
        unsigned walked = 0;
        void* new_anchor = nullptr;
        for (void* n = h; n && n != anchor; n = NextOf(n)) {
            if (++walked > kMaxListWalk) {
                tbl.broken = true;
                PrintOut(PRINT_BAD,
                         "[hash_lookup] list walk exceeded %u nodes - falling back to linear\n",
                         kMaxListWalk);
                return;
            }
            const char* key = KeyOf(n);
            if (!key || Volatile(key))
                continue;
            if (!new_anchor)
                new_anchor = n;
            bool high = false;
            std::uint32_t hv = HashKey(key, fold, &high);
            if (!tbl.Put(key, hv, n, stamp)) {
                tbl.broken = true;
                PrintOut(PRINT_BAD, "[hash_lookup] out of memory - falling back to linear\n");
                return;
            }
        }
        last_head = h;
        if (new_anchor)
            anchor = new_anchor;
    }

    /** The engine's own scan, kept for the cases the table must not answer:
     *  a broken table, and keys whose case-folding we cannot guarantee matches
     *  the CRT's. */
    void* LinearFind(const char* key) const {
        if (!head)
            return nullptr;
        unsigned walked = 0;
        for (void* n = *head; n; n = NextOf(n)) {
            if (++walked > kMaxListWalk)
                return nullptr;
            const char* k = KeyOf(n);
            if (k && KeyEqual(k, key, fold))
                return n;
        }
        return nullptr;
    }

    void* Find(const char* key) {
        // Never served from the table - the node can be freed under us.
        if (Volatile(key))
            return LinearFind(key);
        Sync();
        bool high = false;
        std::uint32_t hv = HashKey(key, fold, &high);
        if (tbl.broken || (fold && high))
            return LinearFind(key);
        return tbl.Find(key, hv);
    }
};

Index g_cvars;
Index g_cmds;
Index g_aliases;

char** g_cmd_argv = nullptr;
bool   g_bound = false;

/** Cvar_FindVar's exact behaviour, quirk included.
 *
 *  Every inlined copy of the search in SoF.exe carries the same oddity: after
 *  the name compare fails it also matches when the requested name is exactly
 *  "matrix" and the node's name is exactly "timescale". So `Cvar_FindVar
 *  ("matrix")` returns whichever of the two cvars sits nearer the head. It is
 *  a one-name special case, so the hash path is only bypassed for that name. */
void* LinearFindCvarQuirk() {
    if (!g_cvars.head)
        return nullptr;
    unsigned walked = 0;
    for (void* n = *g_cvars.head; n; n = g_cvars.NextOf(n)) {
        if (++walked > kMaxListWalk)
            return nullptr;
        const char* k = g_cvars.KeyOf(n);
        if (!k)
            continue;
        if (std::strcmp(k, "matrix") == 0 || std::strcmp(k, "timescale") == 0)
            return n;
    }
    return nullptr;
}

}  // namespace

void IndexBind(unsigned char* base) {
    g_cvars.Bind(reinterpret_cast<void**>(base + kRvaCvarVars),
                 kCvarNextOfs, kCvarNameOfs, /*inline=*/false, /*fold=*/false,
                 /*skip_tilde=*/true);
    // Cmd_ExecuteString matches commands and aliases with stricmp, so both of
    // those indexes are case-insensitive. (Cmd_AddCommand's duplicate check is
    // case-*sensitive*; that scan is deliberately left alone - see README.md.)
    g_cmds.Bind(reinterpret_cast<void**>(base + kRvaCmdFunctions),
                kCmdNextOfs, kCmdNameOfs, /*inline=*/false, /*fold=*/true,
                /*skip_tilde=*/false);
    g_aliases.Bind(reinterpret_cast<void**>(base + kRvaCmdAlias),
                   kAliasNextOfs, kAliasNameOfs, /*inline=*/true, /*fold=*/true,
                   /*skip_tilde=*/false);
    g_cmd_argv = reinterpret_cast<char**>(base + kRvaCmdArgv);
    g_bound = true;
}

void IndexReset() {
    g_bound = false;
    g_cmd_argv = nullptr;
    g_cvars.Release();
    g_cmds.Release();
    g_aliases.Release();
}

}  // namespace hashlookup

// ---------------------------------------------------------------------------
// Entry points for the assembly stubs.
//
// force_align_arg_pointer: these are entered from engine code with whatever
// stack alignment it happened to have, while GCC otherwise assumes 16-byte
// alignment on entry and is free to emit aligned SSE spills.
// ---------------------------------------------------------------------------
extern "C" {
void* HashLookup_ExecNode = nullptr;
}

extern "C" __attribute__((force_align_arg_pointer))
void* HashLookup_FindCvar(const char* name) {
    using namespace hashlookup;
    if (!g_bound || !name)
        return nullptr;
    if (name[0] == 'm' && std::strcmp(name, "matrix") == 0)
        return LinearFindCvarQuirk();
    return g_cvars.Find(name);
}

extern "C" __attribute__((force_align_arg_pointer))
int HashLookup_FindExec(void) {
    using namespace hashlookup;
    HashLookup_ExecNode = nullptr;
    if (!g_bound || !g_cmd_argv)
        return 0;
    const char* key = *g_cmd_argv;
    if (!key)
        return 0;
    if (void* cmd = g_cmds.Find(key)) {
        HashLookup_ExecNode = cmd;
        return 1;
    }
    if (void* alias = g_aliases.Find(key)) {
        HashLookup_ExecNode = alias;
        return 2;
    }
    return 0;
}

extern "C" __attribute__((force_align_arg_pointer))
void HashLookup_InvalidateCommands(void) {
    hashlookup::g_cmds.Invalidate();
}

#endif  // FEATURE_HASH_LOOKUP
