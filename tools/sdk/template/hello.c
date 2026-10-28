/* A small plugin. The command "hello" writes a line to the server log with the server name and the player count.
 * Change this file, then build with build.bat. It uses the public plugin API only. */
#include <stdio.h>
#include <string.h>
#include "sv_plugin.h"
#include "sv_game_read.h"

static const SvHost *host;
static const SvGameRead *read_api;

static SvResult SV_CALL hello(SvOwner owner, uint64_t actor, const char *arguments)
{
    SvStatus status = { sizeof status };
    SvWorldState world = { sizeof world };
    char text[160];
    (void)actor;
    (void)arguments;
    if (host->status(owner, &status) != SV_OK)
        return SV_FAILED;
    if (read_api->world(owner, &world) == SV_OK)
        snprintf(text, sizeof text, "Hello from %s: %u players and %u bots.", world.server_name, status.players, status.bots);
    else
        snprintf(text, sizeof text, "Hello: %u players and %u bots.", status.players, status.bots);
    return host->log(owner, text);
}

static SvResult SV_CALL enable(const SvHost *h, SvOwner owner)
{
    SvResult r;
    if (h->abi != SV_PLUGIN_ABI || h->size < SV_HOST_V1_SIZE)
        return SV_INCOMPATIBLE;
    host = h;
    /* The manifest must list "read" in its capabilities, or this call gives SV_DENIED. */
    r = h->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, (const void **)&read_api);
    if (r != SV_OK)
        return r;
    return h->command(owner, "hello", SV_COMMAND_LOCAL, hello);
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "my.hello");
    strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    return SV_OK;
}
