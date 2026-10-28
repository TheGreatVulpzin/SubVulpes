/* The plugin of the system areas. It calls <area>_fixture() of every area in its first tick. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"

static const SvHost *host;

static SvResult SV_CALL tick(SvOwner owner, const SvEvent *event)
{
    static int done;
    char text[96];
    int line;
    /* The phone events only make the host wait for a decision. The shop test answers them itself. */
    if (event->kind != SV_EVENT_TICK || done)
        return SV_OK;
    done = 1;
#define RUN(area) \
    line = area##_fixture(host, owner); \
    if (line) { \
        snprintf(text, sizeof text, #area " fixture failed at line %d.", line); \
        host->log(owner, text); \
        return SV_FAILED; \
    } \
    host->log(owner, #area "_FIXTURE_PASS");
    SYSTEMS_AREAS(RUN)
#undef RUN
    return SV_OK;
}

static SvResult SV_CALL enable(const SvHost *api, SvOwner owner)
{
    if (!api || api->size < sizeof *api || api->abi != SV_PLUGIN_ABI)
        return SV_INCOMPATIBLE;
    host = api;
    return host->subscribe(owner, SV_EVENT_TICK | SV_EVENT_PHONE_DIGIT | SV_EVENT_PHONE_CALL, tick);
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "test.systems");
    strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    return SV_OK;
}
