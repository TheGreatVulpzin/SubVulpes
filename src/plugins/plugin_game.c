#include "plugin_host.h"
#include "sr_runtime.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void sv_game_status(SvStatus *status)
{
    uint32_t i;
    static DWORD first;
    if (!first)
        first = GetTickCount();
    status->size = sizeof *status;
    status->tick = (uint32_t)sr_game.ticks;
    status->players = status->bots = 0;
    status->uptime_ms = GetTickCount() - first;
    for (i = 0; i < SR_MAX_PLAYER; ++i)
        if (sr_players[i].active)
            sr_players[i].is_bot ? ++status->bots : ++status->players;
}

int sv_game_actor_admin(uint64_t actor)
{
    /* Until entity handles exist, only local operator commands are authorized. */
    (void)actor;
    return 0;
}
