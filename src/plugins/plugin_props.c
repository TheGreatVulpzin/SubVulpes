/* The property table of the plugin API. See sv_game_props.h.
 *
 * Each row names one number of the game. The calls below find the record, check the operation, the slot and the range,
 * and then write the field. A row can add a check before the write and a step after it, for the numbers that other
 * records or the clients must follow. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "plugin_props.h"
#include "plugin_game_internal.h"
#include "events.h"
#include "objects.h"
#include "player.h"
#include "humans.h"
#include "server.h"
#include "sr_runtime.h"
#include "sr_server.h"
#include <string.h>

/* The longest change that one add gives to a velocity, in units for each tick. */
#define PUSH_MAX 64.0

#define FIELD(function, expression) \
    static void *function(int32_t i, uint32_t s) { (void)s; return &(expression); }

FIELD(player_godmode, sr_players[i].godmode)
FIELD(player_admin, sr_players[i].is_admin)
FIELD(player_team, sr_players[i].team)
FIELD(human_health, sr_humans[i].health)
FIELD(human_bleeding, sr_humans[i].bleeding)
FIELD(human_stun, sr_humans[i].stun_timer)
FIELD(human_protect, sr_humans[i].protect_timer)
FIELD(human_bone_velocity, sr_humans[i].bones[s].vel)
FIELD(object_velocity, sr_objects[i].vel)

/* A bot cannot be an admin. */
static SvResult admin_check(int32_t index, SvValue value)
{
    (void)value;
    return sr_players[index].is_bot ? SV_INVALID : SV_OK;
}

/* The connections of the player follow the flag, as the password login does. */
static void admin_after(int32_t index)
{
    int i;
    for (i = 0; i < sr_numofconnections; ++i)
        if (sr_connections[i].player_id == index)
            sr_connections[i].is_admin = sr_players[index].is_admin;
}

/* The human of the player follows the team, and the clients get the player update. */
static void team_after(int32_t index)
{
    if (sr_players[index].human_id >= 0)
        sr_humans[sr_players[index].human_id].team = sr_players[index].team;
    createevent_updateplayer(index);
}

static SvResult bone_add(int32_t index, uint32_t slot, SvVector change)
{
    vec3 *vel = &sr_humans[index].bones[slot].vel;
    vel->x += change.x;
    vel->y += change.y;
    vel->z += change.z;
    return SV_OK;
}

/* A push on an object moves every particle of it. A piece of the train belongs to the track system. */
static SvResult object_add(int32_t index, uint32_t slot, SvVector change)
{
    const Object *ob = &sr_objects[index];
    int i;
    (void)slot;
    if (ob->type >= 12)
        return SV_DENIED;
    if (ob->num_particles < 0 || ob->num_particles > 64)
        return SV_FAILED;
    for (i = 0; i < ob->num_particles; ++i)
        if (ob->particles[i] < 0 || ob->particles[i] >= SR_MAX_PARTICLE)
            return SV_FAILED;
    for (i = 0; i < ob->num_particles; ++i) {
        vec3 *vel = &sr_particles[ob->particles[i]].vel;
        vel->x += change.x;
        vel->y += change.y;
        vel->z += change.z;
    }
    return SV_OK;
}

static const Prop props[] = {
    { "player.godmode", SV_ENTITY_PLAYER, SV_PROP_INT, 1, SV_PROP_GET | SV_PROP_SET, 0, 1, player_godmode, NULL, NULL, NULL },
    { "player.admin", SV_ENTITY_PLAYER, SV_PROP_INT, 1, SV_PROP_GET | SV_PROP_SET, 0, 1, player_admin, admin_check, admin_after, NULL },
    { "player.team", SV_ENTITY_PLAYER, SV_PROP_INT, 1, SV_PROP_GET | SV_PROP_SET, 0, 7, player_team, NULL, team_after, NULL },
    { "human.health", SV_ENTITY_HUMAN, SV_PROP_INT, 1, SV_PROP_GET | SV_PROP_SET | SV_PROP_ADD, -100, 100, human_health, NULL, NULL, NULL },
    { "human.bleeding", SV_ENTITY_HUMAN, SV_PROP_INT, 1, SV_PROP_GET | SV_PROP_SET, 0, 1, human_bleeding, NULL, NULL, NULL },
    { "human.stun", SV_ENTITY_HUMAN, SV_PROP_INT, 1, SV_PROP_GET | SV_PROP_SET | SV_PROP_ADD, 0, 3600, human_stun, NULL, NULL, NULL },
    { "human.protect", SV_ENTITY_HUMAN, SV_PROP_INT, 1, SV_PROP_GET | SV_PROP_SET | SV_PROP_ADD, 0, 21600, human_protect, NULL, NULL, NULL },
    { "human.bone_velocity", SV_ENTITY_HUMAN, SV_PROP_VECTOR, 15, SV_PROP_GET | SV_PROP_ADD, 0, PUSH_MAX, human_bone_velocity, NULL, NULL, bone_add },
    { "object.velocity", SV_ENTITY_OBJECT, SV_PROP_VECTOR, 1, SV_PROP_GET | SV_PROP_ADD, 0, PUSH_MAX, object_velocity, NULL, NULL, object_add },
};

enum { GROUP_MAX = 8, ROW_MAX = 256 };

static struct {
    const Prop *rows;
    size_t count;
} groups[GROUP_MAX] = { { props, sizeof props / sizeof props[0] } };
static size_t group_count = 1;

static size_t row_count(void)
{
    size_t i, total = 0;
    for (i = 0; i < group_count; ++i)
        total += groups[i].count;
    return total;
}

/* The row with this id. Ids start at 1 and follow the order of the groups. */
static const Prop *row_at(uint32_t id)
{
    size_t i, index = id - 1;
    if (!id)
        return NULL;
    for (i = 0; i < group_count; ++i) {
        if (index < groups[i].count)
            return &groups[i].rows[index];
        index -= groups[i].count;
    }
    return NULL;
}

static int name_used(const char *name)
{
    size_t i, k;
    for (i = 0; i < group_count; ++i)
        for (k = 0; k < groups[i].count; ++k)
            if (!strcmp(groups[i].rows[k].name, name))
                return 1;
    return 0;
}

SvResult sv_props_add_group(const Prop *rows, size_t count)
{
    size_t i, k;
    if (!rows || !count)
        return SV_INVALID;
    for (i = 0; i < group_count; ++i)
        if (groups[i].rows == rows)
            return SV_OK;
    if (group_count >= GROUP_MAX || row_count() + count > ROW_MAX)
        return SV_LIMIT;
    for (i = 0; i < count; ++i) {
        const Prop *p = &rows[i];
        if (!p->name || !p->name[0] || strlen(p->name) >= SV_PROP_NAME_MAX || !p->field || !p->count ||
            (p->type != SV_PROP_INT && p->type != SV_PROP_FLOAT && p->type != SV_PROP_VECTOR) ||
            !(p->ops & (SV_PROP_GET | SV_PROP_SET | SV_PROP_ADD)) || (p->type == SV_PROP_VECTOR && (p->ops & SV_PROP_SET)) ||
            (p->type == SV_PROP_VECTOR && (p->ops & SV_PROP_ADD) && !p->add_vector) || name_used(p->name))
            return SV_INVALID;
        for (k = i + 1; k < count; ++k)
            if (!strcmp(p->name, rows[k].name))
                return SV_INVALID;
    }
    groups[group_count].rows = rows;
    groups[group_count].count = count;
    ++group_count;
    return SV_OK;
}

static int finite(float value)
{
    return value == value && value - value == 0.0f;
}

static SvResult context(SvOwner owner)
{
    return sv_plugins_context(owner) && sv_game_is_ready() ? SV_OK : SV_STATE;
}

static SvResult SV_CALL props_find(SvOwner owner, const char *name, uint32_t *id)
{
    size_t i, length, total = row_count();
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (!name || !id)
        return SV_INVALID;
    length = strnlen(name, SV_PROP_NAME_MAX);
    if (!length || length >= SV_PROP_NAME_MAX)
        return SV_INVALID;
    for (i = 1; i <= total; ++i)
        if (!strcmp(row_at((uint32_t)i)->name, name)) {
            *id = (uint32_t)i;
            return SV_OK;
        }
    return SV_NOT_FOUND;
}

static SvResult SV_CALL props_info(SvOwner owner, uint32_t id, SvPropInfo *info)
{
    const Prop *p;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (!info || info->size < sizeof *info)
        return SV_INVALID;
    p = row_at(id);
    if (!p)
        return SV_NOT_FOUND;
    memset(info, 0, sizeof *info);
    info->size = sizeof *info;
    info->id = id;
    info->entity_kind = p->kind;
    info->type = p->type;
    info->count = p->count;
    info->ops = p->ops;
    info->min = p->min;
    info->max = p->max;
    strcpy(info->name, p->name);
    return SV_OK;
}

/* Steps that every call shares: the context, the property, the record, the operation and the slot. */
static SvResult locate(SvOwner owner, uint32_t id, SvEntity entity, uint32_t slot, uint32_t op, const Prop **row,
    int32_t *record)
{
    const Prop *p;
    if (context(owner) != SV_OK)
        return SV_STATE;
    p = row_at(id);
    if (!p)
        return SV_NOT_FOUND;
    if (p->kind == SV_ENTITY_WORLD) {
        if (entity)
            return SV_INVALID;
        *record = 0;
    } else {
        *record = sv_entity_resolve(p->kind, entity);
        if (*record < 0)
            return SV_NOT_FOUND;
    }
    if (!(p->ops & op))
        return SV_DENIED;
    if (slot >= p->count)
        return SV_INVALID;
    *row = p;
    return SV_OK;
}

static int in_range(const Prop *p, SvValue value)
{
    if (p->type == SV_PROP_INT)
        return value.i >= p->min && value.i <= p->max;
    return finite(value.f) && value.f >= p->min && value.f <= p->max;
}

static SvResult SV_CALL props_get(SvOwner owner, uint32_t id, SvEntity entity, uint32_t slot, SvValue *value)
{
    const Prop *p;
    int32_t record;
    const void *field;
    SvResult result = locate(owner, id, entity, slot, SV_PROP_GET, &p, &record);
    if (result != SV_OK)
        return result;
    if (!value)
        return SV_INVALID;
    field = p->field(record, slot);
    memset(value, 0, sizeof *value);
    if (p->type == SV_PROP_INT)
        value->i = *(const int32_t *)field;
    else if (p->type == SV_PROP_FLOAT)
        value->f = *(const float *)field;
    else {
        const vec3 *v = field;
        value->v.x = v->x;
        value->v.y = v->y;
        value->v.z = v->z;
    }
    return SV_OK;
}

static SvResult SV_CALL props_set(SvOwner owner, uint32_t id, SvEntity entity, uint32_t slot, SvValue value)
{
    const Prop *p;
    int32_t record;
    void *field;
    SvResult result = locate(owner, id, entity, slot, SV_PROP_SET, &p, &record);
    if (result != SV_OK)
        return result;
    if (p->type == SV_PROP_VECTOR || !in_range(p, value))
        return SV_INVALID;
    if (p->check && (result = p->check(record, value)) != SV_OK)
        return result;
    field = p->field(record, slot);
    if (p->type == SV_PROP_INT)
        *(int32_t *)field = value.i;
    else
        *(float *)field = value.f;
    if (p->after)
        p->after(record);
    return SV_OK;
}

static SvResult SV_CALL props_add(SvOwner owner, uint32_t id, SvEntity entity, uint32_t slot, SvValue change)
{
    const Prop *p;
    int32_t record;
    void *field;
    SvValue sum;
    SvResult result = locate(owner, id, entity, slot, SV_PROP_ADD, &p, &record);
    if (result != SV_OK)
        return result;
    if (p->type == SV_PROP_VECTOR) {
        double length2;
        if (!finite(change.v.x) || !finite(change.v.y) || !finite(change.v.z))
            return SV_INVALID;
        length2 = (double)change.v.x * change.v.x + (double)change.v.y * change.v.y + (double)change.v.z * change.v.z;
        if (length2 > p->max * p->max)
            return SV_INVALID;
        return p->add_vector(record, slot, change.v);
    }
    field = p->field(record, slot);
    if (p->type == SV_PROP_INT) {
        int64_t total = (int64_t)*(const int32_t *)field + change.i;
        if (total < p->min || total > p->max)
            return SV_INVALID;
        sum.i = (int32_t)total;
    } else {
        if (!finite(change.f))
            return SV_INVALID;
        sum.f = *(const float *)field + change.f;
        if (!in_range(p, sum))
            return SV_INVALID;
    }
    if (p->check && (result = p->check(record, sum)) != SV_OK)
        return result;
    if (p->type == SV_PROP_INT)
        *(int32_t *)field = sum.i;
    else
        *(float *)field = sum.f;
    if (p->after)
        p->after(record);
    return SV_OK;
}

const SvGameProps sv_game_props = {
    sizeof(SvGameProps), SV_GAME_PROPS_VERSION, props_find, props_info, props_get, props_set, props_add
};
