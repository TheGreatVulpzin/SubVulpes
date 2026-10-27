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
 *   SV_EVENT_LOST   lost is the number of records that the full queue dropped since the last delivery
 *
 * The host gives these records at a safe point, before the tick event, in the order in which they happened. The full
 * API is available in the callback. See docs/plugin-events.md. */
#pragma pack(push, 8)
typedef struct {
    SvEvent base;
    SvEntity player, human, actor;
    uint32_t lost, reserved;
} SvGameEvent;
#pragma pack(pop)

#endif
