#include "feature_config.h"

#if FEATURE_HASH_LOOKUP

// Assembly bridges from the engine's patched scan loops into the hash index.
//
// Each stub is entered by a jmp that overwrote the head of a linear search,
// and leaves through one of that same function's own labels with the node in
// the register the engine's loop would have left it in. Nothing else about the
// function changes, so latching, CVAR_INTERNAL gating, info validation, alias
// loop counting and every other behaviour stays literal engine code.
//
// Contract at every entry:
//   * esp is exactly what the engine had - the stubs restore it before
//     leaving, and the surrounding function still reads its arguments through
//     [esp+N].
//   * ebx / esi / edi / ebp hold whatever the engine put there. The cdecl call
//     preserves them, and each stub sets only the one register its re-entry
//     label expects. eax / ecx / edx are scratch: the loop we replaced
//     clobbered them too, and every re-entry label writes them before reading.
//
// GCC has no `naked` attribute on x86, so these are module-level asm. Jump
// targets are memory-indirect through globals rather than immediates: this is
// a DLL, and the engine addresses are only known once its module base is
// resolved at install time.

extern "C" {

// Filled by hashlookup::Install() from the resolved engine base.
void* hl_t_varvalue_found = nullptr;
void* hl_t_varvalue_miss = nullptr;
void* hl_t_varstring_found = nullptr;
void* hl_t_varstring_miss = nullptr;
void* hl_t_cvarget_found = nullptr;
void* hl_t_cvarget_miss = nullptr;
void* hl_t_cvarset2_found = nullptr;
void* hl_t_cvarset2_miss = nullptr;
void* hl_t_fullset_found = nullptr;
void* hl_t_fullset_miss = nullptr;
void* hl_t_cvarcmd_found = nullptr;
void* hl_t_cvarcmd_miss = nullptr;
void* hl_t_exec_cmd = nullptr;
void* hl_t_exec_alias = nullptr;
void* hl_t_exec_none = nullptr;
void* hl_t_cmdremove_resume = nullptr;

// Value of `offset cmd_functions`, i.e. the address of the list head - the
// operand of the one instruction the Cmd_RemoveCommand stub has to replay.
void* hl_v_cmd_functions = nullptr;

void hl_stub_varvalue(void);
void hl_stub_varstring(void);
void hl_stub_cvarget(void);
void hl_stub_cvarset2(void);
void hl_stub_fullset(void);
void hl_stub_cvarcmd(void);
void hl_stub_cmdexec(void);
void hl_stub_cmdremove(void);

}  // extern "C"

// One shape covers all six cvar sites: read the name the engine already has on
// its stack, hash it, and enter the function's own found / not-found label.
#define HL_CVAR_STUB(sym, esp_ofs, reg, tfound, tmiss) \
    asm(".text\n"                                      \
        ".globl _" #sym "\n"                           \
        "_" #sym ":\n"                                 \
        "    movl " #esp_ofs "(%esp), %eax\n"          \
        "    pushl %eax\n"                             \
        "    call _HashLookup_FindCvar\n"              \
        "    addl $4, %esp\n"                          \
        "    testl %eax, %eax\n"                       \
        "    jz 1f\n"                                  \
        "    movl %eax, %" #reg "\n"                   \
        "    jmp *_" #tfound "\n"                      \
        "1:  jmp *_" #tmiss "\n")

// Cvar_VariableValue / Cvar_VariableString: 4 pushes deep, name at [esp+0x14],
// match returned in edx.
HL_CVAR_STUB(hl_stub_varvalue,  0x14, edx, hl_t_varvalue_found,  hl_t_varvalue_miss);
HL_CVAR_STUB(hl_stub_varstring, 0x14, edx, hl_t_varstring_found, hl_t_varstring_miss);

// Cvar_Get / Cvar_Set2 / Cvar_FullSet: 0x84 of locals plus 4 pushes, name at
// [esp+0x98] (the same slot the engine's own loop reloads it from), match in
// ebp.
HL_CVAR_STUB(hl_stub_cvarget,  0x98, ebp, hl_t_cvarget_found,  hl_t_cvarget_miss);
HL_CVAR_STUB(hl_stub_cvarset2, 0x98, ebp, hl_t_cvarset2_found, hl_t_cvarset2_miss);
HL_CVAR_STUB(hl_stub_fullset,  0x98, ebp, hl_t_fullset_found,  hl_t_fullset_miss);

// Cvar_Command has already called Cmd_Argv(0) and spilled the result to
// [esp+0x10]; match in ebp.
HL_CVAR_STUB(hl_stub_cvarcmd,  0x10, ebp, hl_t_cvarcmd_found,  hl_t_cvarcmd_miss);

#undef HL_CVAR_STUB

// Cmd_ExecuteString searched cmd_functions and then cmd_alias back to back, so
// one stub replaces both loops. It reads cmd_argv[0] itself and enters the
// command label, the alias label, or the Cvar_Command fallthrough - each with
// esi set exactly as the corresponding loop would have left it.
asm(".text\n"
    ".globl _hl_stub_cmdexec\n"
    "_hl_stub_cmdexec:\n"
    "    call _HashLookup_FindExec\n"
    "    cmpl $1, %eax\n"
    "    je 1f\n"
    "    cmpl $2, %eax\n"
    "    je 2f\n"
    "    jmp *_hl_t_exec_none\n"
    "1:  movl _HashLookup_ExecNode, %esi\n"
    "    jmp *_hl_t_exec_cmd\n"
    "2:  movl _HashLookup_ExecNode, %esi\n"
    "    jmp *_hl_t_exec_alias\n");

// Cmd_RemoveCommand is the only destructive operation on any of the three
// lists. This stub does not replace its scan - it just drops the command index
// before the engine Z_Frees a node, then replays the single instruction it
// overwrote and lets the original code run. edx still holds the name argument
// the not-found path prints, so it is saved across the call.
asm(".text\n"
    ".globl _hl_stub_cmdremove\n"
    "_hl_stub_cmdremove:\n"
    "    pushl %edx\n"
    "    call _HashLookup_InvalidateCommands\n"
    "    popl %edx\n"
    "    movl _hl_v_cmd_functions, %ebp\n"
    "    jmp *_hl_t_cmdremove_resume\n");

#endif  // FEATURE_HASH_LOOKUP
