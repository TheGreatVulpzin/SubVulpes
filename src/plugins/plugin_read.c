#include "plugin_host.h"
#include "plugin_entities.h"
#include "sv_game_read.h"
#include "plugin_game_internal.h"
#include "sr_runtime.h"
#include <string.h>

static SvVector vector(vec3 value)
{
    SvVector result = { value.x, value.y, value.z };
    return result;
}

static SvResult context(SvOwner owner)
{
    return sv_plugins_context(owner) && sv_game_is_ready() ? SV_OK : SV_STATE;
}

static SvResult resolve(SvOwner owner, uint32_t kind, SvEntity entity, int32_t *index)
{
    if (context(owner) != SV_OK)
        return SV_STATE;
    *index = sv_entity_resolve(kind, entity);
    return *index < 0 ? SV_NOT_FOUND : SV_OK;
}

static SvResult SV_CALL read_entities(SvOwner owner, uint32_t kind, uint32_t cursor,
    SvEntity *entities, uint32_t capacity, uint32_t *count, uint32_t *next)
{
    uint32_t limit = sv_entity_capacity(kind), used = 0, end;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (!limit || cursor > limit || !capacity || capacity > SV_ENTITY_PAGE_MAX ||
        !entities || !count || !next)
        return SV_INVALID;
    end = limit - cursor > SV_ENTITY_PAGE_MAX ? cursor + SV_ENTITY_PAGE_MAX : limit;
    while (cursor < end && used < capacity) {
        SvEntity entity = sv_entity_at(kind, (int32_t)cursor++);
        if (entity)
            entities[used++] = entity;
    }
    *count = used;
    *next = cursor;
    return SV_OK;
}

#define READ_BEGIN(kind, type) \
    int32_t index; \
    SvResult result = resolve(owner, kind, entity, &index); \
    type value = { sizeof value }; \
    if (result != SV_OK) return result; \
    if (!state || state->size < sizeof value) return SV_INVALID; \
    value.entity = entity

static SvResult SV_CALL read_player(SvOwner owner, SvEntity entity, SvPlayerState *state)
{
    const Player *p;
    uint32_t i;
    READ_BEGIN(SV_ENTITY_PLAYER, SvPlayerState);
    p = &sr_players[index];
    value.account = sv_entity_at(SV_ENTITY_ACCOUNT, p->account);
    value.human = sv_entity_at(SV_ENTITY_HUMAN, p->human_id);
    memcpy(value.name, p->name, sizeof value.name - 1);
    value.money = p->money; value.stocks = p->stocks; value.team = p->team;
    value.is_bot = p->is_bot; value.is_admin = p->is_admin; value.ready = p->ready;
    value.godmode = p->godmode; value.input_mode = p->input_mode; value.menu = p->menu;
    memcpy(value.input_axes, p->input_axes, sizeof value.input_axes);
    value.view_axis = p->yaw; value.view_pitch = p->input_pitch;
    value.arm_axis = p->input_extra[0]; value.arm_pitch = p->input_extra[1];
    value.buttons = p->input_buttons;
    value.looks[0] = p->look_a; value.looks[1] = p->look_b; value.looks[2] = p->look_c;
    value.looks[3] = p->look_d; value.looks[4] = p->look_e;
    for (i = 0; i < 4; ++i)
        value.appearance[i] = vector(p->colors[i]);
    memcpy(state, &value, sizeof value);
    return SV_OK;
}

static SvResult SV_CALL read_human(SvOwner owner, SvEntity entity, SvHumanState *state)
{
    const Human *h;
    uint32_t i;
    READ_BEGIN(SV_ENTITY_HUMAN, SvHumanState);
    h = &sr_humans[index];
    value.player = sv_entity_at(SV_ENTITY_PLAYER, h->player_id);
    value.account = sv_entity_at(SV_ENTITY_ACCOUNT, h->account);
    value.vehicle = sv_entity_at(SV_ENTITY_OBJECT, h->vehicle_id);
    value.seat = value.vehicle ? h->vehicle_seat : -1;
    value.health = h->health; value.bleeding = h->bleeding;
    value.team = h->team; value.on_ground = h->on_ground; value.move_mode = h->move_mode;
    value.despawn_counter = h->despawn_timer; value.stun_ticks = h->stun_timer;
    value.protect_ticks = h->protect_timer; value.reload_ticks = h->reload_timer;
    value.position = vector(h->pos); value.velocity = vector(h->vel);
    value.axis = h->axis; value.pitch = h->pitch; value.stance = h->stance;
    value.body_strength = h->body_strength;
    memcpy(value.input_axes, h->input_axes, sizeof value.input_axes);
    value.view_axis = h->view_axis; value.view_pitch = h->view_pitch;
    value.arm_axis = h->arm_axis; value.arm_pitch = h->arm_pitch;
    value.buttons = h->input_buttons;
    value.looks[0] = h->look_a; value.looks[1] = h->look_b; value.looks[2] = h->look_c;
    value.looks[3] = h->look_d; value.looks[4] = h->look_e;
    for (i = 0; i < 4; ++i)
        value.appearance[i] = vector(h->colors[i]);
    memcpy(state, &value, sizeof value);
    return SV_OK;
}

static SvResult SV_CALL read_item(SvOwner owner, SvEntity entity, SvItemState *state)
{
    const Item *it;
    int i;
    READ_BEGIN(SV_ENTITY_ITEM, SvItemState);
    it = &sr_items[index];
    if (it->num_children < 0 || it->num_children > 8)
        return SV_FAILED;
    value.parent_human = sv_entity_at(SV_ENTITY_HUMAN, it->parent_human);
    value.parent_item = sv_entity_at(SV_ENTITY_ITEM, it->parent_item);
    value.type = it->type; value.parent_slot = value.parent_human ? it->parent_slot : -1;
    value.time_to_live = it->time_to_live; value.ammo = it->ammo;
    value.cooldown_ticks = it->cooldown; value.trigger_ticks = it->trigger_ticks;
    value.mass = it->mass; value.position = vector(it->pos); value.velocity = vector(it->vel);
    value.angular_velocity = vector(it->angvel); value.angular_momentum = vector(it->ang_momentum);
    for (i = 0; i < 3; ++i)
        value.rotation[i] = vector(it->rot[i]);
    value.child_count = (uint32_t)it->num_children;
    for (i = 0; i < it->num_children; ++i)
        value.children[i] = sv_entity_at(SV_ENTITY_ITEM, it->children[i]);
    value.phone_id = it->phone_id; value.phone_number = it->phone_number;
    value.call_state = it->call_state; value.bills_mask = it->bills_mask;
    memcpy(state, &value, sizeof value);
    return SV_OK;
}

static SvResult SV_CALL read_object(SvOwner owner, SvEntity entity, SvObjectState *state)
{
    const Object *o;
    int i;
    READ_BEGIN(SV_ENTITY_OBJECT, SvObjectState);
    o = &sr_objects[index];
    if (o->num_wheels < 0 || o->num_wheels > 32 || o->num_wheel_bodies < 0 ||
        o->num_wheel_bodies > 16 || o->num_seats < 0 || o->num_seats > 7)
        return SV_FAILED;
    for (i = 0; i < SR_MAX_HUMAN && o->num_seats > 0; ++i) {
        const Human *h = &sr_humans[i];
        SvEntity player;
        if (!h->active || h->health < 1 || h->vehicle_id != index || h->vehicle_seat != 0)
            continue;
        player = sv_entity_at(SV_ENTITY_PLAYER, h->player_id);
        if (player && sr_players[h->player_id].human_id == i) {
            value.driver = player;
            break;
        }
    }
    value.type = o->type; value.state = o->state; value.health = o->health;
    value.color = o->color; value.time_to_live = o->time_to_live; value.key_away = o->key_away;
    value.position = vector(o->pos); value.velocity = vector(o->vel);
    for (i = 0; i < 3; ++i)
        value.rotation[i] = vector(o->rot[i]);
    value.box_min = vector(o->box_min); value.box_max = vector(o->box_max);
    value.wheel_count = (uint32_t)o->num_wheels;
    value.wheel_body_count = (uint32_t)o->num_wheel_bodies;
    value.seat_count = (uint32_t)o->num_seats;
    value.gear = o->gear; value.engine_rpm = o->engine_rpm; value.throttle = o->throttle;
    memcpy(state, &value, sizeof value);
    return SV_OK;
}

static SvResult SV_CALL read_account(SvOwner owner, SvEntity entity, SvAccountState *state)
{
    const Account *a;
    READ_BEGIN(SV_ENTITY_ACCOUNT, SvAccountState);
    a = &sr_data_accounts.records[index];
    memcpy(value.name, a->name, sizeof value.name - 1);
    value.money = a->money; value.playtime_raw = a->playtime;
    value.looks[0] = a->look_a; value.looks[1] = a->look_b; value.looks[2] = a->look_c;
    memcpy(state, &value, sizeof value);
    return SV_OK;
}

static SvResult SV_CALL read_bone(SvOwner owner, SvEntity human, uint32_t bone, SvBoneState *state)
{
    int32_t index;
    uint32_t i;
    const Bone *b;
    SvResult result = resolve(owner, SV_ENTITY_HUMAN, human, &index);
    SvBoneState value = { sizeof value };
    if (result != SV_OK)
        return result;
    if (bone >= 15 || !state || state->size < sizeof value)
        return SV_INVALID;
    b = &sr_humans[index].bones[bone];
    value.human = human; value.bone = bone;
    value.position = vector(b->pos); value.velocity = vector(b->vel);
    value.angular_velocity = vector(b->w); value.angular_momentum = vector(b->ang_momentum);
    value.mass = b->mass; value.radius = b->radius;
    for (i = 0; i < 3; ++i)
        value.rotation[i] = vector(b->rot[i]);
    memcpy(state, &value, sizeof value);
    return SV_OK;
}

static SvResult SV_CALL read_inventory(SvOwner owner, SvEntity human, uint32_t slot, SvInventoryState *state)
{
    int32_t index, i;
    const InventorySlot *inventory;
    SvResult result = resolve(owner, SV_ENTITY_HUMAN, human, &index);
    SvInventoryState value = { sizeof value };
    if (result != SV_OK)
        return result;
    if (slot >= 8 || !state || state->size < sizeof value)
        return SV_INVALID;
    inventory = &sr_humans[index].inventory[slot];
    if (inventory->count < 0 || inventory->count > 8)
        return SV_FAILED;
    value.human = human; value.slot = slot; value.count = (uint32_t)inventory->count;
    for (i = 0; i < inventory->count; ++i)
        value.items[i] = sv_entity_at(SV_ENTITY_ITEM, inventory->items[i]);
    memcpy(state, &value, sizeof value);
    return SV_OK;
}

static SvResult SV_CALL read_world(SvOwner owner, SvWorldState *state)
{
    SvWorldState value = { sizeof value };
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (!state || state->size < sizeof value)
        return SV_INVALID;
    value.tick = (uint32_t)sr_game.ticks; value.reset_sequence = sv_game_reset_sequence();
    value.mode = sr_game.gametype; value.round = sr_game.round_number; value.state = sr_game.state;
    value.timer_ticks = sr_game.timer; value.wait_ticks = sr_game.wait_time;
    value.round_ticks = sr_game.round_time; value.end_ticks = sr_game.end_time;
    value.world_clock_raw = sr_data_game_world.clock;
    value.server_active = sr_data_server.active; value.max_players = sr_data_server.max_players;
    memcpy(value.server_name, sr_data_server.name, sizeof value.server_name - 1);
    value.spawn = vector(sr_data_game_world.spawn);
    value.spawn_sizes.x = sr_data_game_world.spawn_size_x;
    value.spawn_sizes.y = sr_data_game_world.spawn_size_y;
    value.spawn_sizes.z = sr_data_game_world.spawn_size_z;
    value.item_type_count = 27; value.object_type_count = 14;
    memcpy(state, &value, sizeof value);
    return SV_OK;
}

const SvGameRead sv_game_read = {
    sizeof(SvGameRead), SV_GAME_READ_VERSION, read_entities,
    read_player, read_human, read_item, read_object, read_account,
    read_bone, read_inventory, read_world
};
