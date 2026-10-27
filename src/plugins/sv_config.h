#ifndef SV_CONFIG_H
#define SV_CONFIG_H

#include "sv_plugin.h"

/* The settings of one plugin, set by the administrator. See docs/plugin-config.md.
 *
 * The plugin declares its settings in its plugin.ini, and the host checks every value before the plugin sees it. A plugin
 * reads only its own settings, so the owner of the call picks the table. The getters work in any host callback. */
#define SV_API_CONFIG 11u
#define SV_CONFIG_VERSION 1u
#define SV_CONFIG_KEY_MAX 32
#define SV_CONFIG_TEXT_MAX 63

/* The type of a setting, as the manifest names it: flag, int, float, text, choice and secret. */
enum { SV_SETTING_FLAG = 1, SV_SETTING_INT, SV_SETTING_FLOAT, SV_SETTING_TEXT, SV_SETTING_CHOICE, SV_SETTING_SECRET };

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
    /* A flag (0 or 1), an integer, or the index of a choice (counted from 0). SV_NOT_FOUND for an unknown key,
     * SV_INVALID for another type or a null pointer. */
    SvResult (SV_CALL *get_int)(SvOwner owner, const char *key, int32_t *value);
    SvResult (SV_CALL *get_float)(SvOwner owner, const char *key, float *value);
    /* A text, a secret, or the name of a choice, with a final zero. The capacity must be at least 64. */
    SvResult (SV_CALL *get_text)(SvOwner owner, const char *key, char *buffer, uint32_t capacity);
} SvConfig;

/* The change event of a live setting (SV_EVENT_CONFIG). The values are the text of the setting. For a secret both are empty,
 * and the plugin reads the new value with get_text. The host gives the event to the owner only. */
typedef struct {
    SvEvent base;
    char key[SV_CONFIG_KEY_MAX + 1];
    char old_value[SV_CONFIG_TEXT_MAX + 1];
    char new_value[SV_CONFIG_TEXT_MAX + 1];
} SvConfigEvent;
#pragma pack(pop)

#endif
