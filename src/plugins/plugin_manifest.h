#ifndef SV_PLUGIN_MANIFEST_H
#define SV_PLUGIN_MANIFEST_H

#include "plugin_config.h"

enum { SV_RUNTIME_NATIVE, SV_RUNTIME_LUA, SV_RUNTIME_WASM };

typedef struct {
    char id[48], version[24], library[48];
    uint32_t runtime, live, unsafe, library_role, cap_mask;
    char name[49], description[161], author[49], homepage[97];
    char dependencies[8][48], dependency_min[8][24], conflicts[8][48];
    uint32_t dependency_count, conflict_count;
    SvSettingSet config;
} SvManifest;

typedef void (*SvManifestReport)(void *context, const char *key, const char *message);

/* The report contains fixed messages and never includes manifest values. */
int sv_manifest_read(const char *absolute_path, SvManifest *out, SvManifestReport report, void *context);
const char *sv_manifest_runtime_name(uint32_t runtime);
/* Returns 0 when the buffer cannot hold the complete list. */
int sv_manifest_caps_text(uint32_t mask, uint32_t unsafe, char *out, size_t capacity);

#endif
