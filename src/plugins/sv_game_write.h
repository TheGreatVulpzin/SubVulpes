#ifndef SV_GAME_WRITE_H
#define SV_GAME_WRITE_H

#include "sv_game.h"

#define SV_API_GAME_WRITE 2u
#define SV_GAME_WRITE_VERSION 1u

/* Message kinds: 0 is a server message for every client. Kind 4 reaches only admins, as the replies of the chat commands do. */
enum { SV_MESSAGE_SERVER = 0, SV_MESSAGE_ADMIN = 4 };
/* The health range of the game: -100 is dead and 100 is full health. */
enum { SV_HEALTH_MIN = -100, SV_HEALTH_MAX = 100 };
/* The look selectors and the highest value of each, as the game makes them. */
enum { SV_LOOK_COUNT = 5, SV_TEAM_MAX = 7, SV_ITEM_TYPE_COUNT = 27, SV_ITEM_SLOT_COUNT = 7,
    SV_OBJECT_TYPE_COUNT = 12, SV_OBJECT_COLOR_MAX = 5, SV_STUN_MAX = 3600 };
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
    /* Makes an item of a type from 0 to 26, except 26 (newspapers belong to the news system), at rest at the position.
     * The item reference goes to *item only on success. SV_LIMIT means that all item slots are in use. */
    SvResult (SV_CALL *item_create)(SvOwner owner, uint32_t type, SvVector position, SvEntity *item);
    /* Ends the life of an item. The game removes it in its next item lifetime pass. It leaves its human and its parent
     * item, and its children leave it. A key takes its vehicle with it. */
    SvResult (SV_CALL *item_remove)(SvOwner owner, SvEntity item);
    /* Puts an item in a slot of a human (0 and 1 hands, 2 to 6 pockets). SV_CONFLICT when the slot rules refuse it. */
    SvResult (SV_CALL *item_equip)(SvOwner owner, SvEntity item, SvEntity human, uint32_t slot);
    /* Puts a child item in a parent item, such as a magazine in a gun. The child must have no parent item.
     * SV_CONFLICT when the types do not fit or the parent is full. */
    SvResult (SV_CALL *item_attach)(SvOwner owner, SvEntity parent, SvEntity child);
    /* Takes an item out of its human or its parent item. SV_STATE when it has neither. */
    SvResult (SV_CALL *item_detach)(SvOwner owner, SvEntity item);
    /* Makes an object of a type from 0 to 11 (cars, trucks, the helicopter) at the position, turned by the yaw angle
     * in radians. The color is 0 to 5. Types 12 and 13 are the train pieces, which only the track system makes.
     * The reference goes to *object only on success. SV_LIMIT means that all 1,024 object slots are in use. */
    SvResult (SV_CALL *object_create)(SvOwner owner, uint32_t type, SvVector position, float yaw, int32_t color,
        SvEntity *object);
    /* Ends the life of an object. The game removes it in its next object lifetime pass, and the humans inside get out.
     * SV_DENIED for a train piece. */
    SvResult (SV_CALL *object_remove)(SvOwner owner, SvEntity object);
    /* Sets the bleeding flag of a human to 0 or 1. A bleeding body loses health over time. */
    SvResult (SV_CALL *set_bleeding)(SvOwner owner, SvEntity human, int32_t enabled);
    /* Sets the stun timer of a human, from 0 to SV_STUN_MAX ticks. The torso is weak while it runs, so the body staggers.
     * The game counts it down by 1 each tick. */
    SvResult (SV_CALL *set_stun)(SvOwner owner, SvEntity human, int32_t ticks);
    /* Makes a bot player on foot, named NPC, in team 6 like the bots of the game, with a human at the position turned by the
     * yaw angle. It has no route and no end time. Both references go out only on success. SV_LIMIT means that no player
     * slot, human slot or bond is free, and then nothing is left behind. */
    SvResult (SV_CALL *npc_create)(SvOwner owner, SvVector position, float yaw, SvEntity *player, SvEntity *human);
    /* Asks the game to remove a bot. Its bot expiry removes the human and the player in the next world logic pass.
     * SV_DENIED for a player who is not a bot. */
    SvResult (SV_CALL *npc_remove)(SvOwner owner, SvEntity player);
} SvGameWrite;
#pragma pack(pop)

#endif
