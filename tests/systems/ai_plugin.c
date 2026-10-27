/* Plugin side of the ai area. Stub: it only checks that the table exists. The owner of the area replaces it. */
#include "systems_plugin.h"
#include "sv_game_ai.h"

/* Runs inside the tick callback. Returns 0, or the line of the check that failed. */
int ai_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    const SvGameAi *api;
    STEP(host->get_api(owner, SV_API_AI, SV_GAME_AI_VERSION, &table) == SV_OK);
    api = (const SvGameAi *)table;
    STEP(api && api->size >= sizeof *api && api->version == SV_GAME_AI_VERSION);
    return 0;
}
