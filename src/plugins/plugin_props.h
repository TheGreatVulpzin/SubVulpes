#ifndef SV_PLUGIN_PROPS_H
#define SV_PLUGIN_PROPS_H

#include <stddef.h>
#include "sv_game_props.h"

/* The entity kind 0 means the property belongs to the world and not to one record. The entity argument must be 0.
 * A world property that has one value for each type of a list (for example each object type) uses the slot for the type. */
#define SV_ENTITY_WORLD 0u

/* One row of the property table. The field function gives the address of the number for a record and a slot. A vector
 * row also gives add_vector, because a push is not a plain sum. The check runs before a write and may refuse it. The
 * after step runs after a write, for the records and the clients that must follow the number. */
typedef struct {
    const char *name;
    uint32_t kind, type, count, ops;
    double min, max;
    void *(*field)(int32_t index, uint32_t slot);
    SvResult (*check)(int32_t index, SvValue value);
    void (*after)(int32_t index);
    SvResult (*add_vector)(int32_t index, uint32_t slot, SvVector change);
    /* 1 for a row of a table that the game fills once at the start (for example the item types): the host keeps the first
     * value of each slot that a plugin changes, and puts it back when that plugin stops. Needs at most 32 slots and no
     * vector. A group that omits this member gets 0. */
    int restore;
} Prop;

/* Adds a group of rows to the table. The rows must stay valid until the process ends, and the names must not repeat a
 * name that is already in the table. Adding the same group again does nothing. Returns SV_OK, SV_INVALID for bad rows or
 * a repeated name, and SV_LIMIT when the table is full (at most 8 groups and 256 rows). A plugin area calls this from
 * its install function, before the first plugin tick. */
SvResult sv_props_add_group(const Prop *rows, size_t count);

/* Puts back the first value of every slot of a restore row that this owner changed last. Called when a plugin stops. */
void sv_props_release(SvOwner owner);

#endif
