#ifndef SV_GAME_ENGINE_H
#define SV_GAME_ENGINE_H

/* Capability 8: engine parameters of object types (N17 in docs/legacy-modules.md).
 * This file is a stub. The owner of the area adds the methods after "version". Do not reorder fields. */
#include "sv_game.h"

#define SV_API_ENGINE 8u
#define SV_GAME_ENGINE_VERSION 1u

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
} SvGameEngine;
#pragma pack(pop)

#endif
