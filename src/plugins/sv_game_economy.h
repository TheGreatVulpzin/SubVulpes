#ifndef SV_GAME_ECONOMY_H
#define SV_GAME_ECONOMY_H

/* Capability 4: companies, prices and stocks (N21 in docs/legacy-modules.md).
 * This file is a stub. The owner of the area adds the methods after "version". Do not reorder fields. */
#include "sv_game.h"

#define SV_API_ECONOMY 4u
#define SV_GAME_ECONOMY_VERSION 1u

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
} SvGameEconomy;
#pragma pack(pop)

#endif
