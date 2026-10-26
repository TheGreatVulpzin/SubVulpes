#ifndef SV_PLUGIN_HANDLES_H
#define SV_PLUGIN_HANDLES_H

#include <stdint.h>

/* Initialize fresh storage before use. Other calls accept zeroed storage.
 * Destroy a live pool before reinitializing. Keep one live pool per kind. */
typedef struct {
    uint32_t *index_to_slot;
    int32_t *slot_to_index;
    uint32_t *generation;
    uint32_t *free_next;
    uint32_t capacity;
    uint32_t free_head;
    uint8_t kind;
} SvHandlePool;

/* A handle stores kind:8, generation:32 and virtual slot plus one:24.
 * Init returns 1 on success, or 0 with a zeroed pool on failure.
 * Kind and capacity must be nonzero. Capacity is at most 0x00ffffff.
 * Handle validity ends at destroy. Reinitialization starts a new lifetime. */
int sv_handle_pool_init(SvHandlePool *pool, uint8_t kind, uint32_t capacity);
void sv_handle_pool_destroy(SvHandlePool *pool);
/* Clear keeps generation history and permanently retired slots. */
void sv_handle_pool_clear(SvHandlePool *pool);
/* Bind is idempotent. Bind and at return zero for an invalid index. */
uint64_t sv_handle_pool_bind(SvHandlePool *pool, uint32_t index);
void sv_handle_pool_erase(SvHandlePool *pool, uint32_t index);
/* Move needs an occupied source and empty destination. Success returns 1. */
int sv_handle_pool_move(SvHandlePool *pool, uint32_t from, uint32_t to);
uint64_t sv_handle_pool_at(const SvHandlePool *pool, uint32_t index);
/* Resolve returns the current index, or -1 for an invalid handle. */
int32_t sv_handle_pool_resolve(const SvHandlePool *pool, uint64_t handle);

#endif
