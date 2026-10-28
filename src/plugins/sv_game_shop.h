#ifndef SV_GAME_SHOP_H
#define SV_GAME_SHOP_H

#include "sv_game.h"

#define SV_API_SHOP 9u
#define SV_GAME_SHOP_VERSION 1u
#define SV_SHOP_ROWS 10u
#define SV_SHOP_BUILDINGS 4096u
#define SV_SHOP_PRICE_MAX 0xffffff

enum { SV_PHONE_IDLE, SV_PHONE_DIAL, SV_PHONE_RINGING, SV_PHONE_TALK };
enum { SV_PHONE_DIGIT_DECISION = 1u, SV_PHONE_CALL_DECISION };
enum { SV_CATALOG_VEHICLES = 1u, SV_CATALOG_ITEMS, SV_CATALOG_CLOTHES };
enum { SV_SHOP_SAFE_DECISIONS = 1u, SV_SHOP_SAFE_PURCHASES = 2u };

#pragma pack(push, 8)
/* A listener checks base.size before it reads the fields after base. */
typedef struct {
    SvEvent base;
    SvEntity phone;
    int32_t number;
    SvEntity human;
} SvPhoneEvent;

typedef struct {
    uint32_t size;
    SvEntity phone, human, peer;
    int32_t state, number, info, cooldown, ring_ticks;
} SvPhoneState;

typedef struct { int32_t type, price, color; } SvShopRow;
typedef struct {
    uint32_t size, kind, capacity, count;
    SvShopRow rows[SV_SHOP_ROWS];
} SvShopCatalog;
typedef struct {
    uint32_t size, count;
    SvShopRow rows[SV_SHOP_ROWS];
} SvShopCatalogWrite;

typedef struct {
    uint32_t size, version;
    SvResult (SV_CALL *phone)(SvOwner owner, SvEntity phone, SvPhoneState *out);
    SvResult (SV_CALL *phone_idle)(SvOwner owner, SvEntity phone);
    SvResult (SV_CALL *catalog)(SvOwner owner, uint32_t building, SvShopCatalog *out);
    SvResult (SV_CALL *catalog_replace)(SvOwner owner, uint32_t building, const SvShopCatalogWrite *input);
} SvGameShop;
#pragma pack(pop)

typedef SvResult (*SvShopDecide)(const SvEvent *event);
typedef int (*SvShopWriteContext)(SvOwner owner);
/* Pass the shared sv_plugins_decide and sv_plugins_write_context, and the reserved phone bits. */
int sv_shop_install(SvShopDecide decide, SvShopWriteContext write_context, uint32_t digit_kind,
    uint32_t call_kind, uint32_t features);
/* The core calls this only at a kind-2 safe point, after a bounded digit append or before call lookup. */
SvResult sv_shop_phone_decision(uint32_t decision, SvEntity phone, SvEntity human);
/* Close links before the core retires a phone reference. */
SvResult sv_shop_phone_retire(SvEntity phone);
/* Register the phone scalar rows with the shared property mechanism when available. */
SvResult sv_shop_props_install(void);
/* The phone decisions cannot run a plugin inside the simulation, so the core only notes a phone, and the host asks the plugins
 * at the start of the next tick (sv_shop_poll). When no plugin subscribed to the event, nothing waits.
 *
 * sv_shop_phone_digit_added: the core calls it right after it appended a digit to the number of a phone.
 * sv_shop_phone_call_wait: the core calls it where it would dial a number of 4 digits. A result of 1 means that the phone
 * waits for the decision, and the core must not dial in this tick. The core asks again in the next tick, and then the
 * result is 0 unless a plugin claimed the number. A claimed phone goes back to idle.
 * sv_shop_poll: the host calls it at the start of each tick, outside the simulation. */
void sv_shop_phone_digit_added(int32_t item);
int sv_shop_phone_call_wait(int32_t item);
void sv_shop_poll(void);

#endif
