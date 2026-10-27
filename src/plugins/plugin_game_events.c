/* The core side of the plugin events: it turns a game index into a reference and posts it. See plugin_events.c. */
#include "plugin_game_events.h"
#include "plugin_host.h"
#include "plugin_entities.h"
#include "sr_runtime.h"

/* The player whose bullet hit the human last, or -1. It lives as long as the human. */
static int32_t last_hit[SR_MAX_HUMAN];

static uint32_t now(void)
{
    return (uint32_t)sr_game.ticks;
}

void sv_game_event_spawn(int player, int human)
{
    if (human < 0 || human >= SR_MAX_HUMAN)
        return;
    last_hit[human] = -1;
    if (player < 0 || player >= SR_MAX_PLAYER)
        return;
    sv_events_post(SV_EVENT_SPAWN, now(), sv_entity_at(SV_ENTITY_PLAYER, player), sv_entity_at(SV_ENTITY_HUMAN, human), 0);
}

void sv_game_event_death(int human)
{
    SvEntity killer = 0;
    int owner, player;
    if (human < 0 || human >= SR_MAX_HUMAN || !sv_plugins_wants(SV_EVENT_DEATH))
        return;
    player = sr_humans[human].player_id;
    owner = last_hit[human];
    if (owner >= 0 && owner < SR_MAX_PLAYER && sr_players[owner].active) {
        int body = sr_players[owner].human_id;
        if (body >= 0 && body < SR_MAX_HUMAN && body != human && sr_humans[body].active)
            killer = sv_entity_at(SV_ENTITY_HUMAN, body);
    }
    sv_events_post(SV_EVENT_DEATH, now(), player >= 0 && player < SR_MAX_PLAYER ? sv_entity_at(SV_ENTITY_PLAYER, player) : 0,
        sv_entity_at(SV_ENTITY_HUMAN, human), killer);
}

void sv_game_event_leave(int player)
{
    if (player < 0 || player >= SR_MAX_PLAYER)
        return;
    sv_events_post(SV_EVENT_LEAVE, now(), sv_entity_at(SV_ENTITY_PLAYER, player), 0, 0);
}

void sv_game_event_hit(int human, int owner)
{
    if (human < 0 || human >= SR_MAX_HUMAN)
        return;
    last_hit[human] = owner;
}
