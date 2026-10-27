#ifndef SV_GAME_PROPS_H
#define SV_GAME_PROPS_H

#include "sv_game.h"

#define SV_API_GAME_PROPS 10u
#define SV_GAME_PROPS_VERSION 1u
#define SV_PROP_NAME_MAX 32

/* The type of a property value, and the operations that a property offers. */
enum { SV_PROP_INT = 1, SV_PROP_FLOAT, SV_PROP_VECTOR };
enum { SV_PROP_GET = 1, SV_PROP_SET = 2, SV_PROP_ADD = 4 };

typedef union {
    int32_t i;
    float f;
    SvVector v;
} SvValue;

#pragma pack(push, 8)
typedef struct {
    uint32_t size, id, entity_kind, type, count, ops;
    char name[SV_PROP_NAME_MAX];
    /* For an integer or a float: the lowest and the highest value that a set or an add may leave in the field.
     * For a vector: min is 0 and max is the longest change that one add accepts. */
    double min, max;
} SvPropInfo;

/* One table for the numbers of the game that a plugin may read and change. Each property has a name, an entity kind,
 * a type, a count of slots (1 for a plain field, 15 for the bones of a human) and a range. A new field of the game adds
 * a row to the table in plugin_props.c. It adds no method and no ABI change.
 *
 * The context rule is the one of the write table: a host callback on the server thread, a ready world and no stop
 * transition, otherwise SV_STATE. A call checks everything before it changes the game, so an error leaves the game
 * unchanged. The order of the checks and of the codes:
 *   SV_NOT_FOUND  the property id is unknown, or the entity is not of the kind of the property
 *   SV_DENIED     the property does not offer the operation
 *   SV_INVALID    the slot is out of range, or the value is out of range or not finite
 * A row can add its own checks and codes. They are in docs/plugin-game-api.md. */
typedef struct {
    uint32_t size, version;
    /* Finds a property by its exact name. The id is stable for the life of the process, not between versions. */
    SvResult (SV_CALL *find)(SvOwner owner, const char *name, uint32_t *id);
    /* Describes the property with this id. Ids start at 1 with no gap, so a plugin lists all by raising the id until
     * SV_NOT_FOUND. */
    SvResult (SV_CALL *info)(SvOwner owner, uint32_t id, SvPropInfo *info);
    SvResult (SV_CALL *get)(SvOwner owner, uint32_t id, SvEntity entity, uint32_t slot, SvValue *value);
    /* Writes the value. Needs SV_PROP_SET. The value must be inside the range of the property. */
    SvResult (SV_CALL *set)(SvOwner owner, uint32_t id, SvEntity entity, uint32_t slot, SvValue value);
    /* Adds the change to the value. Needs SV_PROP_ADD. The sum of a number must stay inside the range. The change of a
     * vector must be finite and not longer than the max of the property. */
    SvResult (SV_CALL *add)(SvOwner owner, uint32_t id, SvEntity entity, uint32_t slot, SvValue change);
} SvGameProps;
#pragma pack(pop)

#endif
