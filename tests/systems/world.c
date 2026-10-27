/* A small world for the system areas: real constructors, no map. */
#include "systems_host.h"

int world_player = -1, world_human = -1, world_item = -1, world_object = -1, world_account = -1;

void world_start(void)
{
    static const vec3 position = { 10.0f, 20.0f, 30.0f };
    static const vec3 rotation[3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
    float pos[3] = { 10.0f, 20.0f, 30.0f };
    float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    x87_init();
    CHECK(sv_plugins_start());
    world_player = createplayer();
    CHECK(world_player >= 0);
    strcpy(sr_players[world_player].name, "Systems test");
    sr_players[world_player].account_id[0] = 123;
    sr_players[world_player].account_id[1] = 456;
    world_account = createaccount(world_player);
    CHECK(world_account >= 0);
    sr_players[world_player].account = world_account;
    world_human = createhuman(pos, rot, world_player);
    CHECK(world_human >= 0);
    sr_players[world_player].human_id = world_human;
    sr_humans[world_human].account = world_account;
    sr_itemtypes[0].inv_inertia = (vec3){ 1, 1, 1 };
    world_item = (int)createitem(0, &position, NULL, rotation);
    CHECK(world_item >= 0);
    memset(&sr_data_objecttype[0], 0, sizeof sr_data_objecttype[0]);
    world_object = createobject(0, &position, NULL, rotation, 0);
    CHECK(world_object >= 0);
    sr_data_server.max_players = 128;
}
