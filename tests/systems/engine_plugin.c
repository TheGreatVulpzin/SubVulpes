/* Plugin side of the engine area. Stub: it only checks that the table exists. The owner of the area replaces it. */
#include "systems_plugin.h"
#include "sv_game_engine.h"

/* Runs inside the tick callback. Returns 0, or the line of the check that failed. */
int engine_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    const SvGameEngine *api;
    STEP(host->get_api(owner, SV_API_ENGINE, SV_GAME_ENGINE_VERSION, &table) == SV_OK);
    api = (const SvGameEngine *)table;
    STEP(api && api->size >= sizeof *api && api->version == SV_GAME_ENGINE_VERSION);
    return 0;
}
