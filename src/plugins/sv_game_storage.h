#ifndef SV_GAME_STORAGE_H
#define SV_GAME_STORAGE_H

/* Capability 7: storage of the plugin (N18 in docs/legacy-modules.md).
 * This file is a stub. The owner of the area adds the methods after "version". Do not reorder fields. */
#include "sv_game.h"

#define SV_API_STORAGE 7u
#define SV_GAME_STORAGE_VERSION 1u

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
} SvGameStorage;
#pragma pack(pop)

#endif
