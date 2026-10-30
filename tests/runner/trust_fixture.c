#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include "sv_plugin.h"

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    char path[MAX_PATH];
    (void)instance;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH &&
        GetEnvironmentVariableA("SV_PLUGIN_TRUST_MARKER", path, sizeof path)) {
        HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE)
            CloseHandle(file);
    }
    return TRUE;
}

static SvResult SV_CALL enable(const SvHost *host, SvOwner owner)
{
    const char *wait = getenv("SV_PLUGIN_TRUST_WAIT");
    if (wait && !strcmp(wait, "1"))
        Sleep(62000);
    return host->log(owner, "Trust fixture enabled.");
}

static void SV_CALL disable(SvOwner owner)
{
    (void)owner;
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || plugin->size != sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "test.trust");
    strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    plugin->disable = disable;
    return SV_OK;
}
