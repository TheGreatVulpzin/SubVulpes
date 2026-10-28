#include "plugin_entities.h"
#include "plugin_handles.h"
#include "sv_game_storage.h"
#include "sr_runtime.h"

static SvHandlePool pools[SV_ENTITY_ACCOUNT + 1];
static int initialized;

uint32_t sv_entity_capacity(uint32_t kind)
{
    switch (kind) {
    case SV_ENTITY_PLAYER: return SR_MAX_PLAYER;
    case SV_ENTITY_HUMAN: return SR_MAX_HUMAN;
    case SV_ENTITY_ITEM: return SR_MAX_ITEM;
    case SV_ENTITY_OBJECT: return SR_MAX_OBJECT;
    case SV_ENTITY_ACCOUNT: return SR_MAX_ACCOUNT;
    default: return 0;
    }
}

static int active(uint32_t kind, uint32_t index)
{
    if (index >= sv_entity_capacity(kind))
        return 0;
    switch (kind) {
    case SV_ENTITY_PLAYER: return sr_players[index].active != 0;
    case SV_ENTITY_HUMAN: return sr_humans[index].active != 0;
    case SV_ENTITY_ITEM: return sr_items[index].active != 0;
    case SV_ENTITY_OBJECT: return sr_objects[index].active != 0;
    case SV_ENTITY_ACCOUNT: return sr_data_accounts.count > 0 && index < (uint32_t)sr_data_accounts.count;
    default: return 0;
    }
}

int sv_entities_start(void)
{
    uint32_t kind;
    if (initialized)
        return 1;
    for (kind = SV_ENTITY_PLAYER; kind <= SV_ENTITY_ACCOUNT; ++kind) {
        if (!sv_handle_pool_init(&pools[kind], (uint8_t)kind, sv_entity_capacity(kind))) {
            while (kind > SV_ENTITY_PLAYER)
                sv_handle_pool_destroy(&pools[--kind]);
            return 0;
        }
    }
    initialized = 1;
    return 1;
}

void sv_entities_clear(uint32_t kind)
{
    if (initialized && sv_entity_capacity(kind)) {
        /* The values of the plugins for these things go before their handles do. */
        sv_storage_clear(kind);
        sv_handle_pool_clear(&pools[kind]);
    }
}

void sv_entities_stop(void)
{
    uint32_t kind;
    /* Keep generation history if the host starts again in this process. */
    for (kind = SV_ENTITY_PLAYER; kind <= SV_ENTITY_ACCOUNT; ++kind)
        sv_entities_clear(kind);
}

void sv_entity_created(uint32_t kind, uint32_t index)
{
    if (initialized && active(kind, index)) {
        sv_handle_pool_erase(&pools[kind], index);
        (void)sv_handle_pool_bind(&pools[kind], index);
    }
}

void sv_entity_deleted(uint32_t kind, uint32_t index)
{
    if (initialized && sv_entity_capacity(kind)) {
        /* A player, a human, an item or a vehicle takes its values of the plugin storage with it. */
        if (kind != SV_ENTITY_ACCOUNT)
            sv_storage_leave(sv_handle_pool_at(&pools[kind], index));
        sv_handle_pool_erase(&pools[kind], index);
    }
}

SvEntity sv_entity_at(uint32_t kind, int32_t index)
{
    if (!initialized || !active(kind, (uint32_t)index))
        return 0;
    return sv_handle_pool_at(&pools[kind], (uint32_t)index);
}

int32_t sv_entity_resolve(uint32_t kind, SvEntity entity)
{
    int32_t index;
    if (!initialized || !sv_entity_capacity(kind))
        return -1;
    index = sv_handle_pool_resolve(&pools[kind], entity);
    return index >= 0 && active(kind, (uint32_t)index) ? index : -1;
}
