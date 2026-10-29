#ifndef SV_PLUGIN_INTERNAL_H
#define SV_PLUGIN_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "plugin_host.h"
#include "plugin_config.h"
#include "plugin_manifest.h"

typedef struct {
    char id[SV_PLUGIN_ID_SIZE];
    char version[SV_PLUGIN_VERSION_SIZE];
    char path[MAX_PATH];
    char hash[65];
    uint32_t runtime, cap_mask;
    char name[49], description[161], author[49], homepage[97];
    char dependencies[SV_PLUGIN_DEP_MAX][SV_PLUGIN_ID_SIZE];
    /* The lowest package version that the module accepts for each dependency. An empty text accepts any version. */
    char dependency_min[SV_PLUGIN_DEP_MAX][SV_PLUGIN_VERSION_SIZE];
    uint32_t dependency_count;
    /* The plugins that cannot be active at the same time as this one. */
    char conflicts[SV_PLUGIN_DEP_MAX][SV_PLUGIN_ID_SIZE];
    uint32_t conflict_count;
    /* 1 for a library: it offers an API to other plugins and has no game behavior of its own. */
    uint32_t library_role;
    /* The reason that the plugin gave when it refused to start. */
    char refusal[SV_PLUGIN_REASON_SIZE];
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
    uint64_t events;
    uint32_t enabling;
    uint32_t unsafe;
    uint32_t slow_count;
    uint64_t slow_start_ms;
    const char *doom;
    SvSettingSet config;
    uint32_t visiting;
    uint64_t calls;
    uint64_t elapsed_us;
} SvModule;

int sv_plugin_id(const char *text);
/* 1 when the text is a version to compare: digits in groups, with one dot between two groups. */
int sv_plugin_version_min(const char *text);
/* 1 when the package version "have" is not lower than the version "want". The groups of digits at the start of "have"
 * count, and a missing group is 0. A version that does not start with digits is lower than every other. */
int sv_plugin_version_at_least(const char *have, const char *want);
int sv_plugin_profile(SvModule *modules, uint32_t *count);
SvResult sv_plugin_load(SvModule *module);
void sv_plugin_unload(SvModule *module);
int sv_plugin_inspect(const char *id, SvModule *module);
int sv_plugin_declared(const char *id, SvModule *module);
int sv_plugin_validate_folder(const char *folder);
int sv_plugin_approved(const SvModule *module);
int sv_plugin_approve(const char *id, const char *hash);
int sv_plugin_save_state(const char *id, int enabled);
HANDLE sv_plugin_lock(void);
/* The private settings file of a plugin: user/plugins/ID.settings.ini. With create, the folders are made. */
int sv_plugin_private_file(const char *id, char path[MAX_PATH], int create);

#endif
