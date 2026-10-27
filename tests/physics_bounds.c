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
    puts("Physics capacity checks passed: contacts, body reservations, held items, player list and spawn rollback.");
}
