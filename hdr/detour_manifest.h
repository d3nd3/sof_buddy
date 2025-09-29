#ifndef DETOUR_MANIFEST_H
#define DETOUR_MANIFEST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Detour patch type constants
#define DETOUR_PATCH_JMP 0
#define DETOUR_PATCH_PUSH_RET 1
#define DETOUR_PATCH_NOP_JMP 2
#define DETOUR_PATCH_NOP_NOP_JMP 3
#define DETOUR_PATCH_STC_JC 4
#define DETOUR_PATCH_CLC_JNC 5
// Direct CALL (E8) patch helper. Used with DETOUR_TYPE_RAW to indicate the
// manifest entry should be applied by writing a relative CALL instruction.
#define DETOUR_PATCH_CALL 6

typedef enum DetourManifestTarget {
    DETOUR_MANIFEST_SOF_EXE,
    DETOUR_MANIFEST_REF_GL,
    DETOUR_MANIFEST_GAME_X86,
    DETOUR_MANIFEST_PLAYER,
    DETOUR_MANIFEST_UNKNOWN
} DetourManifestTarget;

typedef enum DetourType {
    DETOUR_TYPE_PTR,
    // DETOUR_TYPE_RAW: direct memory patch (no trampoline). Use for simple
    // WriteE8Call/WriteE9Jmp-style patches where we do not need a trampoline.
    // These are applied directly by writing instructions into the target
    // address rather than creating a trampoline via DetourCreate.
    DETOUR_TYPE_RAW,
    DETOUR_TYPE_MOD
} DetourType;

typedef struct DetourDefinition {
    const char *name;           // Unique detour name
    DetourType type;            // PTR or MOD
    const char *target_module;  // For MOD type: module name
    const char *target_proc;    // For MOD type: procedure name, for PTR type: address expression
    // Preferred: a short prefix indicating which wrapper namespace to use
    // when resolving the implementation symbol (e.g. "exe", "ref", "game").
    // The concrete function pointer is registered at static init time via
    // `detour_manifest_register_detour_function(name, func_ptr)` (the tool
    // will emit those registrations). Keeping a prefix (rather than an
    // explicit pointer) ensures generated symbols follow a consistent naming
    // convention and are validated at runtime.
    const char *wrapper_prefix;  // e.g. "exe", "ref", "game"
    int patch_type;             // Detour patch type (DETOUR_TYPE_JMP, etc.)
    int detour_len;             // Length of detour
    const char *description;    // Human readable description
    const char *feature_name;   // Feature this detour belongs to
} DetourDefinition;

void detour_manifest_init(void);
// Register a pointer to the original function/storage for a detour name.
// Callers pass the address of a file-scope pointer that will hold the
// original function pointer when a detour is applied (used for removal/restores).
void detour_manifest_register_original_storage(const char *name, void **storage);

// Register a detour implementation function pointer by name. This maps a
// logical detour name to its implementation pointer (used for lookup/validation).
void detour_manifest_register_detour_function(const char *name, void *func_ptr);
// Return the internal trampoline pointer for a named detour (or NULL if none).
void *detour_manifest_get_internal_trampoline(const char *name);
void detour_manifest_load_feature(const char *feature_name);
void detour_manifest_apply_feature(const char *feature_name, DetourManifestTarget target, const char *phase);
void detour_manifest_apply_all(DetourManifestTarget target, const char *phase);
void detour_manifest_add_definition(const DetourDefinition *def);
// Queue a static array of DetourDefinition entries for import during
// `detour_manifest_init()`. This is safe to call from static initializers
// because the manifest will flush the queued arrays explicitly at init time.
void detour_manifest_queue_definitions(const DetourDefinition *defs, int count);

// Feature initialization system
typedef void (*FeatureInitFunc)(void);
typedef void (*FeatureRegisterFunc)(void);

void feature_system_register_startup(const char *feature_name, FeatureInitFunc func);
void feature_system_register_early_init(const char *feature_name, FeatureInitFunc func);
void feature_system_register_detours(const char *feature_name, FeatureRegisterFunc func);
void feature_system_register_late_init(const char *feature_name, FeatureInitFunc func);
void feature_system_register_feature(const char *feature_name);
void feature_system_execute_startup(void);
void feature_system_execute_early_init(void);
void feature_system_execute_detours(void);
void feature_system_execute_late_init(void);

#ifdef __cplusplus
}
#endif

#endif
 