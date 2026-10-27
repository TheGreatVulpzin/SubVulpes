/* Plugin side of the mission area. Stub: it only checks that the table exists. The owner of the area replaces it. */
#include "systems_plugin.h"
#include "sv_game_mission.h"

/* Runs inside the tick callback. Returns 0, or the line of the check that failed. */
int mission_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    const SvGameMission *api;
    STEP(host->get_api(owner, SV_API_MISSION, SV_GAME_MISSION_VERSION, &table) == SV_OK);
    api = (const SvGameMission *)table;
    STEP(api && api->size >= sizeof *api && api->version == SV_GAME_MISSION_VERSION);
    return 0;
}
