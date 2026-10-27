/* Plugin side of the storage area. Stub: it only checks that the table exists. The owner of the area replaces it. */
#include "systems_plugin.h"
#include "sv_game_storage.h"

/* Runs inside the tick callback. Returns 0, or the line of the check that failed. */
int storage_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    const SvGameStorage *api;
    STEP(host->get_api(owner, SV_API_STORAGE, SV_GAME_STORAGE_VERSION, &table) == SV_OK);
    api = (const SvGameStorage *)table;
    STEP(api && api->size >= sizeof *api && api->version == SV_GAME_STORAGE_VERSION);
    return 0;
}
