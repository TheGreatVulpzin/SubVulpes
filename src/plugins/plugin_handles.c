#include "plugin_handles.h"
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define SV_HANDLE_SLOT_MASK UINT32_C(0x00ffffff)
#define SV_HANDLE_NO_SLOT UINT32_MAX
#define SV_HANDLE_FREE_INDEX (-1)
#define SV_HANDLE_RETIRED_INDEX (-2)

static int sv_handle_pool_ready(const SvHandlePool *pool)
{
    return pool && pool->kind && pool->capacity &&
        pool->capacity <= SV_HANDLE_SLOT_MASK &&
        pool->capacity <= INT32_MAX && pool->index_to_slot &&
        pool->slot_to_index && pool->generation && pool->free_next;
}

static uint64_t sv_handle_encode(const SvHandlePool *pool, uint32_t slot)
{
    return ((uint64_t)pool->kind << 56) |
        ((uint64_t)pool->generation[slot] << 24) | ((uint64_t)slot + 1);
}

int sv_handle_pool_init(SvHandlePool *pool, uint8_t kind, uint32_t capacity)
{
    uint32_t i;
    size_t bytes;
    if (!pool)
        return 0;
    memset(pool, 0, sizeof *pool);
    if (!kind || !capacity || capacity > SV_HANDLE_SLOT_MASK ||
        capacity > INT32_MAX || (size_t)capacity > SIZE_MAX / sizeof(uint32_t) ||
        (size_t)capacity > SIZE_MAX / sizeof(int32_t))
        return 0;
    bytes = (size_t)capacity * sizeof(uint32_t);
    pool->index_to_slot = malloc(bytes);
    pool->slot_to_index = malloc((size_t)capacity * sizeof(int32_t));
    pool->generation = malloc(bytes);
    pool->free_next = malloc(bytes);
    if (!pool->index_to_slot || !pool->slot_to_index ||
        !pool->generation || !pool->free_next) {
        sv_handle_pool_destroy(pool);
        return 0;
    }
    pool->capacity = capacity;
    pool->kind = kind;
    for (i = 0; i < capacity; ++i) {
        pool->index_to_slot[i] = SV_HANDLE_NO_SLOT;
        pool->slot_to_index[i] = SV_HANDLE_FREE_INDEX;
        pool->generation[i] = 1;
        pool->free_next[i] = i + 1 < capacity ? i + 1 : SV_HANDLE_NO_SLOT;
    }
    return 1;
}

void sv_handle_pool_destroy(SvHandlePool *pool)
{
    if (!pool)
        return;
    free(pool->index_to_slot);
    free(pool->slot_to_index);
    free(pool->generation);
    free(pool->free_next);
    memset(pool, 0, sizeof *pool);
}

void sv_handle_pool_clear(SvHandlePool *pool)
{
    uint32_t slot;
    if (!sv_handle_pool_ready(pool))
        return;
    pool->free_head = SV_HANDLE_NO_SLOT;
    for (slot = pool->capacity; slot-- > 0;) {
        pool->index_to_slot[slot] = SV_HANDLE_NO_SLOT;
        pool->free_next[slot] = SV_HANDLE_NO_SLOT;
        if (pool->slot_to_index[slot] >= 0) {
            if (pool->generation[slot] == UINT32_MAX) {
                pool->slot_to_index[slot] = SV_HANDLE_RETIRED_INDEX;
            } else {
                ++pool->generation[slot];
                pool->slot_to_index[slot] = SV_HANDLE_FREE_INDEX;
            }
        }
        if (pool->slot_to_index[slot] == SV_HANDLE_FREE_INDEX) {
            pool->free_next[slot] = pool->free_head;
            pool->free_head = slot;
        }
    }
}

uint64_t sv_handle_pool_bind(SvHandlePool *pool, uint32_t index)
{
    uint32_t slot;
    if (!sv_handle_pool_ready(pool) || index >= pool->capacity)
        return 0;
    slot = pool->index_to_slot[index];
    if (slot != SV_HANDLE_NO_SLOT)
        return sv_handle_pool_at(pool, index);
    slot = pool->free_head;
    if (slot >= pool->capacity)
        return 0;
    pool->free_head = pool->free_next[slot];
    pool->free_next[slot] = SV_HANDLE_NO_SLOT;
    pool->index_to_slot[index] = slot;
    pool->slot_to_index[slot] = (int32_t)index;
    return sv_handle_encode(pool, slot);
}

void sv_handle_pool_erase(SvHandlePool *pool, uint32_t index)
{
    uint32_t slot;
    if (!sv_handle_pool_at(pool, index))
        return;
    slot = pool->index_to_slot[index];
    pool->index_to_slot[index] = SV_HANDLE_NO_SLOT;
    if (pool->generation[slot] == UINT32_MAX) {
        /* Retire the slot so an old generation can never return. */
        pool->slot_to_index[slot] = SV_HANDLE_RETIRED_INDEX;
        return;
    }
    ++pool->generation[slot];
    pool->slot_to_index[slot] = SV_HANDLE_FREE_INDEX;
    pool->free_next[slot] = pool->free_head;
    pool->free_head = slot;
}

int sv_handle_pool_move(SvHandlePool *pool, uint32_t from, uint32_t to)
{
    uint32_t slot;
    if (!sv_handle_pool_at(pool, from) || to >= pool->capacity ||
        pool->index_to_slot[to] != SV_HANDLE_NO_SLOT)
        return 0;
    slot = pool->index_to_slot[from];
    pool->index_to_slot[from] = SV_HANDLE_NO_SLOT;
    pool->index_to_slot[to] = slot;
    pool->slot_to_index[slot] = (int32_t)to;
    return 1;
}

uint64_t sv_handle_pool_at(const SvHandlePool *pool, uint32_t index)
{
    uint32_t slot;
    if (!sv_handle_pool_ready(pool) || index >= pool->capacity)
        return 0;
    slot = pool->index_to_slot[index];
    if (slot >= pool->capacity || pool->slot_to_index[slot] != (int32_t)index)
        return 0;
    return sv_handle_encode(pool, slot);
}

int32_t sv_handle_pool_resolve(const SvHandlePool *pool, uint64_t handle)
{
    uint32_t slot;
    int32_t index;
    if (!sv_handle_pool_ready(pool) || (uint8_t)(handle >> 56) != pool->kind)
        return -1;
    slot = (uint32_t)handle & SV_HANDLE_SLOT_MASK;
    if (!slot || --slot >= pool->capacity ||
        (uint32_t)(handle >> 24) != pool->generation[slot])
        return -1;
    index = pool->slot_to_index[slot];
    if (index < 0 || (uint32_t)index >= pool->capacity ||
        pool->index_to_slot[(uint32_t)index] != slot)
        return -1;
    return index;
}
