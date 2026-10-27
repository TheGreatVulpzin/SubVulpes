#ifndef SV_GAME_EVENTS_H
#define SV_GAME_EVENTS_H

#include "sv_game.h"

/* A game event record. The first field is the SvEvent of the plugin ABI version 1, with its size set to
 * sizeof(SvGameEvent), so a plugin that knows only SvEvent reads the same first 12 bytes. A plugin must check the size
 * before it reads the fields that follow.
 *
 * The references in the record are made when the event happens. A reference that is stale at delivery (the human is
 * gone, for example) gives SV_NOT_FOUND from the read API, as any stale reference does. A field that the event does not
 * have is 0.
 *
 *   SV_EVENT_SPAWN  player and human of the new body
 *   SV_EVENT_DEATH  human that died, its player, and in actor the human that caused the death when the game knows it
 *   SV_EVENT_LEAVE  player that left
 *   SV_EVENT_JOIN   decision. player is the new player and account its account. SV_DENIED drops the player, as a ban does
 *   SV_EVENT_LOST   lost is the number of records that the full queue dropped since the last delivery
 *
 * The host gives these records at a safe point, before the tick event, in the order in which they happened. The full
 * API is available in the callback. See docs/plugin-events.md. */
#pragma pack(push, 8)
typedef struct {
    SvEvent base;
    SvEntity player, human, actor, account;
    uint32_t lost, reserved;
} SvGameEvent;

/* A chat text: the player that typed it and the text, at most 63 bytes. It is a decision at a safe point. The plugin
 * answers SV_OK to let the text go to the chat, or SV_DENIED to drop it. A text that starts with "/" is a command, and the
 * command system handles it, so it gives no chat event. */
typedef struct {
    SvEvent base;
    SvEntity player;
    char text[64];
} SvChatEvent;

/* A damage that is about to happen, inside the simulation. This is the dangerous kind (kind 3 in docs/plugin-events.md).
 * Only a plugin with `unsafe_events=1` in its plugin.ini may subscribe to it. The callback runs in the middle of a
 * calculation of the game, so it can only read. Every write call, `command` and `subscribe` return SV_STATE.
 *
 * The callback answers SV_OK to allow the damage, or SV_DENIED to cancel it (the amount becomes 0). To change the amount, the
 * plugin writes `new_amount` and answers SV_OK. The host passes the record by const pointer, but the memory is writable and
 * `new_amount` is the only field that the plugin may write. A `new_amount` below 0 or above 100000 is ignored. The next
 * plugin sees the new amount in `amount`. The amount is the damage after the scales of the property table.
 *
 * A decision should take a few microseconds. A plugin that takes more than 200 microseconds more than 10 times in one
 * minute is disabled at the next safe point. */
enum { SV_DAMAGE_BULLET = 1, SV_DAMAGE_VEHICLE = 2, SV_DAMAGE_OBJECT = 3 };
#define SV_DAMAGE_MAX 100000
typedef struct {
    SvEvent base;
    SvEntity attacker;     /* The human of the shooter for a bullet. 0 when the game does not know it. */
    SvEntity human;        /* The human that takes the damage, or 0. */
    SvEntity object;       /* The object that takes the damage, or 0. */
    uint32_t source;       /* SV_DAMAGE_BULLET, SV_DAMAGE_VEHICLE or SV_DAMAGE_OBJECT. */
    int32_t amount;        /* The damage before this plugin. */
    int32_t new_amount;    /* The only field that the plugin may write. It starts equal to amount. */
    uint32_t reserved;
} SvDamageEvent;
#pragma pack(pop)

#endif
