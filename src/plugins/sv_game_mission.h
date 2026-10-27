#ifndef SV_GAME_MISSION_H
#define SV_GAME_MISSION_H

/* Capability 5: missions and the scenario choice (N22 in docs/legacy-modules.md).
 * This file is a stub. The owner of the area adds the methods after "version". Do not reorder fields. */
#include "sv_game.h"

#define SV_API_MISSION 5u
#define SV_GAME_MISSION_VERSION 1u

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
} SvGameMission;
#pragma pack(pop)

#endif
