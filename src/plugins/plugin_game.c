#include "plugin_host.h"
#include "plugin_entities.h"
#include "plugin_game_internal.h"
#include "sr_runtime.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static int ready;
static uint64_t reset_sequence;

int sv_game_start(void)
{
    ready = 0;
    return sv_entities_start();
}

void sv_game_stop(void)
{
    ready = 0;
    sv_entities_stop();
}

void sv_game_ready(void) { ready = 1; }
void sv_game_reset(void) { ++reset_sequence; }
int sv_game_is_ready(void) { return ready; }
uint64_t sv_game_reset_sequence(void) { return reset_sequence; }

SvResult sv_game_api(uint32_t capability, uint32_t version, const void **api)
{
    if (capability == SV_API_GAME_READ) {
        if (version != SV_GAME_READ_VERSION)
            return SV_INCOMPATIBLE;
        *api = &sv_game_read;
        return SV_OK;
    }
    if (capability == SV_API_GAME_WRITE) {
        if (version != SV_GAME_WRITE_VERSION)
            return SV_INCOMPATIBLE;
        *api = &sv_game_write;
        return SV_OK;
    }
    return SV_NOT_FOUND;
}

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
    /* Remote command authorization needs a separate permission contract. */
    (void)actor;
    return 0;
}
