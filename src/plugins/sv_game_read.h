#ifndef SV_GAME_READ_H
#define SV_GAME_READ_H

#include "sv_game.h"

#define SV_API_GAME_READ 1u
#define SV_GAME_READ_VERSION 1u
#define SV_ENTITY_PAGE_MAX 256u
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
} SvGameRead;
#pragma pack(pop)

#endif
