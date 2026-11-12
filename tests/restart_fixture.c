/* A plugin that asks the server to restart (or to stop) a few seconds after it starts. tests/restart_check.ps1 runs the real server with
 * it and checks the exit code: 75 for a restart request and 0 for a stop request. The variable SV_TEST_EXIT_REQUEST picks the request.
 * The plugin asks only once: it leaves the file user/restart-once.flag, and it does nothing while that file exists. So a launcher that
 * starts the server again does not enter a loop. Delete the file to ask again. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sv_plugin.h"
#include "sv_game_events.h"
#include "sv_server.h"
#include "sv_plugins_api.h"

static const SvHost *host;
static const SvServerControl *control;

static SvResult SV_CALL event(SvOwner owner, const SvEvent *base)
{
    const char *mode = getenv("SV_TEST_EXIT_REQUEST");
    if (base->kind != SV_EVENT_TIMER || !control)
        return SV_OK;
    {
        FILE *flag = fopen("user/restart-once.flag", "rb");
        if (flag) {
            fclose(flag);
            return SV_OK;
        }
        flag = fopen("user/restart-once.flag", "wb");
        if (flag)
            fclose(flag);
    }
    host->log(owner, "The restart fixture asks the server to end.");
    return control->request(owner, mode && !strcmp(mode, "stop") ? SV_SERVER_STOP : SV_SERVER_RESTART);
}

static SvResult SV_CALL enable(const SvHost *api, SvOwner owner)
{
    const void *table = NULL;
    host = api;
    if (api->get_api(owner, SV_API_SERVER_CONTROL, 1, &table) != SV_OK || !table)
        return SV_FAILED;
    control = (const SvServerControl *)table;
    if (host->subscribe(owner, SV_EVENT_EXT_BIT(1), event) != SV_OK)
        return SV_FAILED;
    return host->after(owner, 125, 1);
}

static void SV_CALL disable(SvOwner owner)
{
    (void)owner;
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "test.restart");
    strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    plugin->disable = disable;
    return SV_OK;
}
