#pragma once

// ---------------------------------------------------------------------------
// Engine layout for the cvar/command hash index (SoF.exe 1.06a).
//
// The feature was ported from sof_buddy-server's hash_lookup module. Its RVAs
// and byte strings were read out of, and verified byte-for-byte against, the
// shipped server engines (sofplus 2012 SoF-spsv.exe and sofplus 2019
// sof-spsv16.exe). The same code layout is present in the retail client
// SoF.exe (ImageBase 0x20000000); every kOrig* byte string below was checked
// against the client image before this feature was enabled by default.
//
// The patcher re-checks kOrig* at runtime and refuses to touch a site that does
// not match, so a different build (or another mod that got there first)
// degrades to "feature off", never to a wild jump.
//
// SoF's engine image is non-relocatable and always lands at 0x20000000, which
// is why the rest of this repo writes engine addresses as absolute 0x2xxxxxxx.
// We still express them as RVAs and add the real module base at install time:
// it costs nothing and stays correct if an image ever does get relocated.
// ---------------------------------------------------------------------------

#include <cstdint>

namespace hashlookup {

// --- globals ---------------------------------------------------------------
constexpr unsigned kRvaCvarVars     = 0x24B1D8;  // cvar_t      *cvar_vars
constexpr unsigned kRvaCmdFunctions = 0x241840;  // cmd_function_t *cmd_functions
constexpr unsigned kRvaCmdAlias     = 0x243C54;  // cmdalias_t  *cmd_alias
constexpr unsigned kRvaCmdArgv      = 0x23F2E0;  // char        *cmd_argv[]

// --- node layouts ----------------------------------------------------------
// cvar_t (q_shared.h): name/string/latched/flags/command/modified/value/next.
// NOTE this is not stock Quake 2's layout - SoF's `value` sits at +0x18.
constexpr unsigned kCvarNameOfs = 0x00;
constexpr unsigned kCvarNextOfs = 0x1C;

// cmd_function_t: Z_Malloc(0x0C) in Cmd_AddCommand; `name` is the caller's
// own pointer (no CopyString), which is why the index may store it directly.
constexpr unsigned kCmdNextOfs = 0x00;
constexpr unsigned kCmdNameOfs = 0x04;

// cmdalias_t: Z_Malloc(0x28); `name` is an inline char[32], not a pointer.
constexpr unsigned kAliasNextOfs = 0x00;
constexpr unsigned kAliasNameOfs = 0x04;

// ---------------------------------------------------------------------------
// Patch sites.
//
// Every one of these is the *scan loop* inside an engine function, never the
// function entry. The shape is identical at each: the loop walks a linked list
// and leaves through one of two labels, with the matched node in a fixed
// register. We replace the loop head with a jmp to a stub that does the O(1)
// lookup and enters the very same label with the very same register set, so
// everything the function does before and after the search is untouched
// engine code.
// ---------------------------------------------------------------------------

// mov eax,[esp+14h] / mov ebp,[edx]
constexpr std::uint8_t kOrigVarLoop[]  = {0x8B, 0x44, 0x24, 0x14, 0x8B, 0x2A};
// mov ebp, ds:cvar_vars
constexpr std::uint8_t kOrigCvarGet[]  = {0x8B, 0x2D, 0xD8, 0xB1, 0x24, 0x20};
// test ebp,ebp / jz +66h / mov edx,[ebp+0]
constexpr std::uint8_t kOrigSetLoop[]  = {0x85, 0xED, 0x74, 0x66, 0x8B, 0x55, 0x00};
// mov ebx,[ebp+0] / mov ecx,eax
constexpr std::uint8_t kOrigCvarCmd[]  = {0x8B, 0x5D, 0x00, 0x8B, 0xC8};
// mov esi, ds:cmd_functions
constexpr std::uint8_t kOrigCmdExec[]  = {0x8B, 0x35, 0x40, 0x18, 0x24, 0x20};
// mov ebp, offset cmd_functions
constexpr std::uint8_t kOrigCmdRemove[] = {0xBD, 0x40, 0x18, 0x24, 0x20};

constexpr unsigned kRvaVarValueLoop  = 0x216DE;  // Cvar_VariableValue  @0x200216D0
constexpr unsigned kRvaVarStringLoop = 0x2176E;  // Cvar_VariableString @0x20021760
constexpr unsigned kRvaCvarGetLoop   = 0x21B54;  // Cvar_Get            @0x20021AE0
constexpr unsigned kRvaCvarSet2Loop  = 0x21D87;  // Cvar_Set2           @0x20021D70
constexpr unsigned kRvaFullSetLoop   = 0x221C7;  // Cvar_FullSet        @0x200221B0
constexpr unsigned kRvaCvarCmdLoop   = 0x224CF;  // Cvar_Command        @0x200224B0
constexpr unsigned kRvaCmdExecLoop   = 0x1950C;  // Cmd_ExecuteString   @0x200194F0
constexpr unsigned kRvaCmdRemoveHead = 0x191E8;  // Cmd_RemoveCommand   @0x200191E0

// Re-entry labels the stubs jump to (see README.md for the register contract).
constexpr unsigned kRvaVarValueFound  = 0x2174D;  // edx = cvar
constexpr unsigned kRvaVarValueMiss   = 0x21742;
constexpr unsigned kRvaVarStringFound = 0x217DC;  // edx = cvar
constexpr unsigned kRvaVarStringMiss  = 0x217D2;
constexpr unsigned kRvaCvarGetFound   = 0x21BE0;  // ebp = cvar
constexpr unsigned kRvaCvarGetMiss    = 0x21BC4;
constexpr unsigned kRvaCvarSet2Found  = 0x21E11;  // ebp = cvar
constexpr unsigned kRvaCvarSet2Miss   = 0x21DF1;
constexpr unsigned kRvaFullSetFound   = 0x22253;  // ebp = cvar
constexpr unsigned kRvaFullSetMiss    = 0x22231;
constexpr unsigned kRvaCvarCmdFound   = 0x2253C;  // ebp = cvar
constexpr unsigned kRvaCvarCmdMiss    = 0x22532;
constexpr unsigned kRvaCmdExecCmd     = 0x19531;  // esi = cmd_function_t
constexpr unsigned kRvaCmdExecAlias   = 0x1959E;  // esi = cmdalias_t
constexpr unsigned kRvaCmdExecNone    = 0x1958C;  // falls through to Cvar_Command
constexpr unsigned kRvaCmdRemoveResume = 0x191ED;

}  // namespace hashlookup
