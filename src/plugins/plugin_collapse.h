#ifndef SV_PLUGIN_COLLAPSE_H
#define SV_PLUGIN_COLLAPSE_H

#include "sv_game_engine.h"

/* The health at or below which a human falls down: it drops its items, goes limp, and the camera and the pose follow.
 * The engine parameter human.death_start sets it. The default is 0, so a human collapses at 0 health, as the game always
 * did. Final death (-5), the dead joint mode (below 0), the weakness steps (25 and 10) and the healing range keep their own
 * numbers. */
static inline int human_collapsed(int health)
{
    return health <= sv_engine_integer(SV_ENGINE_DEATH_START, 0);
}

#endif
