#include <string.h>
#include "sv_plugin.h"

__declspec(dllimport) int __cdecl sv_test_helper(void);

static SvResult SV_CALL enable(const SvHost *host, SvOwner owner)
{
    (void)host;
    (void)owner;
    return SV_OK;
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (!plugin || abi != SV_PLUGIN_ABI || plugin->size != sizeof *plugin)
        return SV_INCOMPATIBLE;
    if (sv_test_helper() != 42)
        return SV_FAILED;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "test.dependent");
    strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    return SV_OK;
}
