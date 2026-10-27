/* The test program of the system areas. For each area:
 *   <area>_prepare()  before the tick, sets up the game
 *   the plugin tick   calls <area>_fixture() through the public API (see fixture.c)
 *   <area>_verify()   after the tick, checks the game and prints <AREA>_PASS
 * To add checks, edit only the files of your area. See docs/parallel-work.md. */
#include "systems_host.h"

int main(void)
{
    world_start();
#define PREPARE(area) area##_prepare();
    SYSTEMS_AREAS(PREPARE)
#undef PREPARE
    sv_game_ready();
    sv_plugins_tick();
    CHECK(!sv_plugins_failed());
#define VERIFY(area) area##_verify();
    SYSTEMS_AREAS(VERIFY)
#undef VERIFY
    sv_plugins_stop();
    puts("SYSTEMS_PASS");
    return 0;
}
