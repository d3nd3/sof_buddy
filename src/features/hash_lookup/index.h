#pragma once

#include "feature_config.h"

#if FEATURE_HASH_LOOKUP

// Hash index over the engine's cvar / command / alias lists.
//
// The engine keeps all three as singly-linked lists that, while the game is
// running, are only ever *prepended* to. That is checked over the whole image,
// not just the list heads - see README.md: there is exactly one store to any
// cvar_t.next in the binary (the prepend itself), nothing takes the address of
// cvar_vars or cmd_alias, and the only Z_Free of a cvar `name` is in an exit
// teardown. Each list does have a bulk destructor, but all three are atexit
// handlers (which is why they appear to have no callers). So the index is
// cheap to keep coherent: a lookup compares the current head against the head
// we last indexed and, when they differ, walks only the newly prepended
// prefix.
//
// Commands are the one list with a destructive operation - Cmd_RemoveCommand
// Z_Frees the node - so that function is patched to invalidate the whole
// command index first. Removals are rare; a full rebuild there is far simpler
// to reason about than a surgical erase, and it can never leave a freed node
// reachable from the table.
//
// These entry points are what the assembly stubs call. They run on the
// engine's own thread, inside the engine function whose scan they replaced.

extern "C" {

/** Cvar lookup by name, exactly matching the engine's inlined Cvar_FindVar
 *  (including its "matrix" -> `timescale` alias). Null when absent. */
void* HashLookup_FindCvar(const char* name);

/** Cmd_ExecuteString's combined search over cmd_functions then cmd_alias,
 *  keyed on cmd_argv[0], case-insensitive as the engine's stricmp is.
 *  Returns 0 (neither), 1 (command) or 2 (alias); the node goes in
 *  HashLookup_ExecNode. */
int HashLookup_FindExec(void);

/** Node found by the most recent HashLookup_FindExec. */
extern void* HashLookup_ExecNode;

/** Drops the command index; the next lookup rebuilds it by walking the list.
 *  Called from the Cmd_RemoveCommand stub, before the engine frees a node. */
void HashLookup_InvalidateCommands(void);

}  // extern "C"

namespace hashlookup {

/** Points the index at the engine image and empties every table. */
void IndexBind(unsigned char* module_base);

/** Forgets everything, including the module base (detach). */
void IndexReset();

}  // namespace hashlookup

#endif  // FEATURE_HASH_LOOKUP
