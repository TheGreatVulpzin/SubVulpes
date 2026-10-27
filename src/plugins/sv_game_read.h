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
} SvWorldState;

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
} SvGameRead;
#pragma pack(pop)

#endif
