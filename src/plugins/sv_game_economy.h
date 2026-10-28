#ifndef SV_GAME_ECONOMY_H
#define SV_GAME_ECONOMY_H

/* Company snapshots and balanced stock changes. */
#include "sv_game.h"

#define SV_API_ECONOMY 4u
#define SV_GAME_ECONOMY_VERSION 1u
#define SV_COMPANY_COUNT 5u

#pragma pack(push, 8)
typedef struct {
    uint32_t size;
    int32_t shares, price_int, stake;
    float price, open_price;
} SvCompany;

typedef struct {
    uint32_t size, version;
    SvResult (SV_CALL *company)(SvOwner owner, uint32_t company, SvCompany *out);
    SvResult (SV_CALL *set_price)(SvOwner owner, uint32_t company, float price);
    SvResult (SV_CALL *set_shares)(SvOwner owner, uint32_t company, int32_t shares);
    SvResult (SV_CALL *add_stocks)(SvOwner owner, SvEntity player, int32_t amount);
} SvGameEconomy;
#pragma pack(pop)

#endif
