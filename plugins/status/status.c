#include <stdio.h>
#include <string.h>
#include "sv_plugin.h"

#ifndef SV_STATUS_VERSION
#define SV_STATUS_VERSION "0.1.0"
#endif

static const SvHost *api;

static SvResult SV_CALL status_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    SvStatus status = { sizeof status };
    char line[160];
    SvResult result;
    (void)actor;
    (void)arguments;
    result = api->status(owner, &status);
    if (result != SV_OK)
        return result;
    snprintf(line, sizeof line, "Status: tick %u, %u players, %u bots, uptime %u ms.",
             status.tick, status.players, status.bots, status.uptime_ms);
    return api->log(owner, line);
}

static SvResult SV_CALL enable(const SvHost *host, SvOwner owner)
{
    if (host->abi != SV_PLUGIN_ABI || host->size < SV_HOST_V1_SIZE)
        return SV_INCOMPATIBLE;
    api = host;
    return api->command(owner, "status", SV_COMMAND_PUBLIC, status_command);
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "sv.status");
    strcpy(plugin->version, SV_STATUS_VERSION);
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    return SV_OK;
}
