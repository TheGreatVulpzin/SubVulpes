#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sv_plugin.h"
#include "sv_game_events.h"
#include "sv_config.h"
#include "sv_plugins_api.h"

static const SvHost *api;
static const char *mode;
static SvOwner identity;
static const SvConfig *config;

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    char path[MAX_PATH];
    (void)instance;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH && GetEnvironmentVariableA("SV_PLUGIN_TEST_MARKER", path, sizeof path)) {
        HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE)
            CloseHandle(file);
    }
    return TRUE;
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *arguments)
{
    (void)actor;
    (void)arguments;
    return api->log(owner, "fixture command");
}

static SvResult logged(SvOwner owner, const char *name, uint64_t actor)
{
    char text[96];
    _snprintf(text, sizeof text, "ran %s %llu", name, (unsigned long long)actor);
    text[sizeof text - 1] = 0;
    return api->log(owner, text);
}

static SvResult SV_CALL public_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    (void)arguments;
    return logged(owner, "perm_public", actor);
}

static SvResult SV_CALL admin_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    (void)arguments;
    return logged(owner, "perm_admin", actor);
}

static SvResult SV_CALL local_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    (void)arguments;
    return logged(owner, "perm_local", actor);
}

static DWORD WINAPI worker(void *unused)
{
    SvStatus status = { sizeof status };
    (void)unused;
    return api->status(identity, &status) == SV_STATE ? 0 : 1;
}

/* The settings of the plugin, in the modes that start with config. Each mode reads the keys that its manifest declares. */
static SvResult SV_CALL cfg_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    int32_t price = 0, enabled = 0, mode_index = 0, extra = 0, retyped = 0;
    float rate = 0.0f;
    char name[64] = "", token[64] = "", text[300];
    (void)actor;
    (void)arguments;
    if (config->get_int(owner, "price", &price) != SV_OK)
        return SV_FAILED;
    if (!strcmp(mode, "config-update")) {
        /* The manifest lost name, token, mode and enabled, and changed the type of rate to an integer. It gained extra. */
        if (config->get_int(owner, "rate", &retyped) != SV_OK || config->get_int(owner, "extra", &extra) != SV_OK ||
            config->get_text(owner, "name", name, sizeof name) != SV_NOT_FOUND)
            return SV_FAILED;
        _snprintf(text, sizeof text, "cfg price=%d rate=%d extra=%d", price, retyped, extra);
    } else {
        if (config->get_float(owner, "rate", &rate) != SV_OK || config->get_int(owner, "enabled", &enabled) != SV_OK ||
            config->get_text(owner, "name", name, sizeof name) != SV_OK || config->get_int(owner, "mode", &mode_index) != SV_OK ||
            config->get_text(owner, "token", token, sizeof token) != SV_OK)
            return SV_FAILED;
        _snprintf(text, sizeof text, "cfg price=%d rate=%.2f enabled=%d name=%s mode=%d token_len=%u", price, rate, enabled, name,
            mode_index, (unsigned)strlen(token));
    }
    text[sizeof text - 1] = 0;
    return api->log(owner, text);
}

/* The first mode checks every result code of the getters and the defaults. */
static SvResult config_checks(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    int32_t number = 0;
    float real = 0.0f;
    char text[64], small[10];
    if (host->get_api(owner, SV_API_CONFIG, SV_CONFIG_VERSION + 1, &table) != SV_INCOMPATIBLE || table != NULL ||
        host->get_api(owner, SV_API_CONFIG, 0, &table) != SV_INCOMPATIBLE || table != NULL ||
        host->get_api(owner, SV_API_PLUGINS, SV_PLUGINS_VERSION + 1, &table) != SV_INCOMPATIBLE || table != NULL ||
        host->get_api(owner, SV_API_CONFIG, SV_CONFIG_VERSION, &table) != SV_OK)
        return SV_FAILED;
    config = (const SvConfig *)table;
    if (!config || config->size != sizeof *config || config->version != SV_CONFIG_VERSION)
        return SV_FAILED;
    if (config->get_int(owner + 1, "price", &number) != SV_STATE || config->get_int(owner, "nosuch", &number) != SV_NOT_FOUND ||
        config->get_int(owner, NULL, &number) != SV_INVALID || config->get_int(owner, "price", NULL) != SV_INVALID ||
        config->get_int(owner, "rate", &number) != SV_INVALID || config->get_int(owner, "name", &number) != SV_INVALID ||
        config->get_float(owner, "price", &real) != SV_INVALID || config->get_float(owner, "rate", NULL) != SV_INVALID ||
        config->get_text(owner, "price", text, sizeof text) != SV_INVALID ||
        config->get_text(owner, "name", small, sizeof small) != SV_INVALID || config->get_text(owner, "name", NULL, sizeof text) != SV_INVALID ||
        config->get_text(owner, "nosuch", text, sizeof text) != SV_NOT_FOUND)
        return SV_FAILED;
    if (config->get_int(owner, "price", &number) != SV_OK || number != 125 || config->get_float(owner, "rate", &real) != SV_OK ||
        real != 1.5f || config->get_int(owner, "enabled", &number) != SV_OK || number != 1 ||
        config->get_text(owner, "name", text, sizeof text) != SV_OK || strcmp(text, "Name") ||
        config->get_int(owner, "mode", &number) != SV_OK || number != 1 ||
        config->get_text(owner, "mode", text, sizeof text) != SV_OK || strcmp(text, "normal") ||
        config->get_text(owner, "token", text, sizeof text) != SV_OK || text[0])
        return SV_FAILED;
    return SV_OK;
}

static SvResult SV_CALL event(SvOwner owner, const SvEvent *notification)
{
    unsigned unused;
    (void)owner;
    if (notification->kind == SV_EVENT_CONFIG) {
        /* A live setting changed. The log shows the key and the two values, which are empty for a secret. */
        const SvConfigEvent *change = (const SvConfigEvent *)notification;
        char text[200];
        if (notification->size != sizeof *change || change->key[SV_CONFIG_KEY_MAX] || change->old_value[SV_CONFIG_TEXT_MAX] ||
            change->new_value[SV_CONFIG_TEXT_MAX])
            return SV_FAILED;
        _snprintf(text, sizeof text, "CONFIG %s [%s] [%s]", change->key, change->old_value, change->new_value);
        text[sizeof text - 1] = 0;
        return api->log(owner, text);
    }
    if (notification->kind == SV_EVENT_DAMAGE) {
        /* A decision inside the simulation. A slow answer for 99, a failure in the mode unsafe-fail, and the guard on 88: a
         * call that changes the host must give SV_STATE there. */
        const SvDamageEvent *damage = (const SvDamageEvent *)notification;
        if (notification->size != sizeof *damage || !strcmp(mode, "unsafe-fail"))
            return SV_FAILED;
        if (damage->amount == 99)
            Sleep(2);
        if (damage->amount == 88 && (api->command(owner, "late", SV_COMMAND_PUBLIC, command) != SV_STATE ||
                                     api->subscribe(owner, SV_EVENT_TICK, event) != SV_STATE))
            return SV_FAILED;
        return SV_OK;
    }
    if (notification->kind == SV_EVENT_EXT(3))
        return api->log(owner, "ext event 3");
    if (notification->size != sizeof *notification)
        return SV_INVALID;
    if (notification->kind == SV_EVENT_STOP)
        return api->log(owner, "fixture stop event");
    if (notification->kind == SV_EVENT_TICK && !strcmp(mode, "event-fail"))
        return SV_FAILED;
    if (!strcmp(mode, "fpu"))
        _controlfp_s(&unused, _PC_24 | _RC_DOWN, _MCW_PC | _MCW_RC);
    return SV_OK;
}

static SvResult SV_CALL enable(const SvHost *host, SvOwner owner)
{
    HANDLE thread;
    DWORD exit_code = 1;
    SvStatus status = { sizeof status };
    const char *command_name;
    api = host;
    identity = owner;
    mode = getenv("SV_PLUGIN_TEST_MODE");
    if (!mode)
        mode = "normal";
    if (api->status(owner + 99, &status) != SV_STATE || api->status(owner, NULL) != SV_INVALID ||
        api->status(owner, &status) != SV_OK || status.tick != 42 ||
        api->command(owner, "../bad", SV_COMMAND_PUBLIC, command) != SV_INVALID ||
        api->subscribe(owner, 0x80000000u, event) != SV_INVALID)
        return SV_FAILED;
    thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (!thread)
        return SV_FAILED;
    if (WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0)
        GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);
    if (exit_code)
        return SV_FAILED;
    command_name = !strcmp(mode, "conflict") ? "status" : "fixture";
    if (api->command(owner, command_name, SV_COMMAND_LOCAL, command) != SV_OK)
        return SV_CONFLICT;
    if (api->command(owner, command_name, SV_COMMAND_LOCAL, command) != SV_CONFLICT)
        return SV_FAILED;
    if (api->command(owner, "plugins", SV_COMMAND_PUBLIC, command) != SV_INVALID ||
        api->command(owner, "bad_permission", SV_COMMAND_LOCAL + 1, command) != SV_INVALID ||
        api->command(owner, "no_callback", SV_COMMAND_PUBLIC, NULL) != SV_INVALID ||
        api->subscribe(owner, SV_EVENT_TICK, NULL) != SV_INVALID)
        return SV_FAILED;
    if (!strncmp(mode, "config", 6)) {
        if ((!strcmp(mode, "config") && config_checks(api, owner) != SV_OK) ||
            api->get_api(owner, SV_API_CONFIG, SV_CONFIG_VERSION, (const void **)&config) != SV_OK ||
            api->command(owner, "cfg", SV_COMMAND_LOCAL, cfg_command) != SV_OK ||
            api->subscribe(owner, SV_EVENT_TICK | SV_EVENT_STOP | SV_EVENT_CONFIG, event) != SV_OK)
            return SV_FAILED;
    } else if (api->subscribe(owner, SV_EVENT_TICK | SV_EVENT_STOP, event) != SV_OK) {
        return SV_FAILED;
    }
    /* Decisions inside the simulation need unsafe_events=1 in the manifest. Only the unsafe modes have it. */
    if (!strncmp(mode, "unsafe", 6)) {
        if (api->subscribe(owner, SV_EVENT_TICK | SV_EVENT_STOP | SV_EVENT_DAMAGE, event) != SV_OK)
            return SV_FAILED;
    } else if (api->subscribe(owner, SV_EVENT_DAMAGE, event) != SV_DENIED) {
        return SV_FAILED;
    }
    if (!strcmp(mode, "resubscribe") && api->subscribe(owner, 0, NULL) != SV_OK)
        return SV_FAILED;
    /* The capability wall: a manifest that declares only "config" opens that table, and it refuses the others. */
    if (!strcmp(mode, "wall")) {
        const void *table = NULL;
        if (api->get_api(owner, 1u, 1u, &table) != SV_DENIED || table ||
            api->get_api(owner, SV_API_PLUGINS, SV_PLUGINS_VERSION, &table) != SV_DENIED || table ||
            api->get_api(owner, SV_API_CONFIG, SV_CONFIG_VERSION, &table) != SV_OK || !table)
            return SV_FAILED;
    }
    /* The 64-bit mask: the numbered event 3 arrives, the others do not, and a bad low bit changes nothing. */
    if (!strcmp(mode, "ext")) {
        if (api->subscribe(owner, SV_EVENT_TICK | SV_EVENT_STOP | SV_EVENT_EXT_BIT(3), event) != SV_OK ||
            api->subscribe(owner, 0x1000000u, event) != SV_INVALID ||
            api->subscribe(owner, SV_EVENT_TICK | SV_EVENT_EXT_BIT(3), NULL) != SV_INVALID)
            return SV_FAILED;
    }
    if (!strcmp(mode, "permissions") &&
        (api->command(owner, "perm_public", SV_COMMAND_PUBLIC, public_command) != SV_OK ||
         api->command(owner, "perm_admin", SV_COMMAND_ADMIN, admin_command) != SV_OK ||
         api->command(owner, "perm_local", SV_COMMAND_LOCAL, local_command) != SV_OK))
        return SV_FAILED;
    if (!strcmp(mode, "limit")) {
        char name[SV_PLUGIN_ID_SIZE];
        unsigned count = 1;
        SvResult result;
        for (;;) {
            _snprintf(name, sizeof name, "lim%u", count - 1);
            name[sizeof name - 1] = 0;
            result = api->command(owner, name, SV_COMMAND_PUBLIC, command);
            if (result != SV_OK)
                break;
            ++count;
        }
        if (result != SV_LIMIT)
            return SV_FAILED;
        _snprintf(name, sizeof name, "commands registered %u", count);
        name[sizeof name - 1] = 0;
        api->log(owner, name);
    }
    if (!strcmp(mode, "enable-fail"))
        return SV_FAILED;
    return api->log(owner, "fixture enabled");
}

static void SV_CALL disable(SvOwner owner)
{
    api->log(owner, "fixture disabled");
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    const char *entry_mode = getenv("SV_PLUGIN_TEST_MODE");
    if (abi != SV_PLUGIN_ABI || plugin->size != sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "test.fixture");
    strcpy(plugin->version, "0.1.0");
    plugin->flags = entry_mode && !strcmp(entry_mode, "restart") ? 0u : SV_PLUGIN_LIVE;
    plugin->enable = enable;
    plugin->disable = disable;
    return SV_OK;
}
