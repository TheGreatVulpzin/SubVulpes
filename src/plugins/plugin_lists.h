#ifndef SV_PLUGIN_LISTS_H
#define SV_PLUGIN_LISTS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Lists that the host fills (rule 6a of docs/plugin-abi.md). The items are the last member of the container, so a larger
 * container from a caller means larger items. The host writes the items with the stride that the caller knows, and it
 * never writes more than the caller gave. */

/* A container of the current version must not shrink an item below its first size. */
#define SV_LIST_ITEM_CHECK(name, type, first_size) \
    typedef char name##_item_check[(sizeof(type) >= (first_size)) ? 1 : -1]

/* The stride of one item in the container of a caller. caller_size is the size that the caller set. items_offset is where
 * the items start. 0 when the container cannot hold the capacity of items of the first size. */
static __inline uint32_t sv_list_stride(uint32_t caller_size, uint32_t items_offset, uint32_t capacity, uint32_t first_size,
    uint32_t current_size)
{
    uint32_t stride;
    if (!capacity || caller_size < items_offset || (caller_size - items_offset) / capacity < first_size)
        return 0;
    stride = (caller_size - items_offset) / capacity;
    return stride > current_size ? current_size : stride;
}

/* Clears the room of the items and copies count items of the host with the stride of the caller. */
static __inline void sv_list_put(void *items, const void *host_items, uint32_t count, uint32_t capacity, uint32_t host_size,
    uint32_t stride)
{
    uint32_t i;
    memset(items, 0, (size_t)capacity * stride);
    for (i = 0; i < count && i < capacity; ++i)
        memcpy((unsigned char *)items + (size_t)i * stride, (const unsigned char *)host_items + (size_t)i * host_size, stride);
}

#endif
