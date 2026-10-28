/* Timing output is evidence, never a performance gate. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "plugin_host.h"
#include "plugin_entities.h"
#include "plugin_game_events.h"
#include "plugin_actions.h"
#include "sv_game_ai.h"
#include "sv_game_mission.h"
#include "sv_game_shop.h"
#include "sr_runtime.h"

enum { COST_RUNS = 10000, COST_BOTS = 256 };

static double microseconds(LARGE_INTEGER start, LARGE_INTEGER stop, LARGE_INTEGER frequency, int runs)
{
    return (double)(stop.QuadPart - start.QuadPart) * 1000000.0 / (double)frequency.QuadPart / runs;
}

#define MEASURE(label, code) do { \
    QueryPerformanceCounter(&start); \
    for (run = 0; run < COST_RUNS; ++run) { code; } \
    QueryPerformanceCounter(&stop); \
    printf("COST " label " %.3f us\n", microseconds(start, stop, frequency, COST_RUNS)); \
} while (0)

int plugin_cost_run(void)
{
    LARGE_INTEGER start, stop, frequency;
    int run, player, i;
    if (!QueryPerformanceFrequency(&frequency) || !sv_plugins_start())
        return 1;
    for (i = 0; i < COST_BOTS; ++i) {
        sr_players[i].active = sr_players[i].is_bot = 1;
        sv_entity_created(SV_ENTITY_PLAYER, (uint32_t)i);
    }
    for (i = 0; i < 512; ++i) {
        sr_humans[i].active = 1;
        sr_humans[i].pos = (vec3){10, 20, 30};
        sv_entity_created(SV_ENTITY_HUMAN, (uint32_t)i);
    }
    for (i = 0; i < 1024; ++i) {
        sr_items[i].active = 1;
        sr_items[i].pos = (vec3){10, 20, 30};
        sv_entity_created(SV_ENTITY_ITEM, (uint32_t)i);
    }
    sv_game_ready();
    sv_systems_install();
    sv_game_event_poll();
    MEASURE("shop_poll", sv_shop_poll());
    MEASURE("mission_before_tick", sv_mission_before_tick());
    MEASURE("game_event_poll", sv_game_event_poll());
    MEASURE("actions_poll", sv_actions_poll());
    MEASURE("ai_256", for (player = 0; player < COST_BOTS; ++player) (void)sv_ai_before_player(player));
    MEASURE("all_hooks_256",
        sv_shop_poll(); sv_mission_before_tick(); sv_game_event_poll(); sv_actions_poll();
        for (player = 0; player < COST_BOTS; ++player) (void)sv_ai_before_player(player));
    QueryPerformanceCounter(&start);
    sv_plugins_tick();
    QueryPerformanceCounter(&stop);
    printf("COST idle_tick_single %.3f us\n", microseconds(start, stop, frequency, 1));
    MEASURE("idle_tick_mean", sv_plugins_tick());
    if (!sv_plugins_command(0, "costwithin dense") || sv_plugins_failed()) {
        sv_plugins_stop();
        return 1;
    }
    for (i = 0; i < 512; ++i)
        sr_humans[i].pos.x = i == 511 ? 10.0f : 5000.0f;
    for (i = 0; i < 1024; ++i)
        sr_items[i].pos.x = i == 1023 ? 10.0f : 5000.0f;
    if (!sv_plugins_command(0, "costwithin sparse") || sv_plugins_failed()) {
        sv_plugins_stop();
        return 1;
    }
    sv_plugins_stop();
    return 0;
}
