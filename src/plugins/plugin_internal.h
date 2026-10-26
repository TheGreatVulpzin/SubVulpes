#ifndef SV_PLUGIN_INTERNAL_H
#define SV_PLUGIN_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "plugin_host.h"

typedef struct {
    char id[SV_PLUGIN_ID_SIZE];
    char version[SV_PLUGIN_VERSION_SIZE];
    char path[MAX_PATH];
    char hash[65];
    char dependencies[SV_PLUGIN_DEP_MAX][SV_PLUGIN_ID_SIZE];
    uint32_t dependency_count;
    uint32_t metadata_valid;
    uint32_t required;
    uint32_t wanted;
    uint32_t live;
    SvModuleState state;
    char reason[128];
    HMODULE library;
    uint32_t validated;
    HANDLE lock;
    SvPlugin plugin;
    SvEventFn event_callback;
    uint32_t events;
    uint32_t enabling;
    uint32_t visiting;
    uint64_t calls;
    uint64_t elapsed_us;
} SvModule;

int sv_plugin_id(const char *text);
int sv_plugin_profile(SvModule *modules, uint32_t *count);
SvResult sv_plugin_load(SvModule *module);
void sv_plugin_unload(SvModule *module);
int sv_plugin_inspect(const char *id, SvModule *module);
int sv_plugin_approved(const SvModule *module);
int sv_plugin_approve(const char *id, const char *hash);
int sv_plugin_save_state(const char *id, int enabled);
HANDLE sv_plugin_lock(void);

#endif
