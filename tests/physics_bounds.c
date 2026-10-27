#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bonds.h"
#include "humans.h"
#include "items.h"
#include "logic.h"
#include "objects.h"
#include "player.h"
#include "scenario.h"
#include "sr_os.h"

void item_physicssimulation(int item);

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); exit(1); \
} } while (0)

static void contact_limits(void)
{
    static const int counts[] = { INT_MIN, -1, SR_MAX_BOND, SR_MAX_BOND + 1, INT_MAX };
    const float normal[3] = { 0, 1, 0 };
    Bond before;
    size_t i;
    memset(&sr_bonds[SR_MAX_BOND - 1], 0xa5, sizeof(Bond));
    before = sr_bonds[SR_MAX_BOND - 1];
    for (i = 0; i < sizeof counts / sizeof counts[0]; ++i) {
        sr_numofbonds = counts[i];
        CHECK(!bonds_available(1));
        CHECK(contact_bond(1, 7, normal, 0.5f, 0.125f, 0.25f) == NULL);
        CHECK(sr_numofbonds == counts[i]);
        CHECK(memcmp(&before, &sr_bonds[SR_MAX_BOND - 1], sizeof before) == 0);
    }
    sr_numofbonds = 0;
    CHECK(bonds_available(0) && bonds_available(SR_MAX_BOND));
    CHECK(!bonds_available(-1) && !bonds_available(INT_MAX));
    sr_numofbonds = SR_MAX_BOND - 1;
    CHECK(contact_bond(1, 7, normal, 0.5f, 0.125f, 0.25f) == &sr_bonds[SR_MAX_BOND - 1]);
    CHECK(sr_numofbonds == SR_MAX_BOND);
    CHECK(sr_bonds[SR_MAX_BOND - 1].rest_length == 0.25f);
    CHECK(sr_bonds[SR_MAX_BOND - 1].particle_a == 7);
    before = sr_bonds[SR_MAX_BOND - 1];
    CHECK(contact_bond(1, 8, normal, 0.1f, 0, 0) == NULL);
    CHECK(memcmp(&before, &sr_bonds[SR_MAX_BOND - 1], sizeof before) == 0);
}

static void body_limits(void)
{
    float position[3] = { 0, 0, 0 };
    float rotation[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    Human before;
    uint32_t events = sr_numofevents;
    int next_random;
    memset(sr_humans, 0, sizeof sr_data_human);
    sr_numofbonds = SR_MAX_BOND - 27;
    os_srand(12345);
    next_random = os_rand();
    os_srand(12345);
    CHECK(createhuman(position, rotation, -1) == -1);
    CHECK(os_rand() == next_random);
    CHECK(!sr_humans[0].active && sr_numofevents == events && sr_numofbonds == SR_MAX_BOND - 27);
    sr_numofbonds = SR_MAX_BOND - 28;
    CHECK(createhuman(position, rotation, -1) == 0);
    CHECK(sr_humans[0].active && sr_humans[0].num_joints == 14 && sr_humans[0].num_muscles == 14);
    CHECK(sr_numofbonds == SR_MAX_BOND);
    CHECK(createhuman(position, rotation, -1) == -1 && !sr_humans[1].active);
    before = sr_humans[1];
    CHECK(createjointbond(1, 0, 1, (const vec3 *)position) == -1);
    CHECK(memcmp(&before, &sr_humans[1], sizeof before) == 0);
    sr_numofbonds = SR_MAX_BOND - 1;
    human_createmuscleball(1, 0, 1, (const vec3 *)position);
    CHECK(sr_numofbonds == SR_MAX_BOND - 1);
    CHECK(memcmp(&before, &sr_humans[1], sizeof before) == 0);
    sr_numofbonds = SR_MAX_BOND - 2;
    human_createmuscleball(1, 0, 1, (const vec3 *)position);
    CHECK(sr_numofbonds == SR_MAX_BOND && sr_humans[1].num_joints == 1 && sr_humans[1].num_muscles == 1);
}

static void body_list_limits(void)
{
    const vec3 position = { 0, 0, 0 };
    Human before;
    memset(sr_humans, 0, sizeof sr_data_human);
    sr_numofbonds = 0;
    sr_humans[2].num_joints = 32;
    sr_humans[2].num_muscles = 128;
    before = sr_humans[2];
    CHECK(createjointbond(2, 0, 1, &position) == -1);
    human_createmuscleball(2, 0, 1, &position);
    CHECK(sr_numofbonds == 0 && memcmp(&before, &sr_humans[2], sizeof before) == 0);
    sr_humans[2].num_joints = 31;
    CHECK(createjointbond(2, 0, 1, &position) >= 0);
    CHECK(sr_humans[2].num_joints == 32 && sr_numofbonds == 1);
}

static void held_item_limits(void)
{
    const vec3 position = { 0, 0, 0 };
    const vec3 rotation[3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
    Human *human = &sr_humans[0];
    Bond before;
    uint32_t door;
    memset(sr_items, 0, sizeof sr_data_item);
    sr_itemtypes[ITEM_DOOR].mass = 1;
    sr_itemtypes[ITEM_DOOR].inv_inertia = (vec3){1, 1, 1};
    door = createitem(ITEM_DOOR, &position, NULL, rotation);
    CHECK(door == 0);
    human->inventory[0].count = 1;
    human->inventory[0].items[0] = (int)door;
    human->inventory[1].count = 0;
    sr_numofbonds = SR_MAX_BOND;
    before = sr_bonds[SR_MAX_BOND - 1];
    human_calculatearmangles(0);
    CHECK(sr_numofbonds == SR_MAX_BOND);
    CHECK(memcmp(&before, &sr_bonds[SR_MAX_BOND - 1], sizeof before) == 0);
    human->inventory[0].count = 0;
    human->inventory[1].count = 1;
    human->inventory[1].items[0] = (int)door;
    human_calculatearmangles(0);
    CHECK(sr_numofbonds == SR_MAX_BOND);
    CHECK(memcmp(&before, &sr_bonds[SR_MAX_BOND - 1], sizeof before) == 0);
    sr_items[door].type = ITEM_9MM;
    sr_itemtypes[ITEM_9MM].is_gun = 1;
    sr_itemtypes[ITEM_9MM].one_handed = 1;
    sr_numofbonds = SR_MAX_BOND - 1;
    human_calculatearmangles(0);
    CHECK(sr_numofbonds == SR_MAX_BOND - 1);
    CHECK(memcmp(&before, &sr_bonds[SR_MAX_BOND - 1], sizeof before) == 0);
    human->inventory[1].count = 0;
    human->inventory[0].count = 1;
    human_calculatearmangles(0);
    CHECK(sr_numofbonds == SR_MAX_BOND - 1);
    sr_items[door].type = ITEM_DOOR;
    human_calculatearmangles(0);
    CHECK(sr_numofbonds == SR_MAX_BOND && sr_bonds[SR_MAX_BOND - 1].type == 10);
    before = sr_bonds[SR_MAX_BOND - 1];
    sr_humans[1].active = 1;
    sr_humans[1].vehicle_id = -1;
    sr_humans[1].bones[0].pos.x = 0.1f;
    human_collisionhuman(0);
    item_physicssimulation((int)door);
    CHECK(sr_numofbonds == SR_MAX_BOND);
    CHECK(memcmp(&before, &sr_bonds[SR_MAX_BOND - 1], sizeof before) == 0);
}

static void reset_actors(void)
{
    memset(sr_humans, 0, sizeof sr_data_human);
    memset(sr_players, 0, sizeof sr_data_player);
    memset(sr_objects, 0, sizeof sr_data_object);
    sr_numofbonds = 0;
    g_city.num_roads = 1;
    g_city.num_guard_spots = 1;
    g_scenario.items[0] = 7;
}

static void fill_humans(void)
{
    int i;
    for (i = 0; i < SR_MAX_HUMAN; ++i)
        sr_humans[i].active = 1;
}

static void fill_objects(void)
{
    int i;
    for (i = 0; i < 1024; ++i)
        sr_objects[i].active = 1;
}

/* A failed spawn must leave no player, no human and no car behind. */
static void expect_clean(int bonds)
{
    int i;
    for (i = 0; i < 256; ++i)
        CHECK(!sr_players[i].active);
    CHECK(sr_numofbonds == bonds);
}

static void player_list_limits(void)
{
    int i, teams[1] = { 0 }, next_id;
    reset_actors();
    for (i = 0; i < 255; ++i)
        sr_players[i].active = 1;
    CHECK(createplayer() == 255);
    CHECK(createplayer() == -1);
    sr_numofbonds = 5;
    scenario_createtraffic2(3);
    CHECK(!sr_objects[0].active && !sr_humans[0].active && sr_numofbonds == 5);
    CHECK(scenario_createchase(21, 100) == -1 && g_scenario.items[0] == -1);
    g_scenario.items[0] = 7;
    CHECK(scenario_createguards(0, 21, 100) == -1 && g_scenario.items[0] == -1);
    CHECK(!sr_objects[0].active && !sr_humans[0].active && sr_numofbonds == 5);
    next_id = g_scenario.next_id;
    CHECK(createmission_acquisition(1, 1, teams) == 0 && g_scenario.next_id == next_id);
}

static void spawn_rollback(void)
{
    int bonds = SR_MAX_BOND - 27;
    reset_actors();
    fill_humans();
    scenario_createtraffic2(1);
    expect_clean(0);
    CHECK(sr_objects[0].active && sr_objects[0].time_to_live == 0);
    reset_actors();
    sr_numofbonds = bonds;
    scenario_createtraffic2(1);
    expect_clean(bonds);
    CHECK(sr_objects[0].active && sr_objects[0].time_to_live == 0 && !sr_humans[0].active);
    reset_actors();
    sr_numofbonds = bonds;
    CHECK(scenario_createchase(21, 100) == -1 && g_scenario.items[0] == -1);
    expect_clean(bonds);
    CHECK(sr_objects[0].active && sr_objects[0].time_to_live == 0 && !sr_humans[0].active);
    reset_actors();
    sr_numofbonds = bonds;
    CHECK(scenario_createguards(0, 21, 100) == -1 && g_scenario.items[0] == -1);
    expect_clean(bonds);
    CHECK(!sr_humans[0].active);
    reset_actors();
    fill_objects();
    scenario_createtraffic2(1);
    expect_clean(0);
    CHECK(!sr_humans[0].active);
    CHECK(scenario_createchase(21, 100) == -1 && g_scenario.items[0] == -1);
    expect_clean(0);
    CHECK(!sr_humans[0].active);
    g_city.num_roads = 0;
    g_scenario.items[0] = 7;
    CHECK(scenario_createchase(21, 100) == -1 && g_scenario.items[0] == -1);
    expect_clean(0);
}

/* A flat landscape at height 0 gives every test a level to touch. */
static void flat_level(void)
{
    memset(&sr_data_landscape, 0, sizeof sr_data_landscape);
    sr_data_intersect.block[0] = -1;
}

/* Runs fill() with room for "spare" bonds more than it adds, then with room for one bond fewer
 * than it needs. The second run must stop at the limit and leave the last bond whole. */
static int bonds_made(void (*fill)(int spare), int spare)
{
    int start = SR_MAX_BOND - spare;
    fill(spare);
    return sr_numofbonds - start;
}

static void expect_limited(void (*fill)(int spare), const char *what)
{
    int made = bonds_made(fill, 1000);
    if (made < 1) {
        fprintf(stderr, "%s: the test setup makes no contact\n", what);
        exit(1);
    }
    CHECK(bonds_made(fill, 0) == 0 && sr_numofbonds == SR_MAX_BOND);
    CHECK(bonds_made(fill, made - 1) == made - 1 && sr_numofbonds == SR_MAX_BOND);
}

static void set_axes(vec3 *rot)
{
    rot[0] = (vec3){1, 0, 0};
    rot[1] = (vec3){0, 1, 0};
    rot[2] = (vec3){0, 0, 1};
}

static void wheel_fill(int spare)
{
    Object *o;
    reset_actors();
    flat_level();
    o = &sr_objects[0];
    o->active = 1;
    set_axes(o->rot);
    o->particles[0] = 0;
    o->particles[1] = 1;
    o->num_wheels = 1;
    o->wheels[0].particle_a = 0;
    o->wheels[0].particle_b = 1;
    o->wheels[0].weight_a = 0.5f;
    o->wheels[0].weight_b = 0.5f;
    o->wheels[0].radius = 0.5f;
    o->wheels[0].pos = (vec3){10, 0.2f, 10};
    sr_numofbonds = SR_MAX_BOND - spare;
    objectwheelcollision(0, 0);
}

static void wheel_body_fill(int spare)
{
    Object *o;
    reset_actors();
    flat_level();
    o = &sr_objects[0];
    o->active = 1;
    o->num_wheel_bodies = 1;
    o->wheel_bodies[0].pos = (vec3){10, 0.2f, 10};
    set_axes(o->wheel_bodies[0].rot);
    o->wheel_bodies[0].radius = 0.5f;
    o->wheel_bodies[0].half_width = 0.3f;
    sr_numofbonds = SR_MAX_BOND - spare;
    objectwheelcollision2(0, 0);
}

static void wheel_limits(void)
{
    expect_limited(wheel_fill, "wheel");
    expect_limited(wheel_body_fill, "wheel body");
}

/* A dead human has no stance code, so the tick only makes the contact bonds. */
static void human_fill(float y, int spare, int with_object)
{
    float position[3] = { 10, y, 10 };
    float rotation[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    Object *o = &sr_objects[0];
    float top;
    int k;
    reset_actors();
    flat_level();
    sr_numofbonds = SR_MAX_BOND - 28 - spare;
    CHECK(createhuman(position, rotation, -1) == 0);
    sr_humans[0].health = 0;
    if (with_object) {
        top = sr_humans[0].bones[0].pos.y - 0.1f;
        o->active = 1;
        o->box_min = (vec3){-41, top - 5, -41};
        o->box_max = (vec3){61, top + 5, 61};
        o->num_faces = 1;
        o->faces[0].corners = 4;
        o->faces[0].health = 1;
        for (k = 0; k < 4; ++k) {
            o->faces[0].verts[k] = k;
            o->verts[k].particle = k;
            o->particles[k] = k;
        }
        o->verts[0].pos = (vec3){-40, top, -40};
        o->verts[1].pos = (vec3){-40, top, 60};
        o->verts[2].pos = (vec3){60, top, 60};
        o->verts[3].pos = (vec3){60, top, -40};
    }
    humansimulation();
}

static void human_level_fill(int spare)
{
    human_fill(0.05f, spare, 0);
}

static void human_object_fill(int spare)
{
    human_fill(5.9f, spare, 1);
}

static void human_contact_limits(void)
{
    expect_limited(human_level_fill, "human on the level");
    expect_limited(human_object_fill, "human on an object");
}

/* Sets the frame of an object from its first three particles. */
static void set_frame(Object *o)
{
    o->num_frame_particles = 3;
    o->frame_weight_x[0] = -0.5f;
    o->frame_weight_x[1] = 0.5f;
    o->frame_weight_z[0] = -0.5f;
    o->frame_weight_z[2] = 0.5f;
}

static void set_particle(int index, float x, float y, float z)
{
    memset(&sr_particles[index], 0, sizeof sr_particles[index]);
    sr_particles[index].pos = (vec3){x, y, z};
}

/* Three particles under the ground. pos_prev is above it, so every particle crosses it. */
static void object_prev_fill(int spare)
{
    Object *o;
    reset_actors();
    flat_level();
    o = &sr_objects[0];
    o->active = 1;
    o->num_particles = 3;
    o->pos_prev = (vec3){10, 5, 10};
    set_frame(o);
    set_particle(0, 10, -1, 10);
    set_particle(1, 11, -1, 10);
    set_particle(2, 10, -1, 11);
    sr_numofbonds = SR_MAX_BOND - spare;
    objectsimulation();
}

/* Two particles under the ground and one far above it. The center is above the ground. */
static void object_center_fill(int spare)
{
    Object *o;
    reset_actors();
    flat_level();
    o = &sr_objects[0];
    o->active = 1;
    o->num_particles = 3;
    o->pos_prev = (vec3){10, 5, 10};
    set_frame(o);
    set_particle(0, 10, -1, 10);
    set_particle(1, 11, -1, 10);
    set_particle(2, 10, 20, 10);
    sr_numofbonds = SR_MAX_BOND - spare;
    objectsimulation();
}

/* Object 0 has two particles under a quad of object 1. Its center is above the quad. */
static void object_object_fill(int spare)
{
    Object *a, *b;
    int k;
    reset_actors();
    flat_level();
    a = &sr_objects[0];
    b = &sr_objects[1];
    a->active = b->active = 1;
    a->linked_object = b->linked_object = -1;
    a->num_particles = 3;
    a->pos_prev = (vec3){10, 24, 10};
    set_frame(a);
    set_particle(0, 10, 20, 10);
    set_particle(1, 11, 20, 10);
    set_particle(2, 10, 32, 10);
    b->num_particles = 4;
    b->pos_prev = (vec3){10, 23, 10};
    set_frame(b);
    for (k = 0; k < 4; ++k) {
        b->particles[k] = 10 + k;
        b->verts[k].particle = k;
        b->faces[0].verts[k] = k;
    }
    set_particle(10, -40, 23, -40);
    set_particle(11, -40, 23, 60);
    set_particle(12, 60, 23, 60);
    set_particle(13, 60, 23, -40);
    b->num_faces = 1;
    b->faces[0].corners = 4;
    b->faces[0].health = 1;
    a->particles[0] = 0;
    a->particles[1] = 1;
    a->particles[2] = 2;
    sr_numofbonds = SR_MAX_BOND - spare;
    objectsimulation();
}

static void object_limits(void)
{
    expect_limited(object_prev_fill, "object on the level (from the last place)");
    expect_limited(object_center_fill, "object on the level (from the center)");
    expect_limited(object_object_fill, "object on an object");
}

/* A chase makes a car, a leader and up to three crew. "taken" players sit in front of them. */
static void chase_fill(int spare, int taken)
{
    int i;
    reset_actors();
    memset(sr_items, 0, sizeof sr_data_item);
    for (i = 0; i < taken; ++i)
        sr_players[i].active = 1;
    os_srand(777);
    sr_numofbonds = SR_MAX_BOND - spare;
    scenario_createchase(21, 100);
}

/* Every player that the chase makes has a living human, however few bonds are left. */
static void expect_whole_crew(int taken)
{
    int i;
    CHECK(sr_numofbonds <= SR_MAX_BOND);
    for (i = taken; i < 256; ++i)
        if (sr_players[i].active)
            CHECK(sr_players[i].human_id >= 0 && sr_humans[sr_players[i].human_id].active);
}

static void chase_limits(void)
{
    static const int taken[] = { 0, 254 };
    size_t t;
    int spare, used;
    for (t = 0; t < sizeof taken / sizeof taken[0]; ++t) {
        chase_fill(SR_MAX_BOND, taken[t]);
        used = sr_numofbonds;
        CHECK(used > 28);
        for (spare = 0; spare <= used; ++spare) {
            chase_fill(spare, taken[t]);
            expect_whole_crew(taken[t]);
        }
    }
}

void test_physics_bounds(void)
{
    x87_init();
    setupbody();
    contact_limits();
    body_limits();
    held_item_limits();
    body_list_limits();
    player_list_limits();
    spawn_rollback();
    wheel_limits();
    human_contact_limits();
    object_limits();
    chase_limits();
    puts("Physics capacity checks passed: contacts, body reservations, held items, player list, spawn rollback, wheels, human and object contacts and chase crews.");
}
