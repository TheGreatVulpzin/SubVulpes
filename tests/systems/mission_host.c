/* Host side of the mission area. Stub: it only prints the marker. The owner of the area replaces it. */
#include "systems_host.h"

/* Runs before the tick. Set up the game state that the plugin side needs. */
void mission_prepare(void)
{
}

/* Runs after the tick. Check the game state that the plugin side changed. */
void mission_verify(void)
{
    puts("MISSION_PASS");
}
