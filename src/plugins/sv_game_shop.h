#ifndef SV_GAME_SHOP_H
#define SV_GAME_SHOP_H

/* Capability 9: shop catalogs and phone purchases (N15 in docs/legacy-modules.md).
 * This file is a stub. The owner of the area adds the methods after "version". Do not reorder fields. */
#include "sv_game.h"

#define SV_API_SHOP 9u
#define SV_GAME_SHOP_VERSION 1u

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
} SvGameShop;
#pragma pack(pop)

#endif
