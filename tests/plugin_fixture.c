#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include "sv_plugin.h"

static const SvHost *api;
static const char *mode;
static SvOwner identity;

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

static DWORD WINAPI worker(void *unused)
{
    SvStatus status = { sizeof status };
    (void)unused;
    return api->status(identity, &status) == SV_STATE ? 0 : 1;
}

static SvResult SV_CALL event(SvOwner owner, const SvEvent *notification)
{
    unsigned unused;
    (void)owner;
    if (notification->size != sizeof *notification)
        return SV_INVALID;
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
    if (api->subscribe(owner, SV_EVENT_TICK | SV_EVENT_STOP, event) != SV_OK)
        return SV_FAILED;
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
