#ifndef SV_GAME_WRITE_H
#define SV_GAME_WRITE_H

#include "sv_game.h"

#define SV_API_GAME_WRITE 2u
#define SV_GAME_WRITE_VERSION 1u

/* Message kinds: 0 is a server message for every client. Kind 4 reaches only admins, as the replies of the chat commands do. */
enum { SV_MESSAGE_SERVER = 0, SV_MESSAGE_CHAT = 4 };
/* The health range of the game: -100 is dead and 100 is full health. */
enum { SV_HEALTH_MIN = -100, SV_HEALTH_MAX = 100 };
/* The look selectors and the highest value of each, as the game makes them. */
enum { SV_LOOK_COUNT = 5, SV_TEAM_MAX = 7 };
/* A teleport accepts finite coordinates inside this limit. */
#define SV_POSITION_LIMIT 32768.0f

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
    /* Sends a text of 1 to 63 printable ASCII bytes to every client. */
    SvResult (SV_CALL *message)(SvOwner owner, uint32_t kind, const char *text);
    /* Adds the address of the player to the kick list and ends the connection. Bots have no connection. */
    SvResult (SV_CALL *kick)(SvOwner owner, SvEntity player);
    /* Adds money to an account and to each active player of it. The sum must fit in 32 bits. */
    SvResult (SV_CALL *add_money)(SvOwner owner, SvEntity account, int32_t amount);
    SvResult (SV_CALL *set_godmode)(SvOwner owner, SvEntity player, int32_t enabled);
    SvResult (SV_CALL *set_health)(SvOwner owner, SvEntity human, int32_t health);
    /* Moves every bone of a human on foot by the same offset. The velocity does not change. */
    SvResult (SV_CALL *teleport)(SvOwner owner, SvEntity human, SvVector position);
    /* Sets the admin flag of a player and of its connections, as the password login does. Bots cannot be admins. */
    SvResult (SV_CALL *set_admin)(SvOwner owner, SvEntity player, int32_t enabled);
    /* Sets the team of a player and of its human, from 0 to 7, and sends the player update. */
    SvResult (SV_CALL *set_team)(SvOwner owner, SvEntity player, int32_t team);
    /* Sets the five look selectors of a player and of its human, and sends the updates. */
    SvResult (SV_CALL *set_looks)(SvOwner owner, SvEntity player, const int32_t looks[SV_LOOK_COUNT]);
    /* Like message, but only the connection of this player gets the text. Other clients see an empty slot. */
    SvResult (SV_CALL *message_to)(SvOwner owner, uint32_t kind, SvEntity player, const char *text);
} SvGameWrite;
#pragma pack(pop)

#endif
