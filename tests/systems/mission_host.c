#include "systems_host.h"
#include "economy.h"
#include "sr_server.h"

static int make_disk(int type)
{
    const vec3 position = {80, 20, 80};
    const vec3 rotation[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    int item = (int)createitem(type, &position, NULL, rotation);
    CHECK(item >= 0);
    return item;
}

static void command(const char *text)
{
    sr_players[world_player].godmode = 0;
    CHECK(sv_plugins_command(0, text));
    CHECK(sr_players[world_player].godmode == 1);
}

/* Runs before the tick. Set up the game state that the plugin side needs. */
void mission_prepare(void)
{
    int j, full_item;
    SvMissionSlots out = {sizeof out};
    SvMissionStart input = {sizeof input};
    SvMissionRef r = {0};
    CHECK(sv_game_mission.slots(0, 0, &out) == SV_STATE);
    CHECK(sv_game_mission.start(0, 0, &input, &r) == SV_STATE);
    CHECK(sv_game_mission.cancel(0, &r) == SV_STATE);
    (void)make_disk(17);
    (void)make_disk(18);
    full_item = make_disk(19);
    for (j = 0; j < 16; ++j) {
        Mission *m = &team_missions(4)[j];
        memset(m, 0, sizeof *m);
        m->active = 1;
        m->scenario = 100 + j;
        m->kind = 2;
        m->item = full_item;
        m->item_type = 19;
        m->expire = 1000;
    }
    team_missions(3)[0] = team_missions(4)[0];
    for (j = 0; j < 8; ++j) {
        team_missions(1)[j] = team_missions(4)[j];
        team_missions(1)[j].scenario = 200 + j;
    }
    team_missions(3)[0].scenario = 0;
    team_missions(3)[1] = team_missions(4)[0];
    team_missions(3)[1].scenario = 300;
    team_missions(3)[1].stake = 1;
    g_scenario.next_id = 10;
    g_game_world.clock = 100;
    sv_mission_install();
    sv_mission_release_owner(0);
    CHECK(team_missions(3)[0].active);
}

/* Runs after the tick. Check the game state that the plugin side changed. */
void mission_verify(void)
{
    int player = -1, j, saved_money, saved_id, reused;
    int saw_start = 0;
    Mission *m = &team_missions(0)[0];
    CHECK(m->active && m->kind == 5 && m->cash == 100 && m->partner == 1);
    CHECK(g_scenario.next_id == 12);
    CHECK(m->expire == 300 && m->stake == 0 && m->own_growth == 0 && m->other_growth == 0);
    for (j = 0; (uint32_t)j < sr_numofevents; ++j)
        if (sr_events[j].type == 12 && ((uint32_t)sr_events[j].param & 0xffffu) == 0x501u &&
            sr_events[j].attach_type == 100 * 256 + 1)
            saw_start = 1;
    CHECK(saw_start);
    CHECK(scenario_getnextmission(4) == 16);
    /* The AI host test changed the reset sequence. An old mission reference must now fail. */
    command("mission.control stale");
    scenario_setmissioninactive(m->scenario);
    sv_mission_before_tick();
    (void)make_disk(20);
    (void)make_disk(21);
    (void)make_disk(22);
    command("mission.control post-reset");
    for (j = 0; j < SR_MAX_PLAYER; ++j)
        if (sr_players[j].active && !strcmp(sr_players[j].name, "Econ malformed"))
            player = j;
    CHECK(player >= 0);
    CHECK(team_missions(2)[0].active && team_missions(2)[1].active && team_missions(2)[2].active);
    team_missions(2)[8] = team_missions(2)[0];
    team_missions(2)[8].cash = 0;
    CHECK(!sv_mission_before_complete(2, 0));
    team_missions(2)[8].active = 0;
    saved_money = sr_players[player].money;
    sr_players[player].money = INT32_MAX;
    CHECK(!sv_mission_before_complete(2, 0));
    sr_players[player].money = saved_money;
    g_market.stocks[1].price = 600;
    CHECK(!sv_mission_before_complete(2, 0));
    g_market.stocks[1].price = 100;
    CHECK(sv_mission_before_complete(2, 0));
    CHECK(!sv_mission_before_complete(-1, 0) && !sv_mission_before_complete(0, 16));
    logic_teammissioncomplete(2, 0);
    CHECK(!team_missions(2)[0].active && sr_players[player].money == saved_money + 100);
    g_game_world.clock = 121;
    sv_mission_before_tick();
    CHECK(!team_missions(2)[1].active && team_missions(2)[2].active);
    reused = team_missions(2)[2].item;
    sv_entity_deleted(SV_ENTITY_ITEM, (uint32_t)reused);
    sv_entity_created(SV_ENTITY_ITEM, (uint32_t)reused);
    sr_items[reused].time_to_live = 777;
    sv_mission_before_tick();
    CHECK(!team_missions(2)[2].active && sr_items[reused].time_to_live == 777);
    (void)make_disk(17);
    saved_id = g_scenario.next_id;
    g_scenario.next_id = INT32_MAX;
    command("mission.control limit-id");
    CHECK(g_scenario.next_id == INT32_MAX);
    g_scenario.next_id = saved_id;
    command("mission.control owner");
    CHECK(team_missions(2)[0].active);
    sv_mission_release_owner(0);
    CHECK(team_missions(3)[0].active && team_missions(2)[0].active);
    sv_mission_release_owner(1);
    CHECK(!team_missions(2)[0].active && team_missions(3)[0].active && team_missions(3)[1].active);
    CHECK(g_game_world.clock == 121);
    CHECK(!sv_plugins_failed());
    puts("MISSION_PASS");
}
