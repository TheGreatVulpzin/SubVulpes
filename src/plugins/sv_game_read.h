#ifndef SV_GAME_READ_H
#define SV_GAME_READ_H

#include "sv_game.h"

#define SV_API_GAME_READ 1u
#define SV_GAME_READ_VERSION 1u
#define SV_ENTITY_PAGE_MAX 256u
#define SV_CONNECTION_MASKED 1u
/* The longest radius of a within query, in game units. */
#define SV_WITHIN_RADIUS_MAX 4096.0f
#pragma pack(push, 8)
typedef struct {
    uint32_t size;
    SvEntity entity, account, human;
    char name[32];
    int32_t money, stocks, team, is_bot, is_admin, ready, godmode;
    int32_t input_mode, menu;
    float input_axes[4], view_axis, view_pitch, arm_axis, arm_pitch;
    uint32_t buttons;
    int32_t looks[5];
    SvVector appearance[4];
} SvPlayerState;

typedef struct {
    uint32_t size;
    SvEntity entity, player, account, vehicle;
    int32_t seat, health, bleeding, team, on_ground, move_mode;
    int32_t despawn_counter, stun_ticks, protect_ticks, reload_ticks;
    SvVector position, velocity;
    float axis, pitch, stance, body_strength;
    float input_axes[4], view_axis, view_pitch, arm_axis, arm_pitch;
    uint32_t buttons;
    int32_t looks[5];
    SvVector appearance[4];
} SvHumanState;

typedef struct {
    uint32_t size;
    SvEntity entity, parent_human, parent_item;
    int32_t type, parent_slot, time_to_live, ammo, cooldown_ticks, trigger_ticks;
    float mass;
    SvVector position, velocity, rotation[3], angular_velocity, angular_momentum;
    uint32_t child_count;
    SvEntity children[8];
    int32_t phone_id, phone_number, call_state;
    uint32_t bills_mask;
} SvItemState;

typedef struct {
    uint32_t size;
    SvEntity entity, driver;
    int32_t type, state, health, color, time_to_live, key_away;
    SvVector position, velocity, rotation[3], box_min, box_max;
    uint32_t wheel_count, wheel_body_count, seat_count;
    int32_t gear;
    float engine_rpm, throttle;
    /* The human in each seat, 0 for an empty seat. The index is the seat of the game: 0 is the driver. Only the first
     * seat_count slots can be filled. A dead body that still sits in a seat counts, so check its health. */
    SvEntity occupants[8];
} SvObjectState;

typedef struct {
    uint32_t size;
    SvEntity entity;
    char name[32];
    int32_t money, playtime_raw, looks[3];
} SvAccountState;

typedef struct {
    uint32_t size;
    SvEntity human;
    uint32_t bone;
    SvVector position, velocity, rotation[3], angular_velocity, angular_momentum;
    float mass, radius;
} SvBoneState;

typedef struct {
    uint32_t size;
    SvEntity human;
    uint32_t slot, count;
    SvEntity items[8];
} SvInventoryState;

typedef struct {
    uint32_t size, tick;
    uint64_t reset_sequence;
    int32_t mode, round, state, timer_ticks, wait_ticks, round_ticks, end_ticks;
    int32_t world_clock_raw, server_active, max_players;
    char server_name[72];
    SvVector spawn, spawn_sizes;
    uint32_t item_type_count, object_type_count;
    uint32_t headline_count;
} SvWorldState;

/* What a line query can hit. A query takes the nearest hit of the kinds in its flags. */
enum { SV_HIT_NONE = 0, SV_HIT_LEVEL, SV_HIT_OBJECT, SV_HIT_HUMAN };
enum { SV_LINE_LEVEL = 1, SV_LINE_OBJECTS = 2, SV_LINE_HUMANS = 4, SV_LINE_ALL = 7 };
/* The longest line that a query accepts, in game units. */
#define SV_LINE_LENGTH_MAX 4096.0f

typedef struct {
    uint32_t size, kind;
    SvEntity entity;            /* The object or the human that the line hit. 0 for the level and for no hit. */
    int32_t bone;               /* The bone of a human that the line hit, or -1. */
    SvVector position, normal;  /* The hit point and the normal of the surface. Both are 0 for no hit. */
    float fraction, distance;   /* The part of the line before the hit, and the length of that part. */
} SvLineHit;

typedef struct {
    uint32_t size;
    uint8_t ip[4];
    uint32_t port;
    int32_t idle_ticks, spectate;
    uint32_t flags, address_id, network_id;
} SvConnectionState;

/* Level indices last until the next reset. Building indices match the shop catalog. */
#define SV_BUILDING_MAX 4096u
#define SV_BASE_COUNT 5u
#define SV_AREA_COUNT 4u
enum { SV_BUILDING_OTHER = 0, SV_BUILDING_BASE, SV_BUILDING_SHOP, SV_BUILDING_HOSPITAL,
    SV_BUILDING_DEALERSHIP };
typedef struct {
    uint32_t size, index, kind;
    int32_t native_type, team;
    SvVector position, box_min, box_max;
} SvBuildingState;
typedef struct {
    uint32_t size, index;
    int32_t blocks_x, blocks_y, blocks_z;
    float block_size;
    SvVector origin, axes[3];
} SvAreaState;
typedef struct {
    uint32_t size, word, interior_word;
    uint16_t materials[8];
} SvBlockState;
typedef struct {
    uint32_t size;
    SvLineHit hit;
    int32_t material, area, cell[3], face;
} SvSurfaceHit;

typedef struct {
    uint32_t size, version;
    SvResult (SV_CALL *entities)(SvOwner owner, uint32_t kind, uint32_t cursor,
        SvEntity *entities, uint32_t capacity, uint32_t *count, uint32_t *next);
    SvResult (SV_CALL *player)(SvOwner owner, SvEntity entity, SvPlayerState *state);
    SvResult (SV_CALL *human)(SvOwner owner, SvEntity entity, SvHumanState *state);
    SvResult (SV_CALL *item)(SvOwner owner, SvEntity entity, SvItemState *state);
    SvResult (SV_CALL *object)(SvOwner owner, SvEntity entity, SvObjectState *state);
    SvResult (SV_CALL *account)(SvOwner owner, SvEntity entity, SvAccountState *state);
    SvResult (SV_CALL *bone)(SvOwner owner, SvEntity human, uint32_t bone, SvBoneState *state);
    SvResult (SV_CALL *inventory)(SvOwner owner, SvEntity human, uint32_t slot, SvInventoryState *state);
    SvResult (SV_CALL *world)(SvOwner owner, SvWorldState *state);
    /* The nearest hit on the line from "from" to "to". The line must be finite, inside 32768 units on each axis, and not longer
     * than SV_LINE_LENGTH_MAX. flags has at least one SV_LINE_ bit. "ignore" is a human to skip, or 0. A line that hits
     * nothing is a normal answer: the result is SV_OK and the kind is SV_HIT_NONE. The query does not change the game. */
    SvResult (SV_CALL *line)(SvOwner owner, SvVector from, SvVector to, uint32_t flags, SvEntity ignore, SvLineHit *hit);
    /* The same query along the aim of a human: from the middle of its head, in the direction that its head faces, for
     * "distance" units (more than 0, at most SV_LINE_LENGTH_MAX). The human itself is never a hit. */
    SvResult (SV_CALL *aim)(SvOwner owner, SvEntity human, float distance, uint32_t flags, SvLineHit *hit);
    /* Copies news line "index" (below SvWorldState.headline_count) to the buffer. The buffer must hold SV_HEADLINE_BUFFER
     * bytes. SV_NOT_FOUND when the line does not exist. */
    SvResult (SV_CALL *headline)(SvOwner owner, uint32_t index, char *text, uint32_t capacity);
    /* Finds the records of one kind (SV_ENTITY_HUMAN, SV_ENTITY_ITEM or SV_ENTITY_OBJECT) whose place is within "radius" of
     * "center". The place is the position of the snapshot. The references go in the order of the game slots, and the
     * distances (optional, may be NULL) go beside them. The radius is above 0 and at most SV_WITHIN_RADIUS_MAX. The capacity
     * is 1 to SV_ENTITY_PAGE_MAX. SV_LIMIT means that more records matched than fit, and the first "capacity" are given. */
    SvResult (SV_CALL *within)(SvOwner owner, uint32_t kind, SvVector center, float radius, SvEntity *entities,
        float *distances, uint32_t capacity, uint32_t *count);
    /* IP bytes use dotted address order. Port uses host order. Spectate keeps the game value (-1 means none).
     * SV_NOT_FOUND for a bot or a player without a connection. Without network, the IP is zero and SV_CONNECTION_MASKED is set. */
    SvResult (SV_CALL *connection)(SvOwner owner, SvEntity player, SvConnectionState *state);
    /* Safe callbacks only. A count from 0 to SV_BUILDING_MAX. */
    SvResult (SV_CALL *building_count)(SvOwner owner, uint32_t *count);
    SvResult (SV_CALL *building)(SvOwner owner, uint32_t index, SvBuildingState *state);
    /* Team 0 to 4. SV_NOT_FOUND when the level has no box for this base. */
    SvResult (SV_CALL *base)(SvOwner owner, uint32_t team, SvBuildingState *state);
    SvResult (SV_CALL *area)(SvOwner owner, uint32_t index, SvAreaState *state);
    /* Grid coordinates in the area, not world coordinates. An empty cell has word 0. */
    SvResult (SV_CALL *block)(SvOwner owner, uint32_t area, int32_t x, int32_t y, int32_t z, SvBlockState *state);
    /* A level line with the same bounds as line. Material -1 means unknown (landscape or city object).
     * Copies the closest surface before restoring the collision scratch record. */
    SvResult (SV_CALL *surface)(SvOwner owner, SvVector from, SvVector to, SvSurfaceHit *hit);
} SvGameRead;
#pragma pack(pop)

#endif
