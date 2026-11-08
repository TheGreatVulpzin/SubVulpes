/* A second owner checks independent random state and timer cleanup through the public host API. */
#include <string.h>
#include "sv_plugin.h"
#include "sv_game_events.h"
#include "sv_server.h"
#include "sv_plugins_api.h"

static const SvHost *host;
static uint32_t random_index, timer_count;
static int armed;
#define CHECK(test) do { if (!(test)) return SV_FAILED; } while (0)

static SvResult SV_CALL event(SvOwner owner, const SvEvent *base)
{
    const SvTimerEvent *timer = (const SvTimerEvent *)base;
    (void)owner;
    if (base->kind == SV_EVENT_CORE_COMMAND) {
        SvCoreCommandEvent *command = (SvCoreCommandEvent *)base;
        CHECK(command->decision == SV_COMMAND_DEFAULT);
        if (!strcmp(command->arguments, "conflict")) command->decision = SV_COMMAND_DENY;
        if (!strcmp(command->arguments, "reverse")) command->decision = SV_COMMAND_ALLOW;
        return SV_OK;
    }
    CHECK(armed && base->kind == SV_EVENT_TIMER && base->size == sizeof *timer && timer->id == 10);
    ++timer_count;
    return SV_OK;
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *arguments)
{
    static const uint32_t expected[] = {3795205282u, 1093309337u, 3965216804u, 2716767539u};
    uint32_t value;
    (void)actor;
    if (!strcmp(arguments, "step")) {
        CHECK(random_index < 4 && host->random(owner, &value) == SV_OK && value == expected[random_index]);
        ++random_index;
    } else if (!strcmp(arguments, "arm")) {
        armed = 1;
        timer_count = 0;
        CHECK(host->every(owner, 1, 10) == SV_OK);
    } else if (!strcmp(arguments, "seen")) {
        CHECK(timer_count == 1);
    } else if (!strcmp(arguments, "pending")) {
        CHECK(host->subscribe(owner, 0, NULL) == SV_OK);
        CHECK(host->subscribe(owner, SV_EVENT_EXT_BIT(1) | SV_EVENT_EXT_BIT(2), event) == SV_OK);
        CHECK(host->after(owner, 2, 99) == SV_OK);
        armed = 0;
    } else if (!strcmp(arguments, "reply")) {
        CHECK(actor == 0 && host->log(owner, "other owner") == SV_OK);
    } else if (!strcmp(arguments, "denied")) {
        const void *table = NULL;
        const SvPlugins *plugins;
        const struct {
            uint32_t size, version;
            const SvServerControl *server;
            const SvConsole *console;
        } *infra;
        char text[32] = "untouched";
        uint32_t length = 99;
        CHECK(host->get_api(owner, SV_API_SERVER_CONTROL, 1, &table) == SV_DENIED && !table);
        CHECK(host->get_api(owner, SV_API_CONSOLE, 1, &table) == SV_DENIED && !table);
        CHECK(host->get_api(owner, SV_API_PLUGINS, 1, &table) == SV_OK);
        plugins = (const SvPlugins *)table;
        CHECK(plugins->borrow(owner, "test.read.infra", 1, &table) == SV_OK);
        infra = table;
        CHECK(infra->server->request(owner, SV_SERVER_SAVE) == SV_DENIED);
        CHECK(infra->console->execute(owner, "infrareply", text, sizeof text, &length) == SV_DENIED);
        CHECK(!strcmp(text, "untouched") && length == 99);
    } else {
        return SV_INVALID;
    }
    return SV_OK;
}

static SvResult SV_CALL enable(const SvHost *api, SvOwner owner)
{
    const void *table = (const void *)1;
    CHECK(api && api->size >= sizeof *api);
    CHECK(api->get_api(owner, 1, 1, &table) == SV_DENIED && table == NULL);
    table = (const void *)1;
    CHECK(api->get_api(owner, 2, 1, &table) == SV_DENIED && table == NULL);
    host = api;
    random_index = timer_count = 0;
    armed = 0;
    CHECK(host->subscribe(owner, SV_EVENT_EXT_BIT(1) | SV_EVENT_EXT_BIT(2), event) == SV_OK);
    return host->command(owner, "runtimeother", SV_COMMAND_LOCAL, command);
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
    strcpy(plugin->id, "test.runtime");
    strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    plugin->disable = disable;
    return SV_OK;
}
