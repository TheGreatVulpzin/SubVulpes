/* The settings of the plugins, set by the administrator. See docs/plugin-config.md.
 *
 * A plugin declares its settings in its plugin.ini, in sections named [setting.KEY]. The host reads them without loading the
 * DLL, checks each value against the declaration, and keeps the values in a private file that updates do not touch. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plugin_config.h"

/* Provided by plugin_files.c: the path of the private settings file of a plugin. */
int sv_plugin_private_file(const char *id, char path[MAX_PATH], int create);

static const char *type_names[] = { "", "flag", "int", "float", "text", "choice", "secret" };

static int key_ok(const char *key)
{
    size_t i, length = strlen(key);
    if (!length || length > SV_CONFIG_KEY_MAX || !(key[0] >= 'a' && key[0] <= 'z'))
        return 0;
    for (i = 0; i < length; ++i)
        if (!(key[i] >= 'a' && key[i] <= 'z') && !(key[i] >= '0' && key[i] <= '9') && key[i] != '_')
            return 0;
    return 1;
}

/* A text value: printable ASCII with no quote and no semicolon (the ini file would change them), and no space at the ends. */
static int text_ok(const char *text, size_t most)
{
    size_t i, length = strlen(text);
    if (length > most)
        return 0;
    for (i = 0; i < length; ++i)
        if ((unsigned char)text[i] < 0x20 || (unsigned char)text[i] > 0x7e || text[i] == '"' || text[i] == ';')
            return 0;
    return !length || (text[0] != ' ' && text[length - 1] != ' ');
}

static int parse_int(const char *text, int32_t *out)
{
    size_t i = 0, length = strlen(text);
    long long value = 0;
    int negative = 0;
    if (length && text[0] == '-') {
        negative = 1;
        i = 1;
    }
    if (i >= length || length - i > 10)
        return 0;
    for (; i < length; ++i) {
        if (text[i] < '0' || text[i] > '9')
            return 0;
        value = value * 10 + (text[i] - '0');
    }
    if (negative)
        value = -value;
    if (value < INT32_MIN || value > INT32_MAX)
        return 0;
    *out = (int32_t)value;
    return 1;
}

/* A plain decimal number: an optional minus, digits, and at most one point with digits after it. */
static int parse_float(const char *text, float *out)
{
    size_t i = 0, length = strlen(text);
    int digits = 0, point = 0;
    double value;
    if (!length || length > 24)
        return 0;
    if (text[0] == '-')
        i = 1;
    for (; i < length; ++i) {
        if (text[i] >= '0' && text[i] <= '9') {
            ++digits;
        } else if (text[i] == '.' && !point && digits) {
            point = 1;
            digits = 0;
        } else {
            return 0;
        }
    }
    if (!digits)
        return 0;
    value = strtod(text, NULL);
    if (!(value >= -3.0e38 && value <= 3.0e38))
        return 0;
    *out = (float)value;
    return 1;
}

SvResult sv_config_check(const SvSetting *setting, const char *text, char canonical[SV_CONFIG_TEXT_MAX + 1])
{
    int32_t number;
    float real;
    uint32_t i;
    if (!setting || !text || !canonical)
        return SV_INVALID;
    switch (setting->type) {
    case SV_SETTING_FLAG:
        if (strcmp(text, "0") && strcmp(text, "1"))
            return SV_INVALID;
        strcpy(canonical, text);
        return SV_OK;
    case SV_SETTING_INT:
        if (!parse_int(text, &number) || number < setting->imin || number > setting->imax)
            return SV_INVALID;
        snprintf(canonical, SV_CONFIG_TEXT_MAX + 1, "%d", number);
        return SV_OK;
    case SV_SETTING_FLOAT:
        if (!parse_float(text, &real) || real < setting->fmin || real > setting->fmax)
            return SV_INVALID;
        snprintf(canonical, SV_CONFIG_TEXT_MAX + 1, "%.9g", real);
        return SV_OK;
    case SV_SETTING_CHOICE:
        for (i = 0; i < setting->choice_count; ++i)
            if (!strcmp(setting->choices[i], text)) {
                strcpy(canonical, text);
                return SV_OK;
            }
        return SV_INVALID;
    case SV_SETTING_TEXT:
    case SV_SETTING_SECRET:
        if (!text_ok(text, setting->text_max))
            return SV_INVALID;
        strcpy(canonical, text);
        return SV_OK;
    }
    return SV_INVALID;
}

static int read_value(const char *section, const char *name, const char *manifest, char *out, DWORD size)
{
    DWORD n = GetPrivateProfileStringA(section, name, "", out, size, manifest);
    return n < size - 1;
}

/* A list of names with commas, for a choice: each name is 1 to 23 of lowercase letters, digits, underscore and hyphen. */
static int parse_choices(SvSetting *setting, char *list)
{
    char *part = list, *next;
    uint32_t i, k;
    for (;;) {
        size_t length;
        next = strchr(part, ',');
        if (next)
            *next = 0;
        length = strlen(part);
        if (!length || length >= SV_CHOICE_SIZE || setting->choice_count >= SV_CHOICE_MAX)
            return 0;
        for (k = 0; k < length; ++k)
            if (!(part[k] >= 'a' && part[k] <= 'z') && !(part[k] >= '0' && part[k] <= '9') && part[k] != '_' && part[k] != '-')
                return 0;
        for (i = 0; i < setting->choice_count; ++i)
            if (!strcmp(setting->choices[i], part))
                return 0;
        strcpy(setting->choices[setting->choice_count++], part);
        if (!next)
            break;
        part = next + 1;
    }
    return setting->choice_count >= 2;
}

/* Each message names a rule and omits the value, which can contain a secret. */
static int declaration_invalid(const char **reason, const char *message)
{
    *reason = message;
    return 0;
}

/* The declaration of one setting. section is [setting.KEY]. */
static int declare_one(SvSetting *setting, const char *section, const char *manifest, const char **reason)
{
    char text[256], low[64], high[64], first[64];
    int32_t number = 0;
    float real = 0.0f;
    memset(setting, 0, sizeof *setting);
    if (strlen(section) < 9 || !key_ok(section + 8))
        return declaration_invalid(reason, "The setting key is invalid.");
    strcpy(setting->key, section + 8);
    if (!read_value(section, "type", manifest, text, sizeof text))
        return declaration_invalid(reason, "The setting type exceeds its size limit.");
    for (setting->type = SV_SETTING_FLAG; setting->type <= SV_SETTING_SECRET; ++setting->type)
        if (!strcmp(text, type_names[setting->type]))
            break;
    if (setting->type > SV_SETTING_SECRET)
        return declaration_invalid(reason, "The setting type is missing or invalid.");
    if (!read_value(section, "live", manifest, text, sizeof text) || (text[0] && strcmp(text, "0") && strcmp(text, "1")))
        return declaration_invalid(reason, "The setting live flag must be 0 or 1.");
    setting->live = text[0] == '1';
    if (!read_value(section, "admin", manifest, text, sizeof text) || (text[0] && strcmp(text, "0") && strcmp(text, "1")))
        return declaration_invalid(reason, "The setting admin flag must be 0 or 1.");
    setting->admin = text[0] == '1';
    if (!read_value(section, "label", manifest, setting->label, sizeof setting->label) || !setting->label[0] ||
        !text_ok(setting->label, 63))
        return declaration_invalid(reason, "The setting label is missing or invalid.");
    if (!read_value(section, "help", manifest, setting->help, sizeof setting->help) || !text_ok(setting->help, 127))
        return declaration_invalid(reason, "The setting help text is invalid.");
    if (!read_value(section, "default", manifest, first, sizeof first) || !read_value(section, "min", manifest, low, sizeof low) ||
        !read_value(section, "max", manifest, high, sizeof high))
        return declaration_invalid(reason, "The setting default or range exceeds its size limit.");
    switch (setting->type) {
    case SV_SETTING_FLAG:
        if (strcmp(first, "0") && strcmp(first, "1"))
            return declaration_invalid(reason, "The setting default flag must be 0 or 1.");
        break;
    case SV_SETTING_INT:
        if (!parse_int(low, &setting->imin) || !parse_int(high, &setting->imax) || setting->imin > setting->imax ||
            !parse_int(first, &number))
            return declaration_invalid(reason, "The setting integer default or range is invalid.");
        break;
    case SV_SETTING_FLOAT:
        if (!parse_float(low, &setting->fmin) || !parse_float(high, &setting->fmax) || setting->fmin > setting->fmax ||
            !parse_float(first, &real))
            return declaration_invalid(reason, "The setting decimal default or range is invalid.");
        break;
    case SV_SETTING_TEXT:
    case SV_SETTING_SECRET:
        if (!read_value(section, "maxlen", manifest, text, sizeof text))
            return declaration_invalid(reason, "The setting text limit exceeds its size limit.");
        setting->text_max = SV_CONFIG_TEXT_MAX;
        if (text[0] && (!parse_int(text, &number) || number < 1 || number > SV_CONFIG_TEXT_MAX))
            return declaration_invalid(reason, "The setting text limit is invalid.");
        if (text[0])
            setting->text_max = (uint32_t)number;
        /* A secret has no default, so the file of the plugin never holds one. */
        if (setting->type == SV_SETTING_SECRET && first[0])
            return declaration_invalid(reason, "A secret setting cannot declare a default value.");
        break;
    case SV_SETTING_CHOICE:
        if (!read_value(section, "choices", manifest, text, sizeof text) || !parse_choices(setting, text))
            return declaration_invalid(reason, "The setting choice list is missing or invalid.");
        break;
    }
    if (sv_config_check(setting, first, setting->def) != SV_OK)
        return declaration_invalid(reason, "The setting default does not match its type or range.");
    strcpy(setting->stored, setting->def);
    strcpy(setting->effective, setting->def);
    return 1;
}

static void declaration_problem(char *reason, size_t capacity, SvConfigReport report, void *context,
    const char *section, const char *message)
{
    char key[8 + SV_CONFIG_KEY_MAX + 1];
    const char *name = "settings";
    if (section && strlen(section) >= 9 && key_ok(section + 8)) {
        snprintf(key, sizeof key, "setting.%s", section + 8);
        name = key;
    }
    if (reason && capacity && !reason[0])
        snprintf(reason, capacity, "%s", message);
    if (report)
        report(context, name, message);
}

static int declare_settings(const char *manifest, SvSettingSet *set, char *reason, size_t capacity,
    SvConfigReport report, void *context, int collect)
{
    static char names[16384];
    char *name;
    DWORD n;
    uint32_t i, declarations = 0;
    int valid = 1;
    if (reason && capacity)
        reason[0] = 0;
    if (!set || !manifest) {
        declaration_problem(reason, capacity, report, context, NULL, "The manifest path and settings output are required.");
        return 0;
    }
    memset(set, 0, sizeof *set);
    n = GetPrivateProfileSectionNamesA(names, sizeof names, manifest);
    if (n >= sizeof names - 2) {
        declaration_problem(reason, capacity, report, context, NULL, "The manifest section list exceeds its size limit.");
        return 0;
    }
    for (name = names; *name; name += strlen(name) + 1) {
        SvSetting setting;
        const char *message = NULL;
        int declared;
        if (_strnicmp(name, "setting.", 8))
            continue;
        ++declarations;
        if (declarations > SV_SETTING_MAX) {
            declaration_problem(reason, capacity, report, context, NULL, "The manifest declares more than 32 settings.");
            valid = 0;
            if (!collect)
                return 0;
        }
        declared = declare_one(&setting, name, manifest, &message);
        if (!declared) {
            declaration_problem(reason, capacity, report, context, name, message);
            valid = 0;
            if (!collect)
                return 0;
            continue;
        }
        for (i = 0; i < set->count; ++i)
            if (!strcmp(set->item[i].key, setting.key)) {
                declaration_problem(reason, capacity, report, context, name, "The manifest repeats a setting key.");
                valid = 0;
                break;
            }
        if (i < set->count) {
            if (!collect)
                return 0;
            continue;
        }
        if (declarations <= SV_SETTING_MAX)
            set->item[set->count++] = setting;
    }
    return valid;
}

int sv_config_declare_report(const char *manifest, SvSettingSet *set, char *reason, size_t capacity,
    SvConfigReport report, void *context)
{
    return declare_settings(manifest, set, reason, capacity, report, context, 1);
}

int sv_config_declare(SvSettingSet *set, const char *manifest)
{
    return declare_settings(manifest, set, NULL, 0, NULL, NULL, 0);
}

const SvSetting *sv_config_find(const SvSettingSet *set, const char *key)
{
    uint32_t i;
    if (!set || !key)
        return NULL;
    for (i = 0; i < set->count; ++i)
        if (!strcmp(set->item[i].key, key))
            return &set->item[i];
    return NULL;
}

void sv_config_load(SvSettingSet *set, const char *id)
{
    char path[MAX_PATH], text[256], type[16], canonical[SV_CONFIG_TEXT_MAX + 1];
    uint32_t i;
    for (i = 0; i < set->count; ++i) {
        strcpy(set->item[i].stored, set->item[i].def);
        strcpy(set->item[i].effective, set->item[i].def);
    }
    if (!set->count || !sv_plugin_private_file(id, path, 0) || GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
        return;
    for (i = 0; i < set->count; ++i) {
        SvSetting *setting = &set->item[i];
        DWORD n = GetPrivateProfileStringA("types", setting->key, "", type, sizeof type, path);
        /* A key saved under another type goes back to its default. */
        if (!n || strcmp(type, type_names[setting->type]))
            continue;
        n = GetPrivateProfileStringA("settings", setting->key, "\x01", text, sizeof text, path);
        if (n >= sizeof text - 1 || text[0] == '\x01')
            continue;
        if (sv_config_check(setting, text, canonical) == SV_OK) {
            strcpy(setting->stored, canonical);
            strcpy(setting->effective, canonical);
        }
    }
}

/* Writes the whole file to a new file and replaces the old one, so a crash leaves the old file whole. */
static int write_file(const SvSettingSet *set, const char *id)
{
    char path[MAX_PATH], temporary[MAX_PATH + 8], line[160];
    HANDLE file;
    DWORD written;
    uint32_t i;
    if (!sv_plugin_private_file(id, path, 1) || strlen(path) + 5 >= sizeof temporary)
        return 0;
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
#define PUT(text) do { DWORD length = (DWORD)strlen(text); \
    if (!WriteFile(file, text, length, &written, NULL) || written != length) goto fail; } while (0)
    PUT("[types]\r\n");
    for (i = 0; i < set->count; ++i) {
        snprintf(line, sizeof line, "%s=%s\r\n", set->item[i].key, type_names[set->item[i].type]);
        PUT(line);
    }
    PUT("[settings]\r\n");
    for (i = 0; i < set->count; ++i) {
        snprintf(line, sizeof line, "%s=%s\r\n", set->item[i].key, set->item[i].stored);
        PUT(line);
    }
#undef PUT
    if (!FlushFileBuffers(file))
        goto fail;
    CloseHandle(file);
    if (!MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(temporary);
        return 0;
    }
    return 1;
fail:
    CloseHandle(file);
    DeleteFileA(temporary);
    return 0;
}

SvResult sv_config_store(SvSettingSet *set, const char *id, const char *key, const char *text, int by_admin, int *live,
    char old_text[SV_CONFIG_TEXT_MAX + 1], char new_text[SV_CONFIG_TEXT_MAX + 1])
{
    SvSetting *setting = NULL;
    char canonical[SV_CONFIG_TEXT_MAX + 1], saved[SV_CONFIG_TEXT_MAX + 1];
    uint32_t i;
    if (!set || !id || !key || !live || !old_text || !new_text)
        return SV_INVALID;
    for (i = 0; i < set->count; ++i)
        if (!strcmp(set->item[i].key, key))
            setting = &set->item[i];
    if (!setting)
        return SV_NOT_FOUND;
    if (by_admin && !setting->admin)
        return SV_DENIED;
    /* No text means the default. */
    if (sv_config_check(setting, text ? text : setting->def, canonical) != SV_OK)
        return SV_INVALID;
    strcpy(saved, setting->stored);
    strcpy(setting->stored, canonical);
    if (!write_file(set, id)) {
        strcpy(setting->stored, saved);
        return SV_IO;
    }
    *live = setting->live != 0;
    /* A secret never goes into an event or a log. */
    strcpy(old_text, setting->type == SV_SETTING_SECRET ? "" : setting->effective);
    strcpy(new_text, setting->type == SV_SETTING_SECRET ? "" : canonical);
    if (setting->live)
        strcpy(setting->effective, canonical);
    return SV_OK;
}

void sv_config_line(const char *id, const SvSetting *setting, char *line, size_t size)
{
    char range_low[40] = "", range_high[160] = "";
    char value[SV_CONFIG_TEXT_MAX + 1], def[SV_CONFIG_TEXT_MAX + 1];
    uint32_t i;
    switch (setting->type) {
    case SV_SETTING_FLAG:
        strcpy(range_low, "0");
        strcpy(range_high, "1");
        break;
    case SV_SETTING_INT:
        snprintf(range_low, sizeof range_low, "%d", setting->imin);
        snprintf(range_high, sizeof range_high, "%d", setting->imax);
        break;
    case SV_SETTING_FLOAT:
        snprintf(range_low, sizeof range_low, "%.9g", setting->fmin);
        snprintf(range_high, sizeof range_high, "%.9g", setting->fmax);
        break;
    case SV_SETTING_TEXT:
    case SV_SETTING_SECRET:
        strcpy(range_low, "0");
        snprintf(range_high, sizeof range_high, "%u", setting->text_max);
        break;
    case SV_SETTING_CHOICE:
        for (i = 0; i < setting->choice_count; ++i) {
            if (i)
                strcat(range_high, ",");
            strcat(range_high, setting->choices[i]);
        }
        break;
    }
    /* A secret shows only whether it has a value. */
    if (setting->type == SV_SETTING_SECRET) {
        strcpy(value, setting->stored[0] ? "*" : "");
        strcpy(def, "");
    } else {
        strcpy(value, setting->stored);
        strcpy(def, setting->def);
    }
    snprintf(line, size, "SETTING\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%u\t%u\t%u\t%s\t%s", id, setting->key, type_names[setting->type],
        value, def, range_low, range_high, setting->live, setting->admin, strcmp(setting->stored, setting->effective) ? 1u : 0u,
        setting->label, setting->help);
}
