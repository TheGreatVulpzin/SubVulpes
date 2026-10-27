#ifndef SV_GAME_AI_H
#define SV_GAME_AI_H

/* Capability 3: AI control of bots (N20 in docs/legacy-modules.md).
 * This file is a stub. The owner of the area adds the methods after "version". Do not reorder fields. */
#include "sv_game.h"

#define SV_API_AI 3u
#define SV_GAME_AI_VERSION 1u

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
} SvGameAi;
#pragma pack(pop)

#endif
