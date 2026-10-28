#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plugin_entities.h"
#include "plugin_host.h"
#include "humans.h"
#include "items.h"
#include "logic.h"
#include "objects.h"
#include "player.h"
#include "events.h"
#include "server.h"
#include "sr_math.h"
#include "sr_server.h"
#include "plugin_game_internal.h"
#include "plugin_props.h"
#include "plugin_game_events.h"
#include "plugin_damage.h"

_Static_assert(SV_HOST_V1_SIZE == 24, "Preserve the original host prefix");
_Static_assert(sizeof(SvStatus) == 20, "Preserve status ABI v1");
_Static_assert(sizeof(SvEvent) == 12, "Preserve event ABI v1");
_Static_assert(sizeof(SvPlugin) == 92, "Preserve plugin ABI v1");
void test_physics_bounds(void);

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); exit(1); \
} } while (0)

static const vec3 position = { 10.0f, 20.0f, 30.0f };
static const vec3 rotation[3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };

static void player_lifetime(void)
{
    int player = createplayer(), next;
    SvEntity old, surviving;
    CHECK(player == 0);
    old = sv_entity_at(SV_ENTITY_PLAYER, player);
    CHECK(old && sv_entity_resolve(SV_ENTITY_PLAYER, old) == player);
    CHECK(sv_entity_resolve(SV_ENTITY_HUMAN, old) == -1);
    CHECK(sv_entity_at(SV_ENTITY_PLAYER, -1) == 0);
    deleteplayer(player);
    next = createplayer();
    CHECK(next == player && sv_entity_resolve(SV_ENTITY_PLAYER, old) == -1);
    CHECK(sv_entity_at(SV_ENTITY_PLAYER, next) != old);
    surviving = sv_entity_at(SV_ENTITY_PLAYER, next);
    player = createplayer();
    sr_players[player].is_bot = 1;
    old = sv_entity_at(SV_ENTITY_PLAYER, player);
    sr_game.state = 3;
    logicsimulation_world();
    CHECK(!sr_players[player].active);
    CHECK(sv_entity_resolve(SV_ENTITY_PLAYER, old) == -1);
    CHECK(sv_entity_resolve(SV_ENTITY_PLAYER, surviving) == next);
}

static void body_lifetime(void)
{
    float pos[3] = {10, 20, 30};
    float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    int human = createhuman(pos, rot, -1);
    SvEntity old = sv_entity_at(SV_ENTITY_HUMAN, human);
    CHECK(human == 0 && old);
    deletehuman((uint32_t)human);
    CHECK(createhuman(pos, rot, -1) == human);
    CHECK(sv_entity_resolve(SV_ENTITY_HUMAN, old) == -1);
    CHECK(sv_entity_at(SV_ENTITY_HUMAN, human) != old);
}

static void item_lifetime(void)
{
    uint32_t item;
    SvEntity old;
    sr_itemtypes[0].inv_inertia.x = sr_itemtypes[0].inv_inertia.y = sr_itemtypes[0].inv_inertia.z = 1;
    item = createitem(0, &position, NULL, rotation);
    old = sv_entity_at(SV_ENTITY_ITEM, (int32_t)item);
    CHECK(item == 0 && old);
    sr_items[item].time_to_live = 0;
    itemtimetolive();
    CHECK(sv_entity_resolve(SV_ENTITY_ITEM, old) == -1);
    CHECK(createitem(0, &position, NULL, rotation) == item);
    CHECK(sv_entity_at(SV_ENTITY_ITEM, (int32_t)item) != old);
}

static void object_lifetime(void)
{
    int object;
    SvEntity old;
    memset(&sr_data_objecttype[0], 0, sizeof sr_data_objecttype[0]);
    object = createobject(0, &position, NULL, rotation, 0);
    old = sv_entity_at(SV_ENTITY_OBJECT, object);
    CHECK(object == 0 && old);
    sr_objects[object].time_to_live = 0;
    objecttimetolive();
    CHECK(sv_entity_resolve(SV_ENTITY_OBJECT, old) == -1);
    CHECK(createobject(0, &position, NULL, rotation, 0) == object);
    CHECK(sv_entity_at(SV_ENTITY_OBJECT, object) != old);
}

static void account_lifetime(void)
{
    SvEntity old;
    int account;
    strcpy(sr_players[0].name, "Public test account");
    sr_players[0].account_id[0] = 123;
    sr_players[0].account_id[1] = 456;
    account = createaccount(0);
    old = sv_entity_at(SV_ENTITY_ACCOUNT, account);
    CHECK(account == 0 && old);
    CHECK(createaccount(0) == account);
    CHECK(sv_entity_at(SV_ENTITY_ACCOUNT, account) == old);
    /* CTest runs in a dedicated folder with no account file. */
    sr_loadaccountsserver();
    CHECK(sr_data_accounts.count == 0);
    CHECK(sv_entity_resolve(SV_ENTITY_ACCOUNT, old) == -1);
    CHECK(createaccount(0) == account);
    CHECK(sv_entity_at(SV_ENTITY_ACCOUNT, account) != old);
}

static int near_value(float a, float b)
{
    return a - b < 0.001f && b - a < 0.001f;
}

/* Both sides of every item link must agree: item and human, item and parent item. */
static void check_item_links(void)
{
    int i, h, s, k, found;
    for (i = 0; i < SR_MAX_ITEM; ++i) {
        const Item *it = &sr_items[i];
        if (!it->active)
            continue;
        if (it->parent_human != -1) {
            const InventorySlot *slot = &sr_humans[it->parent_human].inventory[it->parent_slot];
            for (found = 0, k = 0; k < slot->count; ++k)
                found += slot->items[k] == i;
            CHECK(found == 1);
        }
        if (it->parent_item != -1) {
            const Item *parent = &sr_items[it->parent_item];
            CHECK(parent->active);
            for (found = 0, k = 0; k < parent->num_children; ++k)
                found += parent->children[k] == i;
            CHECK(found == 1);
        }
    }
    for (h = 0; h < SR_MAX_HUMAN; ++h) {
        if (!sr_humans[h].active)
            continue;
        for (s = 0; s < 8; ++s)
            for (k = 0; k < sr_humans[h].inventory[s].count; ++k) {
                const Item *it = &sr_items[sr_humans[h].inventory[s].items[k]];
                CHECK(it->active && it->parent_human == h && it->parent_slot == s);
            }
    }
}

/* The number of bits that packevent writes for one connection. */
static int event_bits(int index, int conn)
{
    sr_bsdsocket.send_size = 0;
    sr_bsdsocket.send_bit = 0;
    packevent(index, conn);
    return sr_bsdsocket.send_size * 8 + sr_bsdsocket.send_bit;
}

/* The bot of the fixture ends in the world logic pass of the game, and its human goes without a player. */
static void npc_check(void)
{
    int i, npc = -1, human;
    for (i = 2; i < 256; ++i)
        if (sr_players[i].active && sr_players[i].is_bot && !strcmp(sr_players[i].name, "NPC"))
            npc = i;
    CHECK(npc >= 0 && sr_players[npc].team == 6 && sr_players[npc].despawn_time == -1);
    human = sr_players[npc].human_id;
    CHECK(human >= 0 && sr_humans[human].active && sr_humans[human].player_id == npc);
    sr_game.state = 3;
    logicsimulation_world();
    CHECK(!sr_players[npc].active && sr_humans[human].despawn_timer == 0 && sr_humans[human].player_id == -1);
}

static int target_car, rider_car, rider_a, rider_b;
static int test_world_value;

/* A car with two faces for the line queries: face 0 in the plane x = 500 and face 1 in the plane z = 150. */
static void make_target(void)
{
    static const vec3 corners[6] = { {500, 0, 0}, {500, 10, 0}, {500, 0, 10}, {80, 30, 150}, {130, 30, 150}, {80, 80, 150} };
    static const vec3 at = { 300.0f, 20.0f, 300.0f };
    Object *ob;
    int i;
    target_car = createobject(0, &at, NULL, rotation, 4);
    CHECK(target_car > 0);
    ob = &sr_objects[target_car];
    ob->num_faces = 2;
    for (i = 0; i < 6; ++i) {
        ob->verts[i].pos = corners[i];
        ob->verts[i].particle = 0;
    }
    for (i = 0; i < 2; ++i) {
        ob->faces[i].health = 1;
        ob->faces[i].corners = 3;
        ob->faces[i].verts[0] = 3 * i;
        ob->faces[i].verts[1] = 3 * i + 1;
        ob->faces[i].verts[2] = 3 * i + 2;
    }
    ob->num_particles = 2;
    ob->particles[0] = 0;
    ob->particles[1] = 1;
    sr_particles[0].vel = sr_particles[1].vel = (vec3){ 0, 0, 0 };
    ob->box_min = (vec3){ -1, -1, -1 };
    ob->box_max = (vec3){ 501, 81, 201 };
}

/* Two humans sit in one car. The first has a bone under the ground, so the exit must lift it to the ground. */
static void seat_riders(void)
{
    static const vec3 at = { 300.0f, 0.5f, 300.0f };
    float pos[3] = { 300.0f, 0.5f, 300.0f };
    float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    int k;
    rider_car = createobject(0, &at, NULL, rotation, 2);
    rider_a = createhuman(pos, rot, -1);
    rider_b = createhuman(pos, rot, -1);
    CHECK(rider_car > 0 && rider_a > 0 && rider_b > 0);
    sr_humans[rider_a].vehicle_id = rider_car;
    sr_humans[rider_a].vehicle_seat = 0;
    sr_humans[rider_b].vehicle_id = rider_car;
    sr_humans[rider_b].vehicle_seat = 1;
    for (k = 0; k < 15; ++k)
        sr_humans[rider_a].bones[k].vel = sr_humans[rider_b].bones[k].vel = (vec3){ 1.0f, 2.0f, 3.0f };
    sr_humans[rider_a].bones[5].pos.y = -1.0f;
}

/* The exit of the first rider stops its bones. The exit of the second keeps the speed. */
static void riders_check(void)
{
    int k;
    CHECK(sr_humans[rider_a].vehicle_id == -1 && sr_humans[rider_b].vehicle_id == -1);
    CHECK(sr_objects[rider_car].drive_buttons == 4);
    for (k = 0; k < 15; ++k) {
        const vec3 *a = &sr_humans[rider_a].bones[k].vel, *b = &sr_humans[rider_b].bones[k].vel;
        CHECK(a->x == 0.0f && a->y == 0.0f && a->z == 0.0f);
        CHECK(b->x == 1.0f && b->y == 2.0f && b->z == 3.0f);
    }
    CHECK(near_value(sr_humans[rider_a].bones[5].pos.y, 0.0625f));
    CHECK(sr_humans[rider_a].exit_timer == 0 && sr_humans[rider_b].exit_timer == 0);
}

/* The sound event, the push on a bone and on the particles of a car, and the spawn protection counter. */
static void effects_check(uint32_t events, vec3 bone_before)
{
    uint32_t i, sounds = 0;
    const Human *hu = &sr_humans[0];
    for (i = events; i < sr_numofevents; ++i)
        if (sr_events[i].type == 9) {
            ++sounds;
            CHECK(sr_events[i].param == 21 && sr_events[i].sound_a == 1.0f && sr_events[i].sound_b == 0.15f);
            CHECK(sr_events[i].pos.x == 100.0f && sr_events[i].pos.y == 50.0f && sr_events[i].pos.z == 200.0f);
        }
    CHECK(sounds == 1);
    CHECK(near_value(hu->bones[3].vel.x, bone_before.x + 1.0f) && near_value(hu->bones[3].vel.y, bone_before.y + 2.0f) &&
          near_value(hu->bones[3].vel.z, bone_before.z + 3.0f));
    CHECK(sr_particles[0].vel.x == 1.0f && sr_particles[0].vel.y == 2.0f && sr_particles[0].vel.z == 3.0f);
    CHECK(sr_particles[1].vel.x == 1.0f && sr_particles[1].vel.y == 2.0f && sr_particles[1].vel.z == 3.0f);
    CHECK(hu->protect_timer == 500);
    CHECK(test_world_value == 5);
}

/* The news lines that the fixture wrote: the storage, the events and what a client that joins late gets. The fixture started
 * with the lines "first", "second" and "third". */
static void headline_check(uint32_t events)
{
    uint32_t i, kind3 = 0, pair_a = 0, pair_b = 0, pair_c = 0, ticks = (uint32_t)sr_game.ticks;
    const char *line = sr_data_newspaper.lines[3];
    CHECK(sr_data_newspaper.num_lines == 1024);
    CHECK(!strcmp(sr_data_newspaper.lines[0], "first") && !strcmp(sr_data_newspaper.lines[1], "second replaced"));
    CHECK(!strcmp(sr_data_newspaper.lines[2], "third") && !sr_data_newspaper.lines[4][0]);
    CHECK(strlen(line) == 100 && line[0] == 'a' && line[62] == 'a' && line[63] == 'b' && line[99] == 'b');
    CHECK(!strcmp(sr_data_newspaper.lines[1023], "last"));
    for (i = events; i != sr_numofevents; i = (i + 1) & 0xffff) {
        const Event *ev = &sr_events[i];
        if (ev->type != 2 || ev->attach_type != 3)
            continue;
        ++kind3;
        if (ev->attach_id == 1) {
            CHECK(ev->range_mode == (int)pair_a && !strcmp(ev->message, pair_a ? "" : "second replaced"));
            ++pair_a;
        } else if (ev->attach_id == 3) {
            CHECK(ev->range_mode == (int)pair_b);
            CHECK(pair_b ? (strlen(ev->message) == 37 && ev->message[0] == 'b') : (strlen(ev->message) == 63 && ev->message[62] == 'a'));
            ++pair_b;
        } else if (ev->attach_id == 4) {
            CHECK(ev->range_mode == (int)pair_c && !ev->message[0] && ev->param == 0);
            ++pair_c;
        }
    }
    /* Lines 1, 3 and 4 have two events each, and lines 5 to 1023 have two each, and line 1023 is written twice. */
    CHECK(pair_a == 2 && pair_b == 2 && pair_c == 2 && kind3 == 2046);
    /* A message is a filler for a client that is more than 600 ticks late. A headline stays whole, so a late client gets it. */
    sr_game.ticks += 1000;
    CHECK(event_bits((int)events, 0) == 6);
    for (i = events; i != sr_numofevents; i = (i + 1) & 0xffff)
        if (sr_events[i].type == 2 && sr_events[i].attach_type == 3) {
            CHECK(event_bits((int)i, 0) > 6);
            break;
        }
    sr_game.ticks = (int)ticks;
}

/* The places in the core that post events, run with real constructors: a spawn for a human with a player (and none for a
 * human with no player), a death once with the last shooter as the killer, a death with no known killer, and a leave. */
static void core_events(void)
{
    float pos[3] = { 700.0f, 40.0f, 700.0f }, rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    SvGameEvent e;
    SvEntity shooter_player, victim_player, shooter_ref, victim_ref;
    int shooter_p, shooter_h, victim_p, victim_h, loose;
    sv_events_clear();
    shooter_p = createplayer();
    shooter_h = createhuman(pos, rot, shooter_p);
    victim_p = createplayer();
    victim_h = createhuman(pos, rot, victim_p);
    CHECK(shooter_p >= 0 && shooter_h >= 0 && victim_p >= 0 && victim_h >= 0);
    sr_players[shooter_p].human_id = shooter_h;
    sr_players[victim_p].human_id = victim_h;
    shooter_player = sv_entity_at(SV_ENTITY_PLAYER, shooter_p);
    victim_player = sv_entity_at(SV_ENTITY_PLAYER, victim_p);
    shooter_ref = sv_entity_at(SV_ENTITY_HUMAN, shooter_h);
    victim_ref = sv_entity_at(SV_ENTITY_HUMAN, victim_h);
    CHECK(sv_events_next(&e) && e.base.kind == SV_EVENT_SPAWN && e.base.size == sizeof e && e.player == shooter_player &&
          e.human == shooter_ref && e.actor == 0);
    CHECK(sv_events_next(&e) && e.base.kind == SV_EVENT_SPAWN && e.player == victim_player && e.human == victim_ref);
    loose = createhuman(pos, rot, -1);
    CHECK(loose >= 0 && !sv_events_next(&e));
    /* A death with the last shooter as the killer. The event comes once, and not again on the next tick. */
    sv_game_event_hit(victim_h, shooter_p);
    sr_humans[victim_h].health = -50;
    humansimulation();
    CHECK(sv_events_next(&e) && e.base.kind == SV_EVENT_DEATH && e.player == victim_player && e.human == victim_ref &&
          e.actor == shooter_ref);
    CHECK(!sv_events_next(&e));
    humansimulation();
    CHECK(!sv_events_next(&e));
    /* A death that no bullet caused has no killer. A human with no player gives no death event. */
    sr_humans[shooter_h].health = -50;
    sr_humans[loose].health = -50;
    humansimulation();
    CHECK(sv_events_next(&e) && e.base.kind == SV_EVENT_DEATH && e.human == shooter_ref && e.actor == 0);
    CHECK(!sv_events_next(&e));
    /* A leave: the reference of the player is stale at the delivery. */
    deleteplayer(shooter_p);
    CHECK(sv_events_next(&e) && e.base.kind == SV_EVENT_LEAVE && e.player == shooter_player && e.human == 0);
    CHECK(sv_entity_resolve(SV_ENTITY_PLAYER, shooter_player) == -1);
    deleteplayer(victim_p);
    CHECK(sv_events_next(&e) && e.base.kind == SV_EVENT_LEAVE && e.player == victim_player);
    CHECK(!sv_events_next(&e));
    /* Indexes out of range do nothing and post nothing. */
    sv_game_event_spawn(-1, -1);
    sv_game_event_spawn(0, SR_MAX_HUMAN);
    sv_game_event_spawn(SR_MAX_PLAYER, 0);
    sv_game_event_death(-1);
    sv_game_event_death(SR_MAX_HUMAN);
    sv_game_event_leave(-1);
    sv_game_event_leave(SR_MAX_PLAYER);
    sv_game_event_hit(-1, 0);
    sv_game_event_hit(SR_MAX_HUMAN, 0);
    CHECK(!sv_events_next(&e));
    deletehuman((uint32_t)shooter_h);
    deletehuman((uint32_t)victim_h);
    deletehuman((uint32_t)loose);
    sv_events_clear();
}

/* The event queue: the core posts a spawn, two deaths and a leave. One death names a human that is gone by the delivery.
 * A post of a kind that the core does not post, or of two kinds at once, does nothing. */
static void post_events(void)
{
    SvEntity player_ref = sv_entity_at(SV_ENTITY_PLAYER, 0), human_ref = sv_entity_at(SV_ENTITY_HUMAN, 0), stale;
    float pos[3] = { 500.0f, 30.0f, 500.0f }, rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    int temp;
    CHECK(sv_plugins_wants(SV_EVENT_SPAWN) && sv_plugins_wants(SV_EVENT_DEATH) && sv_plugins_wants(SV_EVENT_LEAVE));
    CHECK(sv_plugins_wants(SV_EVENT_TICK) && !sv_plugins_wants(SV_EVENT_STOP) && !sv_plugins_wants(0x4000u));
    sv_events_post(SV_EVENT_TICK, 1, player_ref, human_ref, 0);
    sv_events_post(SV_EVENT_STOP, 1, player_ref, human_ref, 0);
    sv_events_post(0x4000u, 1, player_ref, human_ref, 0);
    sv_events_post(0, 1, player_ref, human_ref, 0);
    sv_events_post(SV_EVENT_SPAWN | SV_EVENT_DEATH, 1, player_ref, human_ref, 0);
    CHECK(sv_events_pending() == 0);
    sv_events_post(SV_EVENT_SPAWN, 11, player_ref, human_ref, 0);
    sv_events_post(SV_EVENT_DEATH, 12, player_ref, human_ref, 0);
    sv_events_post(SV_EVENT_LEAVE, 13, player_ref, 0, 0);
    temp = createhuman(pos, rot, -1);
    CHECK(temp >= 0);
    stale = sv_entity_at(SV_ENTITY_HUMAN, temp);
    sv_events_post(SV_EVENT_DEATH, 14, player_ref, stale, 0);
    deletehuman((uint32_t)temp);
    CHECK(sv_events_pending() == 4);
}

/* More records than the queue holds: the queue keeps 1024 and counts the rest. A lost record leads the next delivery. */
static void post_flood(void)
{
    int i;
    /* The earlier calls of the test made real events too (a bot spawned and left). Start with an empty queue. */
    sv_events_clear();
    for (i = 0; i < 1100; ++i)
        sv_events_post(SV_EVENT_SPAWN, 20, 0, 0, 0);
    CHECK(sv_events_pending() == 1024 + 1);
}

/* The damage scales that the plugin set: human 0 takes 0.5 and deals 2, the car takes 0.5. The rounding goes to the nearest
 * whole number, a half goes up, a small amount keeps its sign, and the result has a cap. The real function of the core
 * for the damage of an object uses the scale. */
static void damage_check(void)
{
    CHECK(sv_damage_human(0, -1, 3, SV_DAMAGE_BULLET) == 2 && sv_damage_human(0, -1, 100, SV_DAMAGE_BULLET) == 50);
    CHECK(sv_damage_human(rider_b, -1, 100, SV_DAMAGE_BULLET) == 100);
    CHECK(sv_damage_human(rider_b, 0, 10, SV_DAMAGE_BULLET) == 20 && sv_damage_human(0, 0, 10, SV_DAMAGE_BULLET) == 10);
    CHECK(sv_damage_human(0, -1, 0, SV_DAMAGE_BULLET) == 0 && sv_damage_human(0, -1, -5, SV_DAMAGE_BULLET) == -5);
    CHECK(sv_damage_human(-1, -1, 7, SV_DAMAGE_BULLET) == 7 && sv_damage_human(SR_MAX_HUMAN, SR_MAX_PLAYER, 7, SV_DAMAGE_BULLET) == 7);
    CHECK(sv_damage_scale_object(target_car) == 0.5f && sv_damage_scale_object(rider_car) == 1.0f);
    CHECK(sv_damage_scale_object(-1) == 1.0f && sv_damage_object(-1, 9) == 9 && sv_damage_object(SR_MAX_OBJECT, 9) == 9);
    CHECK(sv_damage_object(target_car, 1) == 1 && sv_damage_object(target_car, 40) == 20 && sv_damage_object(rider_car, 40) == 40);
    sr_objects[target_car].health = 100;
    applydamageobject(target_car, 40);
    CHECK(sr_objects[target_car].health == 80);
    sr_objects[rider_car].health = 100;
    applydamageobject(rider_car, 40);
    CHECK(sr_objects[rider_car].health == 60);
    *sv_damage_field_human_taken(rider_b) = 100.0f;
    CHECK(sv_damage_human(rider_b, -1, 10000, SV_DAMAGE_BULLET) == 100000);
    *sv_damage_field_human_taken(rider_b) = 0.0f;
    CHECK(sv_damage_human(rider_b, -1, 100, SV_DAMAGE_BULLET) == 0);
    sv_damage_reset_human(rider_b);
    CHECK(sv_damage_human(rider_b, -1, 100, SV_DAMAGE_BULLET) == 100);
    sv_damage_reset_human(0);
    sv_damage_reset_object(target_car);
    CHECK(sv_damage_human(0, 0, 100, SV_DAMAGE_BULLET) == 100 && sv_damage_object(target_car, 40) == 40);
}

/* A damage decision from another thread must not reach the plugin. */
static DWORD WINAPI damage_other_thread(void *argument)
{
    (void)argument;
    return sv_damage_human(0, -1, 77, SV_DAMAGE_BULLET) == 77 ? 0 : 1;
}

/* The plugin decides damage inside the simulation: it cancels 77, changes 55 to 11, and the host ignores a new amount that
 * is below 0 or above the cap. The amount of a vehicle hit carries the shooter. The plugin runs the write guard on 88. A call
 * with an amount of 0 or less asks nobody. The fixture checks the records and the guard in its next tick. */
static void decision_check(void)
{
    HANDLE thread;
    DWORD code;
    CHECK(sv_damage_human(0, -1, 77, SV_DAMAGE_BULLET) == 0);
    CHECK(sv_damage_human(0, -1, 55, SV_DAMAGE_BULLET) == 11);
    CHECK(sv_damage_human(0, -1, 66, SV_DAMAGE_BULLET) == 66);
    CHECK(sv_damage_human(0, -1, 67, SV_DAMAGE_BULLET) == 67);
    CHECK(sv_damage_object(target_car, 55) == 11 && sv_damage_object(target_car, 77) == 0);
    CHECK(sv_damage_human(rider_b, 0, 5, SV_DAMAGE_VEHICLE) == 5);
    CHECK(sv_damage_human(0, -1, 88, SV_DAMAGE_BULLET) == 88);
    CHECK(sv_damage_human(0, -1, 0, SV_DAMAGE_BULLET) == 0 && sv_damage_human(0, -1, -77, SV_DAMAGE_BULLET) == -77);
    CHECK(sv_plugins_decide_damage(NULL) == 0);
    thread = CreateThread(NULL, 0, damage_other_thread, NULL, 0, NULL);
    CHECK(thread != NULL && WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0 && GetExitCodeThread(thread, &code) && code == 0);
    CloseHandle(thread);
}

/* A new body and a new object start at the scale 1, even when the slot had another value. */
static void damage_reset_check(void)
{
    float pos[3] = { 800.0f, 40.0f, 800.0f }, rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    int human = createhuman(pos, rot, -1), again, object, object_again;
    CHECK(human >= 0);
    *sv_damage_field_human_taken(human) = 0.0f;
    *sv_damage_field_human_dealt(human) = 3.0f;
    deletehuman((uint32_t)human);
    again = createhuman(pos, rot, -1);
    CHECK(again == human && *sv_damage_field_human_taken(again) == 1.0f && *sv_damage_field_human_dealt(again) == 1.0f);
    deletehuman((uint32_t)again);
    object = createobject(0, &position, NULL, rotation, 0);
    CHECK(object >= 0);
    *sv_damage_field_object_taken(object) = 0.0f;
    sr_objects[object].time_to_live = 0;
    objecttimetolive();
    object_again = createobject(0, &position, NULL, rotation, 0);
    CHECK(object_again == object && *sv_damage_field_object_taken(object_again) == 1.0f);
    sr_objects[object_again].time_to_live = 0;
    objecttimetolive();
}

/* The fixture calls the write API in this tick. The game state shows what the calls did. */
static void write_api_tick(void)
{
    Human *hu = &sr_humans[0];
    float offset[3];
    int train_piece, made_object, k2;
    vec3 bone_before;
    uint32_t events;
    int k;
    sr_connections[0].player_id = 0;
    sr_connections[0].ip = 0x01020304u;
    sr_connections[0].idle_ticks = 0;
    sr_connections[1].player_id = 1;
    sr_connections[1].is_admin = 0;
    sr_numofconnections = 2;
    sr_numofkicked = 0;
    /* A bot with a reference. The fixture needs it to check that a bot cannot be an admin. */
    sr_players[1].active = 1;
    sr_players[1].is_bot = 1;
    sv_entity_created(SV_ENTITY_PLAYER, 1);
    offset[0] = hu->bones[14].pos.x - hu->bones[0].pos.x;
    offset[1] = hu->bones[14].pos.y - hu->bones[0].pos.y;
    offset[2] = hu->bones[14].pos.z - hu->bones[0].pos.z;
    /* Item 0 left over from the lifetime test carries a link that its human does not hold. */
    sr_items[0].parent_human = -1;
    sr_itemtypes[0].inv_inertia = sr_itemtypes[1].inv_inertia = sr_itemtypes[12].inv_inertia = (vec3){1, 1, 1};
    sr_itemtypes[1].fits_in[0] = 1;
    /* A train piece. Only the track system makes these, so the write API must refuse to remove it. */
    train_piece = createobject(12, &position, NULL, rotation, 0);
    CHECK(train_piece > 0);
    sr_data_soundlist.num_sounds = 47;
    bone_before = hu->bones[3].vel;
    post_events();
    make_target();
    seat_riders();
    events = sr_numofevents;
    sr_server = 1;
    sv_plugins_tick();
    sr_server = 0;
    CHECK(!sv_plugins_failed());
    CHECK(sr_numofevents >= events + 6);
    CHECK(sr_events[events + 2].type == 7 && sr_events[events + 3].type == 7 && sr_events[events + 4].type == 17);
    CHECK(sr_events[events + 5].type == 2 && !strcmp(sr_events[events + 5].message, "private"));
    /* The private message is for player 0 only. Connection 1 belongs to the bot. */
    CHECK(!event_hidden_from((int)events, 0) && !event_hidden_from((int)events, 1));
    CHECK(!event_hidden_from((int)events + 5, 0) && event_hidden_from((int)events + 5, 1));
    sr_events[events + 5].tick++;
    CHECK(!event_hidden_from((int)events + 5, 1));
    sr_events[events + 5].tick--;
    /* The packet writer sends a filler of 6 bits to the other connection, and the whole event to the target. */
    CHECK(event_bits((int)events + 5, 1) == 6);
    CHECK(event_bits((int)events + 5, 0) > 6 + 28 + 6 && event_bits((int)events, 0) == event_bits((int)events, 1));
    riders_check();
    effects_check(events, bone_before);
    headline_check(events);
    damage_check();
    decision_check();
    CHECK(sr_players[0].is_admin == 1 && sr_connections[0].is_admin == 1 && sr_connections[1].is_admin == 0);
    CHECK(sr_players[0].team == 3 && sr_humans[0].team == 3);
    CHECK(sr_players[0].look_b == 5 && sr_humans[0].look_b == 5 && sr_humans[0].look_e == 1);
    CHECK(sr_events[events].type == 2 && sr_events[events].attach_type == SV_MESSAGE_SERVER &&
          sr_events[events].attach_id == -1 && strlen(sr_events[events].message) == 63);
    CHECK(sr_events[events + 1].attach_type == SV_MESSAGE_ADMIN && !strcmp(sr_events[events + 1].message, "admin line"));
    CHECK(sr_accounts[0].money == 1500 && sr_players[0].money == 4742 && sr_players[0].godmode == 1);
    CHECK(hu->bleeding == 1 && hu->stun_timer == 120);
    CHECK(hu->health == 80 && hu->pos.x == 100.0f && hu->pos.y == 50.0f && hu->pos.z == 200.0f);
    for (k = 0; k < 15; ++k)
        CHECK(hu->bones[k].pos_prev.x == hu->bones[k].pos.x && hu->bones[k].pos_prev.z == hu->bones[k].pos.z);
    CHECK(near_value(hu->bones[14].pos.x - hu->bones[0].pos.x, offset[0]) &&
          near_value(hu->bones[14].pos.y - hu->bones[0].pos.y, offset[1]) &&
          near_value(hu->bones[14].pos.z - hu->bones[0].pos.z, offset[2]));
    CHECK(sr_numofkicked == 1 && sr_kickedip[0] == 0x01020304u && sr_connections[0].idle_ticks == 0x4b0);
    /* The gun sits in a hand and the bandage in a pocket until the game removes it. */
    check_item_links();
    CHECK(hu->inventory[0].count == 1 && hu->inventory[3].count == 1);
    itemtimetolive();
    /* The fixture made one object and asked the game to remove it. The train piece stays. */
    made_object = -1;
    for (k2 = 0; k2 < 1024; ++k2)
        if (sr_objects[k2].active && sr_objects[k2].type == 0 && sr_objects[k2].color == 3)
            made_object = k2;
    CHECK(made_object > 0 && sr_objects[made_object].time_to_live == 0);
    objecttimetolive();
    CHECK(!sr_objects[made_object].active && sr_objects[train_piece].active);
    check_item_links();
    CHECK(hu->inventory[0].count == 1 && hu->inventory[3].count == 0);
    sr_numofconnections = 0;
    sr_players[0].is_admin = 0;
    sv_entity_deleted(SV_ENTITY_PLAYER, 1);
    sr_players[1].active = 0;
    sr_players[1].is_bot = 0;
    /* The fixture made a bot and asked for its removal. The world logic pass removes it. */
    npc_check();
}

/* Plugin commands in the chat. Only the PUBLIC command runs for a player. The ADMIN command runs
 * after the /admin flag is set. The LOCAL command never runs for a player. */
static void chat_commands(void)
{
    char text[64];
    sr_players[0].is_admin = 0;
    strcpy(text, "hello");
    CHECK(server_playermessage(0, text) == 0);
    strcpy(text, "/echo hi there");
    CHECK(server_playermessage(0, text) == 1);
    strcpy(text, "/secret one");
    CHECK(server_playermessage(0, text) == 1);
    strcpy(text, "/localonly one");
    CHECK(server_playermessage(0, text) == 1);
    strcpy(text, "/nothing");
    CHECK(server_playermessage(0, text) == 1);
    /* Player 5 has no reference. Its actor would be zero, the local operator, so the chat must not route it. */
    CHECK(sv_entity_at(SV_ENTITY_PLAYER, 5) == 0);
    strcpy(text, "/echo nope");
    CHECK(server_playermessage(5, text) == 1);
    CHECK(sv_game_actor_admin(sv_entity_at(SV_ENTITY_PLAYER, 0)) == 0);
    sr_players[0].is_admin = 1;
    CHECK(sv_game_actor_admin(sv_entity_at(SV_ENTITY_PLAYER, 0)) == 1);
    CHECK(sv_game_actor_admin(0) == 0);
    strcpy(text, "/secret two");
    CHECK(server_playermessage(0, text) == 1);
    strcpy(text, "/localonly two");
    CHECK(server_playermessage(0, text) == 1);
    sr_players[0].is_admin = 0;
    /* A chat text is a decision. The plugin drops a text with the word deny. A command is no chat text. A player with no
     * reference gives no event. A text of 63 bytes arrives whole. */
    strcpy(text, "deny me");
    CHECK(server_playermessage(0, text) == 1);
    strcpy(text, "deny me");
    CHECK(server_playermessage(5, text) == 0);
    strcpy(text, "/deny me");
    CHECK(server_playermessage(0, text) == 1);
    memset(text, 'a', 63);
    text[63] = 0;
    CHECK(server_playermessage(0, text) == 0);
    sr_players[0].chat_block = 1;
    strcpy(text, "deny me");
    CHECK(server_playermessage(0, text) == 1);
    sr_players[0].chat_block = 0;
}

/* A decision from another thread must not reach a plugin. */
static DWORD WINAPI decide_other_thread(void *argument)
{
    SvChatEvent event;
    (void)argument;
    memset(&event, 0, sizeof event);
    event.base.size = sizeof event;
    event.base.kind = SV_EVENT_CHAT;
    event.player = sv_entity_at(SV_ENTITY_PLAYER, 0);
    strcpy(event.text, "deny me");
    return sv_plugins_decide(&event.base) == SV_OK && !sv_game_chat_denied(0, "deny me") ? 0 : 1;
}

/* The join decision: the plugin drops a player named BANNED. The test calls the function that the join code calls.
 * A bad index does nothing. */
/* Queues one menu action for player 0 and runs the action pass of the game. */
static void menu_action(int menu, int id, int value)
{
    Player *p = &sr_players[0];
    PlayerAction *a;
    p->action_tail = p->num_actions;
    a = &p->actions[p->num_actions];
    p->menu = menu;
    a->kind = 0;
    a->arg_a = id;
    a->arg_b = value;
    p->num_actions = (p->num_actions + 1) & 63;
    logic_playeractions(0);
}

/* The equipment menu (7) and the stock menu (8) return to menu 3 with value 16, as the other menus do. */
static void menu_return_check(void)
{
    CHECK(sr_players[0].human_id != -1 && sr_players[0].team != -1);
    menu_action(7, 7, 16);
    CHECK(sr_players[0].menu == 3);
    menu_action(8, 8, 16);
    CHECK(sr_players[0].menu == 3);
    menu_action(4, 4, 16);
    CHECK(sr_players[0].menu == 3);
    /* Another value, or an action for another menu, keeps the menu. */
    menu_action(7, 7, 3);
    CHECK(sr_players[0].menu == 7);
    menu_action(7, 8, 16);
    CHECK(sr_players[0].menu == 7);
    sr_players[0].menu = 0;
}

static void join_decision(void)
{
    int banned = createplayer(), fine = createplayer();
    char big[100];
    HANDLE thread;
    DWORD code;
    CHECK(banned >= 0 && fine >= 0);
    strcpy(sr_players[banned].name, "BANNED");
    strcpy(sr_players[fine].name, "Fine");
    CHECK(sv_game_join_denied(banned) == 1 && sv_game_join_denied(fine) == 0);
    CHECK(sv_game_join_denied(-1) == 0 && sv_game_join_denied(SR_MAX_PLAYER) == 0);
    CHECK(sv_game_chat_denied(-1, "deny") == 0 && sv_game_chat_denied(SR_MAX_PLAYER, "deny") == 0 && sv_game_chat_denied(0, NULL) == 0);
    CHECK(sv_plugins_decide(NULL) == SV_OK);
    /* A text longer than the field arrives cut to 63 bytes. */
    memset(big, 'b', sizeof big - 1);
    big[sizeof big - 1] = 0;
    CHECK(sv_game_chat_denied(0, big) == 0);
    thread = CreateThread(NULL, 0, decide_other_thread, NULL, 0, NULL);
    CHECK(thread != NULL && WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0 && GetExitCodeThread(thread, &code) && code == 0);
    CloseHandle(thread);
    deleteplayer(banned);
    deleteplayer(fine);
    sv_events_clear();
}

/* With too few bonds for a human, npc_create must fail with no player left behind. */
static void npc_limit(void)
{
    int bonds = sr_numofbonds, i, active_before = 0, active_after = 0;
    for (i = 0; i < 256; ++i)
        active_before += sr_players[i].active != 0;
    sr_numofbonds = SR_MAX_BOND - 10;
    CHECK(sv_plugins_command(0, "npclimit"));
    for (i = 0; i < 256; ++i)
        active_after += sr_players[i].active != 0;
    CHECK(sr_numofbonds == SR_MAX_BOND - 10 && active_before == active_after);
    sr_numofbonds = bonds;
}

static void *test_world_field(int32_t index, uint32_t slot)
{
    (void)index;
    (void)slot;
    return &test_world_value;
}

static const Prop test_world[] = {
    { "test.world", SV_ENTITY_WORLD, SV_PROP_INT, 1, SV_PROP_GET | SV_PROP_SET | SV_PROP_ADD, 0, 10, test_world_field, NULL, NULL, NULL },
};

/* A plugin area adds its own rows. The table must refuse bad rows and repeated names, and the same group twice is no error. */
static void props_groups(void)
{
    static const Prop repeated[] = { { "human.health", 1, SV_PROP_INT, 1, SV_PROP_GET, 0, 1, test_world_field, NULL, NULL, NULL } };
    static const Prop no_field[] = { { "test.nofield", SV_ENTITY_WORLD, SV_PROP_INT, 1, SV_PROP_GET, 0, 1, NULL, NULL, NULL, NULL } };
    static const Prop no_ops[] = { { "test.noops", SV_ENTITY_WORLD, SV_PROP_INT, 1, 0, 0, 1, test_world_field, NULL, NULL, NULL } };
    static const Prop vector_set[] = { { "test.vecset", SV_ENTITY_WORLD, SV_PROP_VECTOR, 1, SV_PROP_SET, 0, 1, test_world_field, NULL, NULL, NULL } };
    static const Prop vector_add[] = { { "test.vecadd", SV_ENTITY_WORLD, SV_PROP_VECTOR, 1, SV_PROP_ADD, 0, 1, test_world_field, NULL, NULL, NULL } };
    static const Prop long_name[] = { { "0123456789012345678901234567890123456789", SV_ENTITY_WORLD, SV_PROP_INT, 1, SV_PROP_GET, 0, 1, test_world_field, NULL, NULL, NULL } };
    static const Prop twice[] = { { "test.twice", SV_ENTITY_WORLD, SV_PROP_INT, 1, SV_PROP_GET, 0, 1, test_world_field, NULL, NULL, NULL },
                                  { "test.twice", SV_ENTITY_WORLD, SV_PROP_INT, 1, SV_PROP_GET, 0, 1, test_world_field, NULL, NULL, NULL } };
    CHECK(sv_props_add_group(NULL, 1) == SV_INVALID && sv_props_add_group(test_world, 0) == SV_INVALID);
    CHECK(sv_props_add_group(repeated, 1) == SV_INVALID && sv_props_add_group(no_field, 1) == SV_INVALID);
    CHECK(sv_props_add_group(no_ops, 1) == SV_INVALID && sv_props_add_group(vector_set, 1) == SV_INVALID);
    CHECK(sv_props_add_group(vector_add, 1) == SV_INVALID && sv_props_add_group(long_name, 1) == SV_INVALID);
    CHECK(sv_props_add_group(twice, 2) == SV_INVALID);
    CHECK(sv_props_add_group(test_world, 1) == SV_OK && sv_props_add_group(test_world, 1) == SV_OK);
}

/* A line query uses the shared hit record of the game. The record must keep its old content after the query. */
static void line_mark(void)
{
    const Intersect before = sr_data_intersect;
    sr_data_intersect.fraction = 0.25f;
    sr_data_intersect.pos[0] = 7.0f;
    sr_data_intersect.index = 55;
    sr_data_intersect.tri_particles[0] = 9;
    CHECK(sv_plugins_command(0, "linemark"));
    CHECK(sr_data_intersect.fraction == 0.25f && sr_data_intersect.pos[0] == 7.0f && sr_data_intersect.index == 55 &&
          sr_data_intersect.tri_particles[0] == 9);
    sr_data_intersect = before;
}

int main(void)
{
    SvEntity old;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    x87_init();
    CHECK(sv_entity_capacity(0) == 0);
    CHECK(sv_entity_capacity(UINT32_MAX) == 0);
    CHECK(sv_entity_at(0, 0) == 0);
    CHECK(sv_plugins_start());
    player_lifetime();
    body_lifetime();
    item_lifetime();
    object_lifetime();
    account_lifetime();
    core_events();
    damage_reset_check();
    props_groups();
    sr_players[0].money = 4242;
    memset(sr_players[0].name, 'P', sizeof sr_players[0].name);
    sr_players[0].account = 0;
    sr_players[0].human_id = 0;
    sr_humans[0].player_id = 0;
    sr_humans[0].account = 0;
    sr_humans[0].vehicle_id = 0;
    sr_humans[0].health = 37;
    sr_humans[0].inventory[0].count = 1;
    sr_humans[0].inventory[0].items[0] = 0;
    sr_items[0].parent_human = 0;
    sr_items[0].parent_slot = 0;
    sr_data_object[0].driver = 0;
    sr_data_object[0].num_seats = 1;
    sr_data_server.max_players = 128;
    sr_data_game_world.clock = 123456;
    sr_data_newspaper.num_lines = 3;
    strcpy(sr_data_newspaper.lines[0], "first");
    strcpy(sr_data_newspaper.lines[1], "second");
    strcpy(sr_data_newspaper.lines[2], "third");
    CHECK(!sv_game_is_ready());
    sv_game_ready();
    sv_plugins_tick();
    CHECK(!sv_plugins_failed());
    sr_humans[0].inventory[0].count = 9;
    sr_humans[0].vehicle_id = -1;
    sr_items[0].num_children = -1;
    sr_data_object[0].num_wheels = 33;
    sv_plugins_tick();
    CHECK(!sv_plugins_failed());
    sr_humans[0].inventory[0].count = 0;
    sr_items[0].num_children = 0;
    sr_data_object[0].num_wheels = 0;
    write_api_tick();
    chat_commands();
    menu_return_check();
    join_decision();
    npc_limit();
    line_mark();
    sv_game_reset();
    sv_entities_clear(SV_ENTITY_HUMAN);
    sv_entities_clear(SV_ENTITY_ITEM);
    sv_entities_clear(SV_ENTITY_OBJECT);
    sv_entities_clear(SV_ENTITY_ACCOUNT);
    sv_entity_deleted(SV_ENTITY_PLAYER, 0);
    sv_entity_created(SV_ENTITY_PLAYER, 0);
    post_flood();
    CHECK(sv_game_read.world(1, &(SvWorldState){ sizeof(SvWorldState) }) == SV_STATE);
    sv_plugins_tick();
    CHECK(!sv_plugins_failed());
    old = sv_entity_at(SV_ENTITY_PLAYER, 0);
    sv_plugins_stop();
    CHECK(sv_entity_resolve(SV_ENTITY_PLAYER, old) == -1);
    /* After the stop no plugin wants a record, and the queue is empty. */
    sv_events_post(SV_EVENT_DEATH, 1, 0, 0, 0);
    CHECK(sv_events_pending() == 0 && !sv_plugins_wants(SV_EVENT_DEATH));
    CHECK(sv_game_start());
    sv_entity_created(SV_ENTITY_PLAYER, 0);
    CHECK(sv_entity_at(SV_ENTITY_PLAYER, 0) != old);
    CHECK(sv_entity_resolve(SV_ENTITY_PLAYER, old) == -1);
    sv_game_stop();
    test_physics_bounds();
    puts("Game entity lifetimes passed against real constructors and retirement paths.");
    return 0;
}
