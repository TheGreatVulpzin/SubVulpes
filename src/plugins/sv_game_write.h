#ifndef SV_GAME_WRITE_H
#define SV_GAME_WRITE_H

#include "sv_game.h"

#define SV_API_GAME_WRITE 2u
#define SV_GAME_WRITE_VERSION 1u

/* Message kinds: 0 is a server message for every client. Kind 4 reaches only admins, as the replies of the chat commands do. */
enum { SV_MESSAGE_SERVER = 0, SV_MESSAGE_ADMIN = 4 };
/* Sizes of the things that a plugin can make, as the game has them. */
enum { SV_LOOK_COUNT = 5, SV_ITEM_TYPE_COUNT = 27, SV_ITEM_SLOT_COUNT = 7, SV_OBJECT_TYPE_COUNT = 12,
    SV_OBJECT_COLOR_MAX = 5 };
/* The limits of a sound are a choice of this API. The game plays volumes from 0.5 to 1 and pitches from 0.5 to 1. */
#define SV_SOUND_VOLUME_MAX 2.0f
#define SV_SOUND_PITCH_MIN 0.1f
#define SV_SOUND_PITCH_MAX 4.0f
/* A teleport accepts finite coordinates inside this limit. */
#define SV_POSITION_LIMIT 32768.0f
/* The longest velocity of a bullet event: the fastest bullet of the game flies 15 units for each tick. */
#define SV_BULLET_SPEED_MAX 64.0f

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
    /* Sends a text of 1 to 63 printable ASCII bytes to every client. */
    SvResult (SV_CALL *message)(SvOwner owner, uint32_t kind, const char *text);
    /* Adds the address of the player to the kick list and ends the connection. Bots have no connection. */
    SvResult (SV_CALL *kick)(SvOwner owner, SvEntity player);
    /* Adds money to an account and to each active player of it. The sum must fit in 32 bits. */
    SvResult (SV_CALL *add_money)(SvOwner owner, SvEntity account, int32_t amount);
    /* Moves a human on foot, a vehicle or a free item so that its place reaches the position: bone 0 of a human, the
     * center of a vehicle, the position of an item. The offset is the same for every part, and the speeds do not change.
     * SV_STATE for a human in a vehicle or an item that a human or another item holds. SV_DENIED for a piece of the train. */
    SvResult (SV_CALL *teleport)(SvOwner owner, SvEntity entity, SvVector position);
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
    /* Makes a bot player on foot in team 6 like the bots of the game, with a human at the position turned by the yaw angle.
     * The name is 0 to 31 printable ASCII bytes, or NULL for "NPC". The client shows the name above the body, and the very
     * first update event of the bot already carries it. The bot has no route and no end time. Both references go out only on
     * success. SV_LIMIT means that no player slot, human slot or bond is free, and then nothing is left behind. */
    SvResult (SV_CALL *npc_create)(SvOwner owner, SvVector position, float yaw, const char *name, SvEntity *player,
        SvEntity *human);
    /* Asks the game to remove a bot. Its bot expiry removes the human and the player in the next world logic pass.
     * SV_DENIED for a player who is not a bot. */
    SvResult (SV_CALL *npc_remove)(SvOwner owner, SvEntity player);
    /* Takes a human out of its vehicle as the exit button does, and moves a bone that the exit would put in a wall to the
     * near side of it. With stop 1 the speed of every bone becomes 0, so the body does not fly out at the speed of the
     * vehicle. SV_STATE when the human sits in no vehicle. stop must be 0 or 1. */
    SvResult (SV_CALL *vehicle_exit)(SvOwner owner, SvEntity human, int32_t stop);
    /* Plays a sound of the game at the position for the clients. The index must be below the size of the sound list of the
     * game (47). The volume is from 0 to SV_SOUND_VOLUME_MAX and the pitch from SV_SOUND_PITCH_MIN to SV_SOUND_PITCH_MAX. */
    SvResult (SV_CALL *sound)(SvOwner owner, uint32_t index, SvVector position, float volume, float pitch);
    /* Replaces news line "index", or adds a line when the index equals SvWorldState.headline_count, and sends it to the
     * clients. The text has 0 to SV_HEADLINE_TEXT_MAX printable ASCII characters. The game splits it at character 63 into
     * the two lines that the newspaper shows. SV_INVALID for a gap in the index or a bad text, SV_LIMIT when 1,024 lines
     * exist. The next round loads the news again, so a plugin that wants its line every round writes it again. */
    SvResult (SV_CALL *headline)(SvOwner owner, uint32_t index, const char *text);
    /* Seats a human on foot in a free seat of a vehicle (seat 0 is the driver), as the use button does for the nearest seat.
     * The seat must be below the seat count of the vehicle. SV_DENIED for a piece of the train, SV_INVALID for a bad seat,
     * SV_STATE for a human that is dead or already sits in a vehicle, SV_CONFLICT when the seat is taken. */
    SvResult (SV_CALL *vehicle_enter)(SvOwner owner, SvEntity human, SvEntity vehicle, int32_t seat);
    /* Gives a player, a bot included, a new name of 0 to 31 printable ASCII bytes, and sends the update to the clients. The
     * name of the account stays. Players who joined before may keep the old name on screen if their client reads it once. */
    SvResult (SV_CALL *set_name)(SvOwner owner, SvEntity player, const char *name);
    /* Makes a human say a text of 1 to 63 printable ASCII bytes, as the chat of a player does: the clients near the human
     * see it above the head. The range is 0 (short), 1 (normal) or 2 (long). SV_STATE for a human that is dead. */
    SvResult (SV_CALL *speak)(SvOwner owner, SvEntity human, const char *text, int32_t range);
    /* Tells the clients about a shot that the server does not simulate: a bullet of type 0 to 3 that starts at the position
     * with the velocity (units for each tick, at most SV_BULLET_SPEED_MAX long) and belongs to the weapon item. The clients
     * draw the tracer. No bullet exists in the server, so nothing is hit and no damage is made. The plugin makes the damage
     * itself (a line query and a health change), or it fires a real weapon. */
    SvResult (SV_CALL *bullet)(SvOwner owner, uint32_t type, SvVector position, SvVector velocity, SvEntity weapon);
    /* Tells the clients about the effect of a hit at the position: kind 0 a block of the level, 1 a human, 2 a vehicle and
     * 3 a drip of blood. The normal points out of the surface. It makes no damage. */
    SvResult (SV_CALL *bullet_hit)(SvOwner owner, uint32_t kind, SvVector position, SvVector normal);
} SvGameWrite;
#pragma pack(pop)

#endif
