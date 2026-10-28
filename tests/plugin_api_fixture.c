/* One source for two test plugins: a library that offers an API (FIXTURE_LIBRARY), and a plugin that uses it. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sv_plugin.h"
#include "sv_plugins_api.h"
#include "sv_game_read.h"

#ifdef FIXTURE_LIBRARY
#define FIXTURE_ID "test.library"
#define FIXTURE_VERSION "1.2.0"
#else
#define FIXTURE_ID "test.consumer"
#define FIXTURE_VERSION "0.1.0"
#endif

typedef struct {
    uint32_t size, version;
    SvResult (SV_CALL *add)(SvOwner caller, int32_t a, int32_t b, int32_t *sum);
} MathApi;

static const SvHost *host;
static const SvPlugins *plugins;
static const char *mode;

/* The function of a library runs with the owner of the plugin that calls it, so it logs as that plugin. */
static SvResult SV_CALL add(SvOwner caller, int32_t a, int32_t b, int32_t *sum)
{
    if (!sum)
        return SV_INVALID;
    *sum = a + b;
    return host->log(caller, "library add");
}

#define STEP(condition) do { if (!(condition)) { \
    char step_text[96]; \
    _snprintf(step_text, sizeof step_text, "fixture step failed on line %d", __LINE__); \
    step_text[sizeof step_text - 1] = 0; \
    host->log(owner, step_text); \
    return SV_FAILED; } } while (0)

#ifdef FIXTURE_LIBRARY
static const MathApi math = { sizeof(MathApi), 1, add };

static SvResult SV_CALL on_event(SvOwner owner, const SvEvent *event)
{
    (void)owner;
    (void)event;
    return SV_OK;
}

static SvResult SV_CALL on_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    (void)actor;
    (void)arguments;
    return host->log(owner, "fixture command");
}

static SvResult library_checks(SvOwner owner)
{
    static const MathApi short_head = { 4, 1, add }, other_version = { sizeof(MathApi), 2, add };
    const void *table = NULL;
    char name[SV_PLUGIN_ID_SIZE];
    unsigned count = 2;
    /* A library has no game behavior: no event, no command and no game table. */
    STEP(host->subscribe(owner, SV_EVENT_TICK, on_event) == SV_DENIED);
    STEP(host->command(owner, "libcmd", SV_COMMAND_PUBLIC, on_command) == SV_DENIED);
    STEP(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_DENIED && !table);
    STEP(host->subscribe(owner, 0, NULL) == SV_OK);
    STEP(plugins->provide(owner, "test.consumer", 1, &math) == SV_DENIED);
    STEP(plugins->provide(owner, "test.libraryx", 1, &math) == SV_DENIED);
    STEP(plugins->provide(owner, "test", 1, &math) == SV_DENIED);
    STEP(plugins->provide(owner, "test.library", 1, &short_head) == SV_INVALID);
    STEP(plugins->provide(owner, "test.library", 1, &other_version) == SV_INVALID);
    STEP(plugins->provide(owner, "test.library", 0, &math) == SV_INVALID);
    STEP(plugins->provide(owner, "test.library", 1, NULL) == SV_INVALID);
    STEP(plugins->provide(owner, NULL, 1, &math) == SV_INVALID);
    STEP(plugins->provide(owner, "Test.Library", 1, &math) == SV_INVALID);
    STEP(plugins->provide(owner, "test.library", 1, &math) == SV_OK);
    STEP(plugins->provide(owner, "test.library", 1, &math) == SV_CONFLICT);
    STEP(plugins->provide(owner, "test.library.extra", 1, &math) == SV_OK);
    for (;; ++count) {
        _snprintf(name, sizeof name, "test.library.n%u", count);
        name[sizeof name - 1] = 0;
        if (plugins->provide(owner, name, 1, &math) != SV_OK)
            break;
    }
    STEP(count == SV_PLUGINS_API_PER_PLUGIN && plugins->provide(owner, name, 1, &math) == SV_LIMIT);
    return SV_OK;
}
#endif

#ifndef FIXTURE_LIBRARY
static SvResult use_library(SvOwner owner)
{
    const void *table = NULL;
    const MathApi *api;
    int32_t sum = 0;
    STEP(plugins->find(owner, "test.library", 2, &table) == SV_INCOMPATIBLE && !table);
    STEP(plugins->find(owner, "test.nosuch", 1, &table) == SV_NOT_FOUND && !table);
    STEP(plugins->find(owner, NULL, 1, &table) == SV_INVALID && plugins->find(owner, "test.library", 1, NULL) == SV_INVALID);
    STEP(plugins->find(owner + 1, "test.library", 1, &table) == SV_STATE);
    STEP(plugins->find(owner, "test.library.extra", 1, &table) == SV_OK && table);
    STEP(plugins->find(owner, "test.library", 1, &table) == SV_OK);
    api = (const MathApi *)table;
    STEP(api->size == sizeof *api && api->version == 1);
    STEP(api->add(owner, 2, 3, &sum) == SV_OK && sum == 5);
    return host->log(owner, "consumer sum 5");
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *arguments)
{
    static const MathApi late = { sizeof(MathApi), 1, add };
    SvResult result = use_library(owner);
    (void)actor;
    (void)arguments;
    /* Outside the enable callback a plugin can neither offer an API nor give a reason. */
    STEP(plugins->provide(owner, "test.consumer", 1, &late) == SV_STATE && plugins->refuse(owner, "late") == SV_STATE);
    return result;
}
#endif

static SvResult SV_CALL enable(const SvHost *api, SvOwner owner)
{
    const void *table = NULL;
    host = api;
    mode = getenv("SV_PLUGIN_TEST_MODE");
    if (!mode)
        mode = "normal";
    STEP(host->get_api(owner, SV_API_PLUGINS, SV_PLUGINS_VERSION + 1, &table) == SV_INCOMPATIBLE && !table);
    STEP(host->get_api(owner, SV_API_PLUGINS, SV_PLUGINS_VERSION, &table) == SV_OK);
    plugins = (const SvPlugins *)table;
    STEP(plugins && plugins->size == sizeof *plugins && plugins->version == SV_PLUGINS_VERSION);
#ifdef FIXTURE_LIBRARY
    if (!strcmp(mode, "library-refuse")) {
        STEP(plugins->refuse(owner, "") == SV_INVALID && plugins->refuse(owner, NULL) == SV_INVALID);
        STEP(plugins->refuse(owner, "tab\there") == SV_INVALID && plugins->refuse(owner, "line\nbreak") == SV_INVALID);
        STEP(plugins->refuse(owner, "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789") == SV_INVALID);
        return SV_OK;
    }
    if (library_checks(owner) != SV_OK)
        return SV_FAILED;
#else
    if (!strcmp(mode, "refuse")) {
        STEP(plugins->refuse(owner, "needs a newer server") == SV_OK);
        return SV_FAILED;
    }
    /* A plugin that has no dependency on the library cannot find its API. */
    if (!strcmp(mode, "nodependency")) {
        char text[64];
        SvResult result = plugins->find(owner, "test.library", 1, &table);
        _snprintf(text, sizeof text, "find without dependency: %d", (int)result);
        text[sizeof text - 1] = 0;
        STEP(!table);
        STEP(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_NOT_FOUND);
        return host->log(owner, text);
    }
    STEP(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_NOT_FOUND);
    if (use_library(owner) != SV_OK)
        return SV_FAILED;
    STEP(host->command(owner, "consumer", SV_COMMAND_LOCAL, command) == SV_OK);
#endif
    return host->log(owner, FIXTURE_ID " enabled");
}

static void SV_CALL disable(SvOwner owner)
{
    host->log(owner, FIXTURE_ID " disabled");
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || plugin->size != sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, FIXTURE_ID);
    strcpy(plugin->version, FIXTURE_VERSION);
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    plugin->disable = disable;
    return SV_OK;
}
