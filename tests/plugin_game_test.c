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
    CHECK(sv_game_start());
    player_lifetime();
    body_lifetime();
    item_lifetime();
    object_lifetime();
    account_lifetime();
    old = sv_entity_at(SV_ENTITY_PLAYER, 0);
    sv_game_stop();
    CHECK(sv_entity_resolve(SV_ENTITY_PLAYER, old) == -1);
    CHECK(sv_game_start());
    sv_entity_created(SV_ENTITY_PLAYER, 0);
    CHECK(sv_entity_at(SV_ENTITY_PLAYER, 0) != old);
    CHECK(sv_entity_resolve(SV_ENTITY_PLAYER, old) == -1);
    sv_game_stop();
    puts("Game entity lifetimes passed against real constructors and retirement paths.");
    return 0;
}
