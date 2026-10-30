#ifndef SV_PLUGIN_CAPS_H
#define SV_PLUGIN_CAPS_H

enum { SV_CAP_COUNT = 12 };

typedef struct {
    const char *name, *level, *text;
} SvCapabilityInfo;

/* plugin_manifest.c owns this table. Each row uses bit (1u << row). */
extern const SvCapabilityInfo sv_plugin_caps[SV_CAP_COUNT];

#endif
