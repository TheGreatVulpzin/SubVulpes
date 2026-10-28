#ifndef SV_GAME_ENGINE_H
#define SV_GAME_ENGINE_H

#include "sv_game.h"

#define SV_API_ENGINE 8u
#define SV_GAME_ENGINE_VERSION 1u
#define SV_ENGINE_NAME_SIZE 48u
#define SV_ENGINE_OBJECT_TYPES 14u
#define SV_ENGINE_GLOBAL UINT32_MAX

enum { SV_ENGINE_FLOAT = 1u, SV_ENGINE_INT, SV_ENGINE_BOOL };
enum { SV_ENGINE_PER_TYPE = 1u, SV_ENGINE_GLOBAL_SCOPE };
enum { SV_ENGINE_POWER, SV_ENGINE_RAGDOLL_RESET, SV_ENGINE_DEATH_START, SV_ENGINE_PARAM_COUNT };
enum { SV_ENGINE_OWNER_CLEANUP = 1u, SV_ENGINE_GLOBAL_CONSUMERS = 2u };

#pragma pack(push, 8)
typedef struct {
    uint32_t size, type;
    union { float real; int32_t integer; } value;
} SvEngineValue;

typedef struct {
    uint32_t size, index, scope;
    char name[SV_ENGINE_NAME_SIZE];
    SvEngineValue minimum, maximum, default_value;
} SvEngineInfo;

typedef struct {
    uint32_t size, version;
    SvResult (SV_CALL *describe)(SvOwner owner, uint32_t index, uint32_t object_type, SvEngineInfo *out);
    SvResult (SV_CALL *get)(SvOwner owner, const char *name, uint32_t object_type, SvEngineValue *out);
    SvResult (SV_CALL *set)(SvOwner owner, const char *name, uint32_t object_type, const SvEngineValue *value);
} SvGameEngine;
#pragma pack(pop)

/* Capture defaults after native type setup. Install only the core seams that exist. */
int sv_engine_install(uint32_t features);
void sv_engine_release_owner(SvOwner owner);
int32_t sv_engine_integer(uint32_t index, int32_t fallback);

#endif
