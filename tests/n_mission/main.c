#include "systems_host.h"
#include "sv_game_mission.h"
#include "city.h"
#include "economy.h"

static void command(const char *text)
{
    sr_players[world_player].godmode = 0;
    CHECK(sv_plugins_command(0, text));
    CHECK(sr_players[world_player].godmode == 1);
}

static int scenario_active(int id)
{
    int t, j;
    for (t = 0; t < 5; ++t)
        for (j = 0; j < 16; ++j)
            if (team_missions(t)[j].active && team_missions(t)[j].scenario == id)
                return 1;
    return 0;
}

int main(int argc, char **argv)
{
    int j, id, before;
    world_start();
    g_scenario.next_id = 10;
    g_game_world.clock = 100;
    sv_systems_install();
    sv_game_ready();
    if (argc > 1 && !strcmp(argv[1], "--missing-capability")) {
        command("nmission missing");
        sv_plugins_stop();
        puts("N_MISSION_CAP_PASS");
        return 0;
    }
    command("nmission check");
    /* No building and no road: the builder makes nothing, and the scenario counter stays. */
    command("nmission no-site");
    CHECK(g_scenario.next_id == 10);

    city_addintersection(0, 0, 0);
    city_addintersection(40, 0, 0);
    addcitystreet(0, 1, 1, 1, "Straight");
    command("nmission chase");
    CHECK(g_scenario.next_id == 11);
    id = 10;
    CHECK(team_missions(0)[0].active && team_missions(1)[0].active && !team_missions(2)[0].active);
    CHECK(team_missions(0)[0].scenario == id && team_missions(1)[0].scenario == id);
    CHECK(team_missions(0)[0].item == team_missions(1)[0].item);
    CHECK(team_missions(0)[0].own_growth > 0);

    /* A team with eight client slots in use refuses before anything is made. */
    for (j = 0; j < 8; ++j) {
        team_missions(2)[j] = team_missions(0)[0];
        team_missions(2)[j].scenario = 500 + j;
    }
    before = g_scenario.next_id;
    command("nmission full");
    CHECK(g_scenario.next_id == before && !team_missions(0)[1].active);
    for (j = 0; j < 8; ++j)
        team_missions(2)[j].active = 0;

    command("nmission cancel");
    CHECK(!scenario_active(id));
    command("nmission chase");
    id = g_scenario.next_id - 1;
    CHECK(scenario_active(id));

    if (argc > 1 && !strcmp(argv[1], "--reset")) {
        sv_game_reset();
        team_missions(0)[0].active = 1;
        team_missions(0)[0].scenario = id;
        sv_plugins_stop();
        /* A mission of the new world with the same number is not the plugin's. */
        CHECK(team_missions(0)[0].active);
        puts("N_MISSION_RESET_PASS");
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--completed")) {
        /* The scenario ended in the game, and a new one took its number: stop leaves it. */
        scenario_setmissioninactive(id);
        sv_mission_before_tick();
        team_missions(3)[0] = team_missions(0)[0];
        team_missions(3)[0].active = 1;
        team_missions(3)[0].scenario = id;
        sv_plugins_stop();
        CHECK(team_missions(3)[0].active);
        puts("N_MISSION_COMPLETED_PASS");
        return 0;
    }
    /* Stop ends the missions of the plugin. */
    sv_plugins_stop();
    CHECK(!scenario_active(id));
    puts("N_MISSION_PASS");
    return 0;
}
