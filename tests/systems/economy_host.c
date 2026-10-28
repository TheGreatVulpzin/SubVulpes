#include "systems_host.h"

static int malformed, overflow, wrong_team;

void economy_prepare(void)
{
    int j;
    SvCompany out = {sizeof out};
    CHECK(sv_game_economy.company(0, 0, &out) == SV_STATE);
    CHECK(sv_game_economy.set_price(0, 0, 100) == SV_STATE);
    CHECK(sv_game_economy.set_shares(0, 0, 100) == SV_STATE);
    CHECK(sv_game_economy.add_stocks(0, 0, 1) == SV_STATE);
    sr_server = 1;
    for (j = 0; j < 5; ++j) {
        g_market.stocks[j].shares = 100;
        g_market.stocks[j].price = 100;
        g_market.stocks[j].open_price = 100;
        g_market.stocks[j].price_int = 100;
        g_market.stocks[j].stake = 10;
    }
    sr_players[world_player].team = 0;
    sr_humans[world_human].team = 0;
    sr_players[world_player].stocks = 4;
    sr_players[world_player].money = 12345;
    malformed = createplayer();
    overflow = createplayer();
    wrong_team = createplayer();
    CHECK(malformed >= 0 && overflow >= 0 && wrong_team >= 0);
    strcpy(sr_players[malformed].name, "Econ malformed");
    sr_players[malformed].team = 2;
    sr_players[malformed].stocks = -1;
    strcpy(sr_players[overflow].name, "Econ overflow");
    sr_players[overflow].team = 1;
    sr_players[overflow].stocks = INT32_MAX;
    g_market.stocks[1].shares = INT32_MAX;
    strcpy(sr_players[wrong_team].name, "Econ wrong team");
    sr_players[wrong_team].team = 0x40000000;
    g_market.stocks[4].price = 40000;
}

void economy_verify(void)
{
    int j, saw_stock = 0, saw_finance = 0;
    CHECK(g_market.stocks[0].shares == 998);
    CHECK(g_market.stocks[0].price == 100 && g_market.stocks[0].price_int == 100);
    CHECK(g_market.stocks[0].stake == 10 && g_market.stocks[0].open_price == 100);
    CHECK(sr_players[world_player].stocks == 21 && sr_players[world_player].money == 12345);
    CHECK(sr_players[overflow].stocks == INT32_MAX && g_market.stocks[1].shares == INT32_MAX);
    CHECK(sr_players[malformed].stocks == -1 && g_market.stocks[2].shares == 100);
    CHECK(sr_players[wrong_team].stocks == 0);
    CHECK(g_market.stocks[4].shares == 2500 && g_market.stocks[4].price == 100);
    for (j = 0; (uint32_t)j < sr_numofevents; ++j) {
        if (sr_events[j].type == 13) saw_stock = 1;
        if (sr_events[j].type == 8 && sr_events[j].param == world_player && sr_events[j].attach_id == 21)
            saw_finance = 1;
    }
    CHECK(saw_stock && saw_finance);
    puts("ECONOMY_PASS");
}
