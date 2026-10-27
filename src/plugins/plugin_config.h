#ifndef SV_PLUGIN_CONFIG_H
#define SV_PLUGIN_CONFIG_H

#include "sv_config.h"
#include <stddef.h>

enum { SV_SETTING_MAX = 32, SV_CHOICE_MAX = 8, SV_CHOICE_SIZE = 24 };

/* One declared setting and its values. Every value is kept as text in its canonical form: a flag is 0 or 1, a number has no
 * extra characters, a choice is its name. */
typedef struct {
    char key[SV_CONFIG_KEY_MAX + 1];
    uint32_t type, live, admin, choice_count, text_max;
    int32_t imin, imax;
    float fmin, fmax;
    char label[64], help[128];
    char choices[SV_CHOICE_MAX][SV_CHOICE_SIZE];
    char def[SV_CONFIG_TEXT_MAX + 1];
    char stored[SV_CONFIG_TEXT_MAX + 1];
    char effective[SV_CONFIG_TEXT_MAX + 1];
} SvSetting;

typedef struct {
    uint32_t count;
    SvSetting item[SV_SETTING_MAX];
} SvSettingSet;

/* Reads the sections [setting.KEY] of a manifest. Returns 0 when a declaration is wrong. Every setting starts at its
 * default. */
int sv_config_declare(SvSettingSet *set, const char *manifest);
/* Reads the saved values of a plugin from the private folder. A saved value that fails the check of the declaration, or that
 * was saved under another type, gives way to the default. */
void sv_config_load(SvSettingSet *set, const char *id);
const SvSetting *sv_config_find(const SvSettingSet *set, const char *key);
/* Checks a text against a setting and gives its canonical form. SV_INVALID when it does not fit. */
SvResult sv_config_check(const SvSetting *setting, const char *text, char canonical[SV_CONFIG_TEXT_MAX + 1]);
/* Checks, saves and, for a live setting, applies a value. The saved file changes in one step, so a crash leaves the old file.
 * by_admin is 1 for a player: the key must allow it (SV_DENIED). *live says that the plugin sees the value at once. The old
 * and the new text go out for the change event. SV_NOT_FOUND, SV_INVALID or SV_IO leave everything as it was. */
SvResult sv_config_store(SvSettingSet *set, const char *id, const char *key, const char *text, int by_admin, int *live,
    char old_text[SV_CONFIG_TEXT_MAX + 1], char new_text[SV_CONFIG_TEXT_MAX + 1]);
/* A line for the listing: tabs between the fields, and a secret shows * when it has a value. */
void sv_config_line(const char *id, const SvSetting *setting, char *line, size_t size);

#endif
