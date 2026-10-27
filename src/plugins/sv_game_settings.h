#ifndef SV_GAME_SETTINGS_H
#define SV_GAME_SETTINGS_H

/* Capability 6: settings of the core (N14 in docs/legacy-modules.md).
 * This file is a stub. The owner of the area adds the methods after "version". Do not reorder fields. */
#include "sv_game.h"

#define SV_API_SETTINGS 6u
#define SV_GAME_SETTINGS_VERSION 1u

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
} SvGameSettings;
#pragma pack(pop)

#endif
