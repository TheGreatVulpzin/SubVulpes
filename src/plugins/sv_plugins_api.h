#ifndef SV_PLUGINS_API_H
#define SV_PLUGINS_API_H

#include "sv_plugin.h"

/* The services between plugins. See docs/plugin-libraries.md.
 *
 * A plugin can offer an API of its own to the plugins that depend on it, and it can refuse to start with a reason that the
 * administrator reads. The host never reads or calls the table of an API. It only passes the pointer. */
#define SV_API_PLUGINS 12u
#define SV_PLUGINS_VERSION 1u
#define SV_PLUGINS_API_MAX 64
#define SV_PLUGINS_API_PER_PLUGIN 8

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
    /* Gives the reason for a refusal to start. Call it inside the enable callback, then return a result other than SV_OK.
     * The host shows "Refused to start: REASON" in the plugin list, the log and the launcher. The reason has 1 to 95
     * printable ASCII characters. SV_STATE outside the enable callback, SV_INVALID for a bad reason. */
    SvResult (SV_CALL *refuse)(SvOwner owner, const char *reason);
    /* Offers an API to the plugins that depend on this one. Call it inside the enable callback. The name is the ID of the
     * plugin, or the ID and a dot and a suffix. The version is the version of the contract of the table, at least 1. The
     * table must start with two uint32 fields, size and version, and the version must be equal to the version given here.
     * It must stay valid until the disable callback ends. At most SV_PLUGINS_API_PER_PLUGIN names for each plugin.
     * SV_DENIED for a name of another plugin, SV_CONFLICT for a name that exists, SV_LIMIT for a full table. */
    SvResult (SV_CALL *provide)(SvOwner owner, const char *name, uint32_t version, const void *table);
    /* Finds the API of another plugin. The caller must list the plugin that offers it in its dependencies, so the offering
     * plugin loads first and stops last. SV_DEPENDENCY when it does not. SV_NOT_FOUND for an unknown name or a plugin that is
     * not active, SV_INCOMPATIBLE when the version differs from the version of the table. The table stays valid while the
     * caller is active.
     *
     * A function of the table runs with the owner of the caller. Give the owner of the caller as the first argument of every
     * function that needs the host, and use only that owner for host calls. */
    SvResult (SV_CALL *find)(SvOwner owner, const char *name, uint32_t version, const void **table);
    /* Like SvHost.subscribe, with a 64-bit mask. The bits 0 to 31 are the events of SvHost.subscribe. The bits 32 to 63 are
     * the numbered events (SV_EVENT_EXT_BIT). It replaces the subscription of the plugin, whatever call set it before. */
    SvResult (SV_CALL *subscribe64)(SvOwner owner, uint64_t events, SvEventFn callback);
} SvPlugins;
#pragma pack(pop)

#endif
