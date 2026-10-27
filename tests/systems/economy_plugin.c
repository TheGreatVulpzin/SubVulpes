/* Plugin side of the economy area. Stub: it only checks that the table exists. The owner of the area replaces it. */
#include "systems_plugin.h"
#include "sv_game_economy.h"

/* Runs inside the tick callback. Returns 0, or the line of the check that failed. */
int economy_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    const SvGameEconomy *api;
    STEP(host->get_api(owner, SV_API_ECONOMY, SV_GAME_ECONOMY_VERSION, &table) == SV_OK);
    api = (const SvGameEconomy *)table;
    STEP(api && api->size >= sizeof *api && api->version == SV_GAME_ECONOMY_VERSION);
    return 0;
}
