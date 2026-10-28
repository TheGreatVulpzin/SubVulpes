/* The things that plugins made. One table holds the owner, the kind and the reference of each. */
#include "plugin_made.h"
#include "plugin_entities.h"
#include "sv_game_write.h"
#include "items.h"
#include "objects.h"
#include "player.h"
#include "sr_runtime.h"
#include <string.h>

typedef struct {
    SvOwner owner;
    uint32_t kind;
    SvEntity entity;
} Made;

static Made made[SV_MADE_MAX];
static uint32_t made_count;

void sv_made_clear(void)
{
    memset(made, 0, sizeof made);
    made_count = 0;
}

uint32_t sv_made_count(void)
{
    return made_count;
}

/* Removes the entries of things that are gone. A reference that does not resolve any more names no thing. */
static void prune(void)
{
    uint32_t i, kept = 0;
    for (i = 0; i < made_count; ++i)
        if (sv_entity_resolve(made[i].kind, made[i].entity) >= 0)
            made[kept++] = made[i];
    memset(&made[kept], 0, (made_count - kept) * sizeof made[0]);
    made_count = kept;
}

int sv_made_room(void)
{
    if (made_count < SV_MADE_MAX)
        return 1;
    prune();
    return made_count < SV_MADE_MAX;
}

void sv_made_add(SvOwner owner, uint32_t kind, SvEntity entity)
{
    if (made_count >= SV_MADE_MAX)
        return;
    made[made_count].owner = owner;
    made[made_count].kind = kind;
    made[made_count].entity = entity;
    ++made_count;
}

static void remove_at(uint32_t index)
{
    memmove(&made[index], &made[index + 1], (made_count - index - 1) * sizeof made[0]);
    memset(&made[--made_count], 0, sizeof made[0]);
}

SvResult sv_made_disown(SvOwner owner, SvEntity entity)
{
    uint32_t i;
    for (i = 0; i < made_count; ++i) {
        if (made[i].owner != owner || made[i].entity != entity)
            continue;
        if (sv_entity_resolve(made[i].kind, entity) < 0)
            break;
        remove_at(i);
        return SV_OK;
    }
    return SV_NOT_FOUND;
}

/* The same steps as item_remove, object_remove and npc_remove of the write API. */
static void end_one(const Made *entry)
{
    int32_t index = sv_entity_resolve(entry->kind, entry->entity);
    if (index < 0)
        return;
    if (entry->kind == SV_ENTITY_ITEM)
        sr_items[index].time_to_live = 0;
    else if (entry->kind == SV_ENTITY_OBJECT && sr_objects[index].type < SV_OBJECT_TYPE_COUNT)
        sr_objects[index].time_to_live = 0;
    else if (entry->kind == SV_ENTITY_PLAYER && sr_players[index].is_bot)
        sr_players[index].despawn_time = -1;
}

void sv_made_release(SvOwner owner)
{
    uint32_t i = 0;
    while (i < made_count) {
        if (made[i].owner != owner) {
            ++i;
            continue;
        }
        end_one(&made[i]);
        remove_at(i);
    }
}
