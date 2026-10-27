/* Plugin side of the settings area. Stub: it only checks that the table exists. The owner of the area replaces it. */
#include "systems_plugin.h"
#include "sv_game_settings.h"

/* Runs inside the tick callback. Returns 0, or the line of the check that failed. */
int settings_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    const SvGameSettings *api;
    STEP(host->get_api(owner, SV_API_SETTINGS, SV_GAME_SETTINGS_VERSION, &table) == SV_OK);
    api = (const SvGameSettings *)table;
    STEP(api && api->size >= sizeof *api && api->version == SV_GAME_SETTINGS_VERSION);
    return 0;
}
