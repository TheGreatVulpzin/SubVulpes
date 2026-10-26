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
#include "sr_server.h"
#include "plugin_game_internal.h"

_Static_assert(SV_HOST_V1_SIZE == 24, "Preserve the original host prefix");
_Static_assert(sizeof(SvStatus) == 20, "Preserve status ABI v1");
_Static_assert(sizeof(SvEvent) == 12, "Preserve event ABI v1");
_Static_assert(sizeof(SvPlugin) == 92, "Preserve plugin ABI v1");

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

int main(void)
{
    SvEntity old;
    CHECK(sv_entity_capacity(0) == 0);
    CHECK(sv_entity_capacity(UINT32_MAX) == 0);
    CHECK(sv_entity_at(0, 0) == 0);
    CHECK(sv_plugins_start());
    player_lifetime();
    body_lifetime();
    item_lifetime();
    object_lifetime();
    account_lifetime();
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
    sv_plugins_tick();
    CHECK(!sv_plugins_failed());
    sv_game_reset();
    sv_entities_clear(SV_ENTITY_HUMAN);
    sv_entities_clear(SV_ENTITY_ITEM);
    sv_entities_clear(SV_ENTITY_OBJECT);
    sv_entities_clear(SV_ENTITY_ACCOUNT);
    sv_entity_deleted(SV_ENTITY_PLAYER, 0);
    sv_entity_created(SV_ENTITY_PLAYER, 0);
    CHECK(sv_game_read.world(1, &(SvWorldState){ sizeof(SvWorldState) }) == SV_STATE);
    sv_plugins_tick();
    CHECK(!sv_plugins_failed());
    old = sv_entity_at(SV_ENTITY_PLAYER, 0);
    sv_plugins_stop();
    CHECK(sv_entity_resolve(SV_ENTITY_PLAYER, old) == -1);
    CHECK(sv_game_start());
    sv_entity_created(SV_ENTITY_PLAYER, 0);
    CHECK(sv_entity_at(SV_ENTITY_PLAYER, 0) != old);
    CHECK(sv_entity_resolve(SV_ENTITY_PLAYER, old) == -1);
    sv_game_stop();
    puts("Game entity lifetimes passed against real constructors and retirement paths.");
    return 0;
}
