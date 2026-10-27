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
#include "sr_server.h"
#include "plugin_game_internal.h"

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

/* The fixture calls the write API in this tick. The game state shows what the calls did. */
static void write_api_tick(void)
{
    Human *hu = &sr_humans[0];
    float offset[3];
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
    events = sr_numofevents;
    sr_server = 1;
    sv_plugins_tick();
    sr_server = 0;
    CHECK(!sv_plugins_failed());
    CHECK(sr_numofevents == events + 6);
    CHECK(sr_events[events + 2].type == 7 && sr_events[events + 3].type == 7 && sr_events[events + 4].type == 17);
    CHECK(sr_events[events + 5].type == 2 && !strcmp(sr_events[events + 5].message, "private"));
    /* The private message is for player 0 only. Connection 1 belongs to the bot. */
    CHECK(!event_hidden_from((int)events, 0) && !event_hidden_from((int)events, 1));
    CHECK(!event_hidden_from((int)events + 5, 0) && event_hidden_from((int)events + 5, 1));
    sr_events[events + 5].tick++;
    CHECK(!event_hidden_from((int)events + 5, 1));
    sr_events[events + 5].tick--;
    CHECK(sr_players[0].is_admin == 1 && sr_connections[0].is_admin == 1 && sr_connections[1].is_admin == 0);
    CHECK(sr_players[0].team == 3 && sr_humans[0].team == 3);
    CHECK(sr_players[0].look_b == 5 && sr_humans[0].look_b == 5 && sr_humans[0].look_e == 1);
    CHECK(sr_events[events].type == 2 && sr_events[events].attach_type == SV_MESSAGE_SERVER &&
          sr_events[events].attach_id == -1 && strlen(sr_events[events].message) == 63);
    CHECK(sr_events[events + 1].attach_type == SV_MESSAGE_ADMIN && !strcmp(sr_events[events + 1].message, "admin line"));
    CHECK(sr_accounts[0].money == 1500 && sr_players[0].money == 4742 && sr_players[0].godmode == 1);
    CHECK(hu->health == 80 && hu->pos.x == 100.0f && hu->pos.y == 50.0f && hu->pos.z == 200.0f);
    for (k = 0; k < 15; ++k)
        CHECK(hu->bones[k].pos_prev.x == hu->bones[k].pos.x && hu->bones[k].pos_prev.z == hu->bones[k].pos.z);
    CHECK(near_value(hu->bones[14].pos.x - hu->bones[0].pos.x, offset[0]) &&
          near_value(hu->bones[14].pos.y - hu->bones[0].pos.y, offset[1]) &&
          near_value(hu->bones[14].pos.z - hu->bones[0].pos.z, offset[2]));
    CHECK(sr_numofkicked == 1 && sr_kickedip[0] == 0x01020304u && sr_connections[0].idle_ticks == 0x4b0);
    sr_numofconnections = 0;
    sr_players[0].is_admin = 0;
    sv_entity_deleted(SV_ENTITY_PLAYER, 1);
    sr_players[1].active = 0;
    sr_players[1].is_bot = 0;
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
}

int main(void)
{
    SvEntity old;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
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
    write_api_tick();
    chat_commands();
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
    test_physics_bounds();
    puts("Game entity lifetimes passed against real constructors and retirement paths.");
    return 0;
}
