#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "sv_game_read.h"

static const SvHost *host;
static const SvGameRead *read_api;

static SvResult SV_CALL within_cost(SvOwner owner, uint64_t actor, const char *arguments)
{
    static const uint32_t kinds[] = {SV_ENTITY_HUMAN, SV_ENTITY_ITEM};
    LARGE_INTEGER frequency, start, stop;
    SvEntity found[SV_ENTITY_PAGE_MAX];
    uint32_t count, kind, run;
    char text[160];
    const int dense = strcmp(arguments, "dense") == 0;
    (void)actor;
    if (!QueryPerformanceFrequency(&frequency))
        return SV_FAILED;
    for (kind = 0; kind < 2; ++kind) {
        QueryPerformanceCounter(&start);
        for (run = 0; run < 10000; ++run)
            if (read_api->within(owner, kinds[kind], (SvVector){0, 0, 0}, 4096, found, NULL,
                    SV_ENTITY_PAGE_MAX, &count) != (dense ? SV_LIMIT : SV_OK) ||
                count != (dense ? SV_ENTITY_PAGE_MAX : 1u))
                return SV_FAILED;
        QueryPerformanceCounter(&stop);
        snprintf(text, sizeof text, "COST within_%s_%s %.3f us",
            kind ? "items" : "humans", dense ? "dense" : "sparse",
            (double)(stop.QuadPart - start.QuadPart) * 1000000.0 / (double)frequency.QuadPart / 10000.0);
        if (host->log(owner, text) != SV_OK)
            return SV_FAILED;
    }
    return SV_OK;
}

static SvResult SV_CALL enable(const SvHost *api, SvOwner owner)
{
    const void *table;
    host = api;
    if (host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) != SV_OK)
        return SV_INCOMPATIBLE;
    read_api = (const SvGameRead *)table;
    return host->command(owner, "costwithin", SV_COMMAND_LOCAL, within_cost);
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "test.cost");
    strcpy(plugin->version, "0.1.0");
    plugin->enable = enable;
    return SV_OK;
}
