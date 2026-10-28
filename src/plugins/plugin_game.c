#include "plugin_host.h"
#include "plugin_entities.h"
#include "plugin_game_internal.h"
#include "plugin_props.h"
#include "events.h"
#include "sr_runtime.h"
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static int ready;
static uint64_t reset_sequence;

int sv_game_start(void)
{
    ready = 0;
    sv_plugins_set_release(sv_props_release);
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

/* Every capability has an id, one version and a table. The tables of the system areas are stubs
 * until their owners add methods. See docs/parallel-work.md. */
typedef struct {
    uint32_t capability, version;
    const void *table;
} Capability;

static const Capability capabilities[] = {
    { SV_API_GAME_READ, SV_GAME_READ_VERSION, &sv_game_read },
    { SV_API_GAME_WRITE, SV_GAME_WRITE_VERSION, &sv_game_write },
    { SV_API_AI, SV_GAME_AI_VERSION, &sv_game_ai },
    { SV_API_ECONOMY, SV_GAME_ECONOMY_VERSION, &sv_game_economy },
    { SV_API_MISSION, SV_GAME_MISSION_VERSION, &sv_game_mission },
    { SV_API_SETTINGS, SV_GAME_SETTINGS_VERSION, &sv_game_settings },
    { SV_API_STORAGE, SV_GAME_STORAGE_VERSION, &sv_game_storage },
    { SV_API_ENGINE, SV_GAME_ENGINE_VERSION, &sv_game_engine },
    { SV_API_SHOP, SV_GAME_SHOP_VERSION, &sv_game_shop },
    { SV_API_GAME_PROPS, SV_GAME_PROPS_VERSION, &sv_game_props },
};

SvResult sv_game_api(uint32_t capability, uint32_t version, const void **api)
{
    size_t i;
    for (i = 0; i < sizeof capabilities / sizeof capabilities[0]; ++i) {
        if (capabilities[i].capability != capability)
            continue;
        if (capabilities[i].version != version)
            return SV_INCOMPATIBLE;
        *api = capabilities[i].table;
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

/* An actor is the reference of the player who typed the command. Administration is the flag that
 * /admin sets, so a plugin command with the admin level follows the inherited password login. */
/* A reply to a player: a message of the admin kind, as the replies of the chat commands. */
void sv_game_tell(uint64_t actor, const char *text)
{
    int32_t index = sv_entity_resolve(SV_ENTITY_PLAYER, actor);
    if (index >= 0 && text && strlen(text) <= 63)
        createevent_message_to(4, text, index);
}

int sv_game_actor_admin(uint64_t actor)
{
    int32_t index = sv_entity_resolve(SV_ENTITY_PLAYER, actor);
    return index >= 0 && sr_players[index].is_admin != 0;
}
