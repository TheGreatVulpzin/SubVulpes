/* The game write API. Each call checks its input before it changes the game.
 *
 * A plugin runs in a callback that the host starts between ticks, so a write never meets the
 * simulation. Every operation here leaves the records it touches consistent. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "sv_game_write.h"
#include "plugin_game_internal.h"
#include "events.h"
#include "buildings.h"
#include "items.h"
#include "objects.h"
#include "player.h"
#include "humans.h"
#include "server.h"
#include "sr_runtime.h"
#include "sr_server.h"
#include "vectors.h"
#include <string.h>

enum { MESSAGE_MAX = 63, KICK_IDLE_TICKS = 0x4b0 };

static SvResult resolve(SvOwner owner, uint32_t kind, SvEntity entity, int32_t *index)
{
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    *index = sv_entity_resolve(kind, entity);
    return *index < 0 ? SV_NOT_FOUND : SV_OK;
}

static int finite(float value)
{
    return value == value && value - value == 0.0f;
}

/* Copies a valid text of 1 to 63 printable ASCII bytes. */
static SvResult copy_text(uint32_t kind, const char *text, char buffer[MESSAGE_MAX + 1])
{
    size_t i, length;
    if ((kind != SV_MESSAGE_SERVER && kind != SV_MESSAGE_ADMIN) || !text)
        return SV_INVALID;
    length = strnlen(text, MESSAGE_MAX + 1);
    if (!length || length > MESSAGE_MAX)
        return SV_INVALID;
    for (i = 0; i < length; ++i)
        if ((unsigned char)text[i] < 0x20 || (unsigned char)text[i] > 0x7e)
            return SV_INVALID;
    memcpy(buffer, text, length);
    buffer[length] = 0;
    return SV_OK;
}

static SvResult SV_CALL write_message(SvOwner owner, uint32_t kind, const char *text)
{
    char buffer[MESSAGE_MAX + 1];
    SvResult result;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    result = copy_text(kind, text, buffer);
    if (result != SV_OK)
        return result;
    sr_createevent_message((int)kind, buffer, -1, 0);
    return SV_OK;
}

static SvResult SV_CALL write_message_to(SvOwner owner, uint32_t kind, SvEntity player, const char *text)
{
    char buffer[MESSAGE_MAX + 1];
    int32_t index;
    SvResult result = resolve(owner, SV_ENTITY_PLAYER, player, &index);
    if (result != SV_OK)
        return result;
    result = copy_text(kind, text, buffer);
    if (result != SV_OK)
        return result;
    createevent_message_to((int)kind, buffer, index);
    return SV_OK;
}

static SvResult SV_CALL write_kick(SvOwner owner, SvEntity player)
{
    int32_t index;
    int found = -1, i;
    SvResult result = resolve(owner, SV_ENTITY_PLAYER, player, &index);
    if (result != SV_OK)
        return result;
    for (i = 0; i < sr_numofconnections; ++i)
        if (sr_connections[i].player_id == index)
            found = i;
    if (found < 0)
        return SV_NOT_FOUND;
    if (sr_numofkicked >= SR_MAX_KICKEDIP)
        return SV_LIMIT;
    sr_kickedip[sr_numofkicked++] = sr_connections[found].ip;
    sr_connections[found].idle_ticks = KICK_IDLE_TICKS;
    return SV_OK;
}

static int fits(int current, int32_t amount)
{
    int64_t sum = (int64_t)current + amount;
    return sum >= INT32_MIN && sum <= INT32_MAX;
}

static SvResult SV_CALL write_add_money(SvOwner owner, SvEntity account, int32_t amount)
{
    int32_t index;
    int i;
    SvResult result = resolve(owner, SV_ENTITY_ACCOUNT, account, &index);
    if (result != SV_OK)
        return result;
    if (!fits(sr_accounts[index].money, amount))
        return SV_INVALID;
    for (i = 0; i < SR_MAX_PLAYER; ++i)
        if (sr_players[i].active && sr_players[i].account == index && !fits(sr_players[i].money, amount))
            return SV_INVALID;
    sr_accounts[index].money += amount;
    for (i = 0; i < SR_MAX_PLAYER; ++i)
        if (sr_players[i].active && sr_players[i].account == index)
            sr_players[i].money += amount;
    return SV_OK;
}

static SvResult SV_CALL write_teleport(SvOwner owner, SvEntity human, SvVector position)
{
    int32_t index;
    Human *hu;
    vec3 delta;
    int k;
    SvResult result = resolve(owner, SV_ENTITY_HUMAN, human, &index);
    if (result != SV_OK)
        return result;
    if (!finite(position.x) || !finite(position.y) || !finite(position.z) ||
        position.x > SV_POSITION_LIMIT || position.x < -SV_POSITION_LIMIT ||
        position.y > SV_POSITION_LIMIT || position.y < -SV_POSITION_LIMIT ||
        position.z > SV_POSITION_LIMIT || position.z < -SV_POSITION_LIMIT)
        return SV_INVALID;
    hu = &sr_humans[index];
    if (hu->vehicle_id != -1)
        return SV_STATE;
    delta.x = position.x - hu->bones[0].pos.x;
    delta.y = position.y - hu->bones[0].pos.y;
    delta.z = position.z - hu->bones[0].pos.z;
    for (k = 0; k < 15; ++k) {
        hu->bones[k].pos.x += delta.x;
        hu->bones[k].pos.y += delta.y;
        hu->bones[k].pos.z += delta.z;
        hu->bones[k].pos_prev = hu->bones[k].pos;
    }
    hu->pos = hu->bones[0].pos;
    return SV_OK;
}

static SvResult SV_CALL write_looks(SvOwner owner, SvEntity player, const int32_t *looks)
{
    static const int32_t highest[SV_LOOK_COUNT] = { 2, 7, 3, 1, 1 };
    int32_t index;
    int i, human;
    SvResult result = resolve(owner, SV_ENTITY_PLAYER, player, &index);
    if (result != SV_OK)
        return result;
    if (!looks)
        return SV_INVALID;
    for (i = 0; i < SV_LOOK_COUNT; ++i)
        if (looks[i] < 0 || looks[i] > highest[i])
            return SV_INVALID;
    sr_players[index].look_a = looks[0];
    sr_players[index].look_b = looks[1];
    sr_players[index].look_c = looks[2];
    sr_players[index].look_d = looks[3];
    sr_players[index].look_e = looks[4];
    human = sr_players[index].human_id;
    if (human >= 0) {
        sr_humans[human].look_a = looks[0];
        sr_humans[human].look_b = looks[1];
        sr_humans[human].look_c = looks[2];
        sr_humans[human].look_d = looks[3];
        sr_humans[human].look_e = looks[4];
    }
    createevent_updateplayer(index);
    if (human >= 0)
        createevent_updatehuman(human, index);
    return SV_OK;
}

static int position_ok(SvVector position)
{
    return finite(position.x) && finite(position.y) && finite(position.z) &&
        position.x <= SV_POSITION_LIMIT && position.x >= -SV_POSITION_LIMIT &&
        position.y <= SV_POSITION_LIMIT && position.y >= -SV_POSITION_LIMIT &&
        position.z <= SV_POSITION_LIMIT && position.z >= -SV_POSITION_LIMIT;
}

static SvResult SV_CALL write_item_create(SvOwner owner, uint32_t type, SvVector position, SvEntity *item)
{
    static const vec3 rotation[3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
    vec3 at;
    uint32_t index;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    if (type >= SV_ITEM_TYPE_COUNT || type == ITEM_NEWSPAPER || !item || !position_ok(position))
        return SV_INVALID;
    at.x = position.x;
    at.y = position.y;
    at.z = position.z;
    index = createitem((int)type, &at, NULL, rotation);
    if (index >= SR_MAX_ITEM)
        return SV_LIMIT;
    *item = sv_entity_at(SV_ENTITY_ITEM, (int32_t)index);
    return SV_OK;
}

static SvResult SV_CALL write_item_remove(SvOwner owner, SvEntity item)
{
    int32_t index;
    SvResult result = resolve(owner, SV_ENTITY_ITEM, item, &index);
    if (result != SV_OK)
        return result;
    sr_items[index].time_to_live = 0;
    return SV_OK;
}

static SvResult SV_CALL write_item_equip(SvOwner owner, SvEntity item, SvEntity human, uint32_t slot)
{
    int32_t item_index, human_index;
    SvResult result = resolve(owner, SV_ENTITY_ITEM, item, &item_index);
    if (result != SV_OK)
        return result;
    result = resolve(owner, SV_ENTITY_HUMAN, human, &human_index);
    if (result != SV_OK)
        return result;
    if (slot >= SV_ITEM_SLOT_COUNT)
        return SV_INVALID;
    return linkitem((uint32_t)item_index, UINT32_MAX, human_index, (int)slot) ? SV_OK : SV_CONFLICT;
}

/* 1 when "ancestor" is the item or one of the parents of the item. */
static int in_chain(int32_t item, int32_t ancestor)
{
    int depth;
    for (depth = 0; item != -1; ++depth) {
        if (item == ancestor || depth >= SR_MAX_ITEM)
            return 1;
        item = sr_items[item].parent_item;
    }
    return 0;
}

static SvResult SV_CALL write_item_attach(SvOwner owner, SvEntity parent, SvEntity child)
{
    int32_t parent_index, child_index;
    SvResult result = resolve(owner, SV_ENTITY_ITEM, parent, &parent_index);
    if (result != SV_OK)
        return result;
    result = resolve(owner, SV_ENTITY_ITEM, child, &child_index);
    if (result != SV_OK)
        return result;
    if (parent_index == child_index)
        return SV_INVALID;
    /* linkitem does not remove the child from an old parent item, so refuse that case. */
    if (sr_items[child_index].parent_item != -1 || in_chain(parent_index, child_index))
        return SV_CONFLICT;
    return linkitem((uint32_t)parent_index, (uint32_t)child_index, -1, 0) ? SV_OK : SV_CONFLICT;
}

static SvResult SV_CALL write_item_detach(SvOwner owner, SvEntity item)
{
    int32_t index;
    SvResult result = resolve(owner, SV_ENTITY_ITEM, item, &index);
    if (result != SV_OK)
        return result;
    return linkitem((uint32_t)index, UINT32_MAX, -1, 0) ? SV_OK : SV_STATE;
}

static SvResult SV_CALL write_object_create(SvOwner owner, uint32_t type, SvVector position, float yaw, int32_t color,
    SvEntity *object)
{
    vec3 at, rotation[3];
    int index;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    if (type >= SV_OBJECT_TYPE_COUNT || color < 0 || color > SV_OBJECT_COLOR_MAX || !object || !position_ok(position) ||
        !finite(yaw))
        return SV_INVALID;
    at.x = position.x;
    at.y = position.y;
    at.z = position.z;
    resetorientation(rotation);
    rotateorientation(rotation, &rotation[1], yaw);
    index = createobject((int)type, &at, NULL, rotation, color);
    if (index < 0)
        return SV_LIMIT;
    *object = sv_entity_at(SV_ENTITY_OBJECT, index);
    return SV_OK;
}

static SvResult SV_CALL write_object_remove(SvOwner owner, SvEntity object)
{
    int32_t index;
    SvResult result = resolve(owner, SV_ENTITY_OBJECT, object, &index);
    if (result != SV_OK)
        return result;
    if (sr_objects[index].type >= SV_OBJECT_TYPE_COUNT)
        return SV_DENIED;
    sr_objects[index].time_to_live = 0;
    return SV_OK;
}

static SvResult SV_CALL write_npc_create(SvOwner owner, SvVector position, float yaw, SvEntity *player, SvEntity *human)
{
    vec3 at, rotation[3];
    int p, h;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    if (!player || !human || !position_ok(position) || !finite(yaw))
        return SV_INVALID;
    at.x = position.x;
    at.y = position.y;
    at.z = position.z;
    p = createplayer();
    if (p < 0)
        return SV_LIMIT;
    strcpy(sr_players[p].name, "NPC");
    sr_players[p].team = 6;
    sr_players[p].is_bot = 1;
    createevent_updateplayer(p);
    resetorientation(rotation);
    rotateorientation(rotation, &rotation[1], yaw);
    h = createhuman(&at.x, &rotation[0].x, p);
    if (h < 0) {
        deleteplayer(p);
        return SV_LIMIT;
    }
    /* The game sets the position of a human at its first tick. Set it now so that a read sees the place. */
    sr_humans[h].pos = sr_humans[h].bones[0].pos;
    sr_players[p].yaw = yaw;
    sr_players[p].human_id = h;
    createevent_updateplayer(p);
    *player = sv_entity_at(SV_ENTITY_PLAYER, p);
    *human = sv_entity_at(SV_ENTITY_HUMAN, h);
    return SV_OK;
}

static SvResult SV_CALL write_npc_remove(SvOwner owner, SvEntity player)
{
    int32_t index;
    SvResult result = resolve(owner, SV_ENTITY_PLAYER, player, &index);
    if (result != SV_OK)
        return result;
    if (!sr_players[index].is_bot)
        return SV_DENIED;
    /* A nonzero time in the past ends the bot in the next world logic pass. */
    sr_players[index].despawn_time = -1;
    return SV_OK;
}

static SvResult SV_CALL write_vehicle_exit(SvOwner owner, SvEntity human, int32_t stop)
{
    int32_t index;
    Human *hu;
    int k;
    SvResult result = resolve(owner, SV_ENTITY_HUMAN, human, &index);
    if (result != SV_OK)
        return result;
    if (stop != 0 && stop != 1)
        return SV_INVALID;
    hu = &sr_humans[index];
    if (hu->vehicle_id < 0 || hu->vehicle_id >= SR_MAX_OBJECT || !sr_objects[hu->vehicle_id].active)
        return SV_STATE;
    human_leavevehicle(index);
    if (stop)
        for (k = 0; k < 15; ++k)
            hu->bones[k].vel.x = hu->bones[k].vel.y = hu->bones[k].vel.z = 0.0f;
    return SV_OK;
}

static SvResult SV_CALL write_sound(SvOwner owner, uint32_t index, SvVector position, float volume, float pitch)
{
    vec3 at;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    if (index >= (uint32_t)sr_data_soundlist.num_sounds || !position_ok(position) || !finite(volume) || !finite(pitch) ||
        volume < 0.0f || volume > SV_SOUND_VOLUME_MAX || pitch < SV_SOUND_PITCH_MIN || pitch > SV_SOUND_PITCH_MAX)
        return SV_INVALID;
    at.x = position.x;
    at.y = position.y;
    at.z = position.z;
    createevent_sound((int)index, &at, volume, pitch);
    return SV_OK;
}

static SvResult SV_CALL write_headline(SvOwner owner, uint32_t index, const char *text)
{
    size_t i, length;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    if (!text || index > SV_HEADLINE_COUNT_MAX)
        return SV_INVALID;
    length = strnlen(text, SV_HEADLINE_TEXT_MAX + 1);
    for (i = 0; i < length; ++i)
        if ((unsigned char)text[i] < 0x20 || (unsigned char)text[i] > 0x7e)
            return SV_INVALID;
    switch (newspaper_set_line((int)index, text)) {
    case NEWS_OK:
        return SV_OK;
    case NEWS_FULL:
        return SV_LIMIT;
    default:
        return SV_INVALID;
    }
}

const SvGameWrite sv_game_write = {
    sizeof(SvGameWrite), SV_GAME_WRITE_VERSION, write_message, write_kick, write_add_money,
    write_teleport, write_looks, write_message_to,
    write_item_create, write_item_remove, write_item_equip, write_item_attach, write_item_detach,
    write_object_create, write_object_remove, write_npc_create, write_npc_remove,
    write_vehicle_exit, write_sound, write_headline
};
