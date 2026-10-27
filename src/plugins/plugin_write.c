/* The game write API. Each call checks its input before it changes the game.
 *
 * A plugin runs in a callback that the host starts between ticks, so a write never meets the
 * simulation. Every operation here leaves the records it touches consistent. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "sv_game_write.h"
#include "plugin_game_internal.h"
#include "events.h"
#include "server.h"
#include "sr_runtime.h"
#include "sr_server.h"
#include <string.h>

enum { MESSAGE_MAX = 63, KICK_IDLE_TICKS = 0x4b0 };

static SvResult resolve(SvOwner owner, uint32_t kind, SvEntity entity, int32_t *index)
{
    if (!sv_plugins_context(owner) || !sv_game_is_ready())
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
    if (!sv_plugins_context(owner) || !sv_game_is_ready())
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

static SvResult SV_CALL write_godmode(SvOwner owner, SvEntity player, int32_t enabled)
{
    int32_t index;
    SvResult result = resolve(owner, SV_ENTITY_PLAYER, player, &index);
    if (result != SV_OK)
        return result;
    if (enabled != 0 && enabled != 1)
        return SV_INVALID;
    sr_players[index].godmode = enabled;
    return SV_OK;
}

static SvResult SV_CALL write_health(SvOwner owner, SvEntity human, int32_t health)
{
    int32_t index;
    SvResult result = resolve(owner, SV_ENTITY_HUMAN, human, &index);
    if (result != SV_OK)
        return result;
    if (health < SV_HEALTH_MIN || health > SV_HEALTH_MAX)
        return SV_INVALID;
    sr_humans[index].health = health;
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

static SvResult SV_CALL write_admin(SvOwner owner, SvEntity player, int32_t enabled)
{
    int32_t index;
    int i;
    SvResult result = resolve(owner, SV_ENTITY_PLAYER, player, &index);
    if (result != SV_OK)
        return result;
    if ((enabled != 0 && enabled != 1) || sr_players[index].is_bot)
        return SV_INVALID;
    sr_players[index].is_admin = enabled;
    for (i = 0; i < sr_numofconnections; ++i)
        if (sr_connections[i].player_id == index)
            sr_connections[i].is_admin = enabled;
    return SV_OK;
}

static SvResult SV_CALL write_team(SvOwner owner, SvEntity player, int32_t team)
{
    int32_t index;
    SvResult result = resolve(owner, SV_ENTITY_PLAYER, player, &index);
    if (result != SV_OK)
        return result;
    if (team < 0 || team > SV_TEAM_MAX)
        return SV_INVALID;
    sr_players[index].team = team;
    if (sr_players[index].human_id >= 0)
        sr_humans[sr_players[index].human_id].team = team;
    createevent_updateplayer(index);
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

const SvGameWrite sv_game_write = {
    sizeof(SvGameWrite), SV_GAME_WRITE_VERSION, write_message, write_kick, write_add_money,
    write_godmode, write_health, write_teleport, write_admin, write_team, write_looks, write_message_to
};
