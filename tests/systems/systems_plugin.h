#ifndef SV_SYSTEMS_PLUGIN_H
#define SV_SYSTEMS_PLUGIN_H

/* Shared includes for the plugin side of the system areas. The plugin side is a native module
 * that the test program loads. It runs in the tick callback and calls the public API. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "areas.h"
#include "sv_game_read.h"
#include "sv_game_props.h"
#include "sv_plugin.h"

/* Each function returns 0 when all checks pass, or the line of the check that failed.
 * Use STEP(condition) to return the line. */
#define STEP(condition) do { if (!(condition)) return __LINE__; } while (0)

/* Sets or clears god mode on a player. The tests use it to mark a result. God mode is a row of the property table. */
static inline SvResult systems_godmode(const SvGameProps *props, SvOwner owner, SvEntity player, int flag)
{
    uint32_t id = 0;
    SvValue value = { 0 };
    SvResult result = props->find(owner, "player.godmode", &id);
    value.i = flag != 0;
    return result == SV_OK ? props->set(owner, id, player, 0, value) : result;
}

#define DECLARE_PLUGIN(area) int area##_fixture(const SvHost *host, SvOwner owner);
SYSTEMS_AREAS(DECLARE_PLUGIN)
#undef DECLARE_PLUGIN

#endif
