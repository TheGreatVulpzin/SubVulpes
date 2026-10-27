/* Plugin side of the shop area. Stub: it only checks that the table exists. The owner of the area replaces it. */
#include "systems_plugin.h"
#include "sv_game_shop.h"

/* Runs inside the tick callback. Returns 0, or the line of the check that failed. */
int shop_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    const SvGameShop *api;
    STEP(host->get_api(owner, SV_API_SHOP, SV_GAME_SHOP_VERSION, &table) == SV_OK);
    api = (const SvGameShop *)table;
    STEP(api && api->size >= sizeof *api && api->version == SV_GAME_SHOP_VERSION);
    return 0;
}
