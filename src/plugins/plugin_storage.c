/* Capability 7: storage of the plugin (N18). Stub: the table has no methods yet. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "sv_game_storage.h"
#include "plugin_game_internal.h"

const SvGameStorage sv_game_storage = {
    sizeof(SvGameStorage), SV_GAME_STORAGE_VERSION
};
